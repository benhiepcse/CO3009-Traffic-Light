#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

/*
 * Provisional hardware profile.
 *
 * CO3009-04 must replace these values after measuring the real modules.
 * Keeping the values here makes that board-dependent work explicit and keeps
 * the traffic-light and button algorithms independent of wiring polarity.
 */
#define TRAFFIC_ROAD1_RAW_OFF    0x00U
#define TRAFFIC_ROAD1_RAW_RED    0x03U
#define TRAFFIC_ROAD1_RAW_AMBER  0x01U
#define TRAFFIC_ROAD1_RAW_GREEN  0x02U

#define TRAFFIC_ROAD2_RAW_OFF    0x00U
#define TRAFFIC_ROAD2_RAW_RED    0x03U
#define TRAFFIC_ROAD2_RAW_AMBER  0x01U
#define TRAFFIC_ROAD2_RAW_GREEN  0x02U

#define BUTTON_D7_ACTIVE_LEVEL    1U
#define BUTTON_D8_ACTIVE_LEVEL    1U
#define BUTTON_D9_ACTIVE_LEVEL    1U
#define BUTTON_D10_ACTIVE_LEVEL   1U
#define BUTTON_USER_ACTIVE_LEVEL  0U

#if (BUTTON_D7_ACTIVE_LEVEL > 1U) || (BUTTON_D8_ACTIVE_LEVEL > 1U) || \
    (BUTTON_D9_ACTIVE_LEVEL > 1U) || (BUTTON_D10_ACTIVE_LEVEL > 1U) || \
    (BUTTON_USER_ACTIVE_LEVEL > 1U)
#error Button active levels must be either 0 or 1
#endif

#endif
