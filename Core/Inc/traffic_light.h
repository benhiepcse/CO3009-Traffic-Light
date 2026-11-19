#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include <stdbool.h>

typedef enum {
    TRAFFIC_OFF = 0,
    TRAFFIC_RED,
    TRAFFIC_AMBER,
    TRAFFIC_GREEN
} traffic_color_t;

/* Call after GPIO setup and before traffic_set_pair(), from the main loop. */
void traffic_init(void);

/*
 * Changes the requested colors, removing the old green first on a handover.
 * Call from the main loop, as with traffic_init().
 * Returns false without changing GPIO when a color is invalid or both roads
 * request green at the same time.
 */
bool traffic_set_pair(traffic_color_t road1, traffic_color_t road2);

#endif
