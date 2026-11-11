#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include <stdbool.h>

typedef enum {
    TRAFFIC_OFF = 0,
    TRAFFIC_RED,
    TRAFFIC_AMBER,
    TRAFFIC_GREEN
} traffic_color_t;

void traffic_init(void);
bool traffic_set_pair(traffic_color_t road1, traffic_color_t road2);

#endif
