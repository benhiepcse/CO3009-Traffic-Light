#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware_profile.h"
#include "traffic_light.h"

typedef struct {
    traffic_color_t road1;
    traffic_color_t road2;
} light_pair_t;

static const uint8_t expected_raw[][TRAFFIC_GREEN + 1U] = {
    { TRAFFIC_ROAD1_RAW_OFF, TRAFFIC_ROAD1_RAW_RED,
      TRAFFIC_ROAD1_RAW_AMBER, TRAFFIC_ROAD1_RAW_GREEN },
    { TRAFFIC_ROAD2_RAW_OFF, TRAFFIC_ROAD2_RAW_RED,
      TRAFFIC_ROAD2_RAW_AMBER, TRAFFIC_ROAD2_RAW_GREEN }
};

static uint8_t output[2];
static uint8_t write_order[2];
static size_t write_count;

static void expect(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "traffic_light_test: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

void traffic_test_write_raw(uint8_t road, uint8_t raw)
{
    expect(road < 2U && raw < 4U, "invalid GPIO output");
    expect(write_count < 2U, "more than two GPIO writes in one change");
    write_order[write_count++] = road;
    output[road] = raw;
    expect(output[0] != TRAFFIC_ROAD1_RAW_GREEN ||
           output[1] != TRAFFIC_ROAD2_RAW_GREEN, "transient dual green");
}

static void expect_pair(light_pair_t pair)
{
    expect(output[0] == expected_raw[0][pair.road1], "wrong road 1 raw code");
    expect(output[1] == expected_raw[1][pair.road2], "wrong road 2 raw code");
}

static void reset_fixture(void)
{
    output[0] = TRAFFIC_ROAD1_RAW_OFF;
    output[1] = TRAFFIC_ROAD2_RAW_OFF;
    write_count = 0U;
    traffic_init();
    expect(write_count == 2U, "init must write both roads");
    expect_pair((light_pair_t){ TRAFFIC_RED, TRAFFIC_RED });
}

static bool dual_green(light_pair_t pair)
{
    return pair.road1 == TRAFFIC_GREEN && pair.road2 == TRAFFIC_GREEN;
}

static void test_transition(light_pair_t from, light_pair_t to)
{
    reset_fixture();
    write_count = 0U;
    expect(traffic_set_pair(from.road1, from.road2), "valid starting pair rejected");
    expect_pair(from);
    write_count = 0U;

    expect(traffic_set_pair(to.road1, to.road2), "valid destination pair rejected");
    expect_pair(to);
    expect(write_count == (size_t)(from.road1 != to.road1) +
                          (size_t)(from.road2 != to.road2),
           "each changed road must be written exactly once");

    if (from.road1 == TRAFFIC_GREEN && to.road2 == TRAFFIC_GREEN) {
        expect(write_order[0] == 0U && write_order[1] == 1U,
               "road 1 green must be removed before enabling road 2 green");
    }
    if (from.road2 == TRAFFIC_GREEN && to.road1 == TRAFFIC_GREEN) {
        expect(write_order[0] == 1U && write_order[1] == 0U,
               "road 2 green must be removed before enabling road 1 green");
    }
}

static void test_all_transitions(void)
{
    light_pair_t valid_pairs[15];
    size_t count = 0U;

    for (traffic_color_t road1 = TRAFFIC_OFF; road1 <= TRAFFIC_GREEN; road1++) {
        for (traffic_color_t road2 = TRAFFIC_OFF; road2 <= TRAFFIC_GREEN; road2++) {
            light_pair_t pair = { road1, road2 };
            if (!dual_green(pair)) {
                expect(count < sizeof(valid_pairs) / sizeof(valid_pairs[0]),
                       "too many valid pairs");
                valid_pairs[count++] = pair;
            }
        }
    }
    expect(count == 15U, "expected 15 valid pairs");
    for (size_t from = 0U; from < count; from++) {
        for (size_t to = 0U; to < count; to++) {
            test_transition(valid_pairs[from], valid_pairs[to]);
        }
    }
}

static void test_rejected_pairs_preserve_outputs(void)
{
    const light_pair_t start = { TRAFFIC_GREEN, TRAFFIC_RED };

    reset_fixture();
    write_count = 0U;
    expect(traffic_set_pair(start.road1, start.road2), "setup pair rejected");
    write_count = 0U;

    expect(!traffic_set_pair(TRAFFIC_GREEN, TRAFFIC_GREEN), "dual green accepted");
    expect(!traffic_set_pair((traffic_color_t)99, TRAFFIC_RED), "invalid road 1 accepted");
    expect(!traffic_set_pair(TRAFFIC_RED, (traffic_color_t)-1), "invalid road 2 accepted");
    expect(write_count == 0U, "rejected request caused GPIO writes");
    expect_pair(start);
    expect(traffic_set_pair(start.road1, start.road2), "unchanged pair rejected");
    expect(write_count == 0U, "rejected request changed cached state");
}

int main(void)
{
    test_all_transitions();
    puts("PASS: all 225 valid traffic transitions including both green handovers");
    test_rejected_pairs_preserve_outputs();
    puts("PASS: invalid and dual-green requests preserve outputs and state");
    return EXIT_SUCCESS;
}
