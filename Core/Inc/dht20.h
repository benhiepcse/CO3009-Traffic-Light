#ifndef DHT20_H
#define DHT20_H

#include <stdint.h>

typedef enum {
    DHT20_NO_SAMPLE = 0,
    DHT20_OK,
    DHT20_ERROR
} dht20_status_t;

typedef struct {
    int16_t temperature_x10;
    uint16_t humidity_x10;
} dht20_sample_t;

void dht20_init(void);
void dht20_poll(void);
dht20_status_t dht20_get_latest(dht20_sample_t *out);

#endif
