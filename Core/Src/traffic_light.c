#include "traffic_light.h"

#include <stdint.h>

#include "hardware_profile.h"

#ifndef TRAFFIC_LIGHT_HOST_TEST
#include "main.h"
#else
extern void traffic_test_write_raw(uint8_t road, uint8_t raw);
#endif

/* Four unique two-bit codes must use each of the four bit positions once. */
#define VALID_COLOR_MAP(off, red, amber, green) \
    ((off) <= 3U && (red) <= 3U && (amber) <= 3U && (green) <= 3U && \
     ((1U << (off)) | (1U << (red)) | (1U << (amber)) | (1U << (green))) == 0x0FU)

_Static_assert(VALID_COLOR_MAP(TRAFFIC_ROAD1_RAW_OFF, TRAFFIC_ROAD1_RAW_RED,
                               TRAFFIC_ROAD1_RAW_AMBER, TRAFFIC_ROAD1_RAW_GREEN),
               "Road 1 requires four unique two-bit raw codes");
_Static_assert(VALID_COLOR_MAP(TRAFFIC_ROAD2_RAW_OFF, TRAFFIC_ROAD2_RAW_RED,
                               TRAFFIC_ROAD2_RAW_AMBER, TRAFFIC_ROAD2_RAW_GREEN),
               "Road 2 requires four unique two-bit raw codes");

static traffic_color_t current_road1 = TRAFFIC_RED;
static traffic_color_t current_road2 = TRAFFIC_RED;

static bool color_is_valid(traffic_color_t color)
{
    return (unsigned int)color <= (unsigned int)TRAFFIC_GREEN;
}

static void write_road_raw(uint8_t road, uint8_t raw)
{
#ifdef TRAFFIC_LIGHT_HOST_TEST
    traffic_test_write_raw(road, raw);
#else
    GPIO_TypeDef *port;
    uint16_t pin_a;
    uint16_t pin_b;
    uint32_t set_mask = 0U;
    uint32_t reset_mask = 0U;

    if (road == 0U) {
        port = TL1_A_GPIO_Port;
        pin_a = TL1_A_Pin;
        pin_b = TL1_B_Pin;
    } else {
        port = TL2_A_GPIO_Port;
        pin_a = TL2_A_Pin;
        pin_b = TL2_B_Pin;
    }

    if ((raw & 0x02U) != 0U) {
        set_mask |= pin_a;
    } else {
        reset_mask |= pin_a;
    }

    if ((raw & 0x01U) != 0U) {
        set_mask |= pin_b;
    } else {
        reset_mask |= pin_b;
    }

    /* Both bits of one module change in one GPIO BSRR transaction. */
    port->BSRR = set_mask | (reset_mask << 16U);
#endif
}

static void write_color(uint8_t road, traffic_color_t color)
{
    static const uint8_t raw_by_road_and_color[][TRAFFIC_GREEN + 1U] = {
        { TRAFFIC_ROAD1_RAW_OFF, TRAFFIC_ROAD1_RAW_RED,
          TRAFFIC_ROAD1_RAW_AMBER, TRAFFIC_ROAD1_RAW_GREEN },
        { TRAFFIC_ROAD2_RAW_OFF, TRAFFIC_ROAD2_RAW_RED,
          TRAFFIC_ROAD2_RAW_AMBER, TRAFFIC_ROAD2_RAW_GREEN }
    };

    write_road_raw(road, raw_by_road_and_color[road][color]);
}

void traffic_init(void)
{
    write_color(0U, TRAFFIC_RED);
    write_color(1U, TRAFFIC_RED);
    current_road1 = TRAFFIC_RED;
    current_road2 = TRAFFIC_RED;
}

bool traffic_set_pair(traffic_color_t road1, traffic_color_t road2)
{
    bool road1_written = false;
    bool road2_written = false;

    if (!color_is_valid(road1) || !color_is_valid(road2)) {
        return false;
    }
    if ((road1 == TRAFFIC_GREEN) && (road2 == TRAFFIC_GREEN)) {
        return false;
    }

    /* Remove an existing green before a green can be enabled on the other road. */
    if ((current_road1 == TRAFFIC_GREEN) && (road1 != TRAFFIC_GREEN)) {
        write_color(0U, road1);
        road1_written = true;
    }
    if ((current_road2 == TRAFFIC_GREEN) && (road2 != TRAFFIC_GREEN)) {
        write_color(1U, road2);
        road2_written = true;
    }

    if (!road1_written && (road1 != current_road1)) {
        write_color(0U, road1);
    }
    if (!road2_written && (road2 != current_road2)) {
        write_color(1U, road2);
    }

    current_road1 = road1;
    current_road2 = road2;
    return true;
}
