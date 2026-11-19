#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "button.h"
#include "hardware_profile.h"

static uint8_t levels[BUTTON_COUNT];

uint8_t button_test_read_level(button_id_t id)
{
    return levels[id];
}

static void fail(const char *message)
{
    fprintf(stderr, "button_test: %s\n", message);
    exit(EXIT_FAILURE);
}

static void expect(bool condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

static uint8_t active_level(button_id_t id)
{
    static const uint8_t active[BUTTON_COUNT] = {
        BUTTON_D7_ACTIVE_LEVEL,
        BUTTON_D8_ACTIVE_LEVEL,
        BUTTON_D9_ACTIVE_LEVEL,
        BUTTON_D10_ACTIVE_LEVEL,
        BUTTON_USER_ACTIVE_LEVEL
    };
    return active[id];
}

static void set_pressed(button_id_t id, bool pressed)
{
    levels[id] = pressed ? active_level(id) : (uint8_t)!active_level(id);
}

static void poll_times(uint32_t count)
{
    while (count > 0U) {
        button_poll();
        count--;
    }
}

static void reset_fixture(void)
{
    button_id_t id;

    for (id = BUTTON_D7; id < BUTTON_COUNT; id++) {
        set_pressed(id, false);
    }
    button_init();
}

static void expect_event(button_id_t id, button_event_type_t type, const char *message)
{
    button_event_t event;

    expect(button_take_event(&event), message);
    expect(event.id == id && event.type == type, message);
}

static void test_debounce_and_repress(void)
{
    button_event_t event;

    reset_fixture();
    set_pressed(BUTTON_D7, true);
    poll_times(BUTTON_DEBOUNCE_TICKS - 1U);
    expect(!button_take_event(&event), "press accepted before debounce completed");
    poll_times(1U);
    expect_event(BUTTON_D7, BUTTON_EVENT_PRESS, "missing debounced press");

    poll_times(10U);
    expect(!button_take_event(&event), "held button repeated PRESS");

    set_pressed(BUTTON_D7, false);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    set_pressed(BUTTON_D7, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect_event(BUTTON_D7, BUTTON_EVENT_PRESS, "release and repress was not detected");
}

static void test_bounce_is_ignored(void)
{
    button_event_t event;

    reset_fixture();
    set_pressed(BUTTON_D8, true);
    poll_times(1U);
    set_pressed(BUTTON_D8, false);
    poll_times(1U);
    set_pressed(BUTTON_D8, true);
    poll_times(BUTTON_DEBOUNCE_TICKS - 1U);
    expect(!button_take_event(&event), "bouncing input generated an event");
    poll_times(1U);
    expect_event(BUTTON_D8, BUTTON_EVENT_PRESS, "stable press after bounce was lost");
}

static void test_single_long_press(void)
{
    button_event_t event;

    reset_fixture();
    set_pressed(BUTTON_USER, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect_event(BUTTON_USER, BUTTON_EVENT_PRESS, "missing USER press");

    poll_times(BUTTON_LONG_PRESS_TICKS - 1U);
    expect(!button_take_event(&event), "LONG_PRESS generated too early");
    poll_times(1U);
    expect_event(BUTTON_USER, BUTTON_EVENT_LONG_PRESS, "missing LONG_PRESS");
    poll_times(BUTTON_LONG_PRESS_TICKS);
    expect(!button_take_event(&event), "held button repeated LONG_PRESS");
}

static void test_fifo_and_clear(void)
{
    button_event_t event;

    reset_fixture();
    set_pressed(BUTTON_D9, true);
    set_pressed(BUTTON_D10, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect_event(BUTTON_D9, BUTTON_EVENT_PRESS, "FIFO first event is wrong");
    expect_event(BUTTON_D10, BUTTON_EVENT_PRESS, "FIFO second event is wrong");

    set_pressed(BUTTON_D7, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    button_clear_events();
    expect(!button_take_event(&event), "button_clear_events did not empty the queue");
    poll_times(10U);
    expect(!button_take_event(&event), "button_clear_events reset held-button state");
    expect(!button_take_event(NULL), "NULL output must be rejected");
}

static void tap(button_id_t id)
{
    set_pressed(id, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    set_pressed(id, false);
    poll_times(BUTTON_DEBOUNCE_TICKS);
}

static void test_release_at_hold_boundary(void)
{
    button_event_t event;

    reset_fixture();
    set_pressed(BUTTON_D8, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect_event(BUTTON_D8, BUTTON_EVENT_PRESS, "missing initial press");
    poll_times(BUTTON_LONG_PRESS_TICKS - 1U);
    set_pressed(BUTTON_D8, false);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect(!button_take_event(&event), "release at the threshold produced LONG_PRESS");

    set_pressed(BUTTON_D8, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect_event(BUTTON_D8, BUTTON_EVENT_PRESS, "stable release did not rearm PRESS");
    poll_times(BUTTON_LONG_PRESS_TICKS - 1U);
    set_pressed(BUTTON_D8, false);
    poll_times(1U);
    expect(!button_take_event(&event), "release bounce advanced the hold counter");
    set_pressed(BUTTON_D8, true);
    poll_times(1U);
    expect_event(BUTTON_D8, BUTTON_EVENT_LONG_PRESS, "short release bounce lost the hold");
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect(!button_take_event(&event), "short release bounce rearmed PRESS");
}

static void test_held_at_init(void)
{
    button_event_t event;

    reset_fixture();
    set_pressed(BUTTON_USER, true);
    button_init();
    poll_times(BUTTON_DEBOUNCE_TICKS - 1U);
    expect(!button_take_event(&event), "held input at init bypassed debounce");
    poll_times(1U);
    expect_event(BUTTON_USER, BUTTON_EVENT_PRESS, "held input at init was lost");
    poll_times(BUTTON_LONG_PRESS_TICKS);
    expect_event(BUTTON_USER, BUTTON_EVENT_LONG_PRESS, "held input at init lost its long press");
}

static void test_queue_overflow_and_wrap(void)
{
    button_event_t event = { BUTTON_USER, BUTTON_EVENT_LONG_PRESS };

    reset_fixture();
    for (unsigned int i = 0U; i < 7U; i++) {
        tap(BUTTON_D7);
    }
    set_pressed(BUTTON_D8, true);
    set_pressed(BUTTON_D10, true);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    expect(!button_take_event(NULL), "NULL output accepted with events queued");
    expect_event(BUTTON_D7, BUTTON_EVENT_PRESS, "overflow displaced the oldest event");
    expect_event(BUTTON_D7, BUTTON_EVENT_PRESS, "overflow displaced the next event");

    set_pressed(BUTTON_D8, false);
    set_pressed(BUTTON_D10, false);
    poll_times(BUTTON_DEBOUNCE_TICKS);
    tap(BUTTON_USER);
    tap(BUTTON_D9);
    for (unsigned int i = 0U; i < 5U; i++) {
        expect_event(BUTTON_D7, BUTTON_EVENT_PRESS, "overflow corrupted an existing event");
    }
    expect_event(BUTTON_D8, BUTTON_EVENT_PRESS, "last free slot did not keep the earlier event");
    expect_event(BUTTON_USER, BUTTON_EVENT_PRESS, "queue did not wrap in FIFO order");
    expect_event(BUTTON_D9, BUTTON_EVENT_PRESS, "queue lost an event after wrap");
    expect(!button_take_event(&event), "queue retained the event dropped on overflow");
    expect(event.id == BUTTON_USER && event.type == BUTTON_EVENT_LONG_PRESS,
           "an empty read changed the caller's output");
}

int main(void)
{
    test_debounce_and_repress();
    test_bounce_is_ignored();
    test_single_long_press();
    test_release_at_hold_boundary();
    test_held_at_init();
    puts("PASS: button debounce, hold boundaries, release/repress, and held-at-init");
    test_fifo_and_clear();
    test_queue_overflow_and_wrap();
    puts("PASS: simultaneous inputs, FIFO, clear, null reads, overflow, and wrap");
    return EXIT_SUCCESS;
}
