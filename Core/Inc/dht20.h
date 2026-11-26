#ifndef DHT20_H
#define DHT20_H

#include <stdint.h>

#define DHT20_ADDRESS_7BIT 0x38U
#define DHT20_POLL_MS      10U
#define DHT20_PERIOD_MS    2000U

typedef enum {
    DHT20_NO_SAMPLE = 0,
    DHT20_OK,
    DHT20_ERROR
} dht20_status_t;

typedef struct {
    int16_t temperature_x10;
    uint16_t humidity_x10;
} dht20_sample_t;

/* Call from the main context after I2C1 initialization. */
void dht20_init(void);
/* Advances at most one I2C transaction. Call about every DHT20_POLL_MS. */
void dht20_poll(void);
/* Copies the cached sample only when returning DHT20_OK. */
dht20_status_t dht20_get_latest(dht20_sample_t *out);

#endif
