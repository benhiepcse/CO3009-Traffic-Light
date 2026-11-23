#include "dht20.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef DHT20_HOST_TEST
extern uint32_t dht20_test_now_ms(void);
extern bool dht20_test_write(uint16_t address, const uint8_t *data, uint16_t len,
                             uint32_t timeout_ms);
extern bool dht20_test_read(uint16_t address, uint8_t *data, uint16_t len,
                            uint32_t timeout_ms);
#else
#include "main.h"
extern I2C_HandleTypeDef hi2c1;
#endif

#define I2C_ADDRESS         (DHT20_ADDRESS_7BIT << 1U)
#define I2C_TIMEOUT_MS      3U
#define POWER_UP_MS       100U
#define CONVERSION_MS      80U
#define DEADLINE_MS       200U
#define STATUS_BUSY        0x80U
#define STATUS_VALID       0x18U
#define RAW_SCALE          (1UL << 20U)
#define FRAME_SIZE         7U

static enum { UNINITIALIZED, WAIT_TRIGGER, WAIT_RESULT } phase;
static uint32_t cycle_start_ms;
static uint32_t trigger_interval_ms;
static uint32_t last_read_ms;
static dht20_status_t status;
static dht20_sample_t latest;

static uint32_t now_ms(void)
{
#ifdef DHT20_HOST_TEST
    return dht20_test_now_ms();
#else
    return HAL_GetTick();
#endif
}

static bool trigger_measurement(void)
{
    uint8_t command[] = { 0xACU, 0x33U, 0x00U };
#ifdef DHT20_HOST_TEST
    return dht20_test_write(I2C_ADDRESS, command, sizeof(command), I2C_TIMEOUT_MS);
#else
    /* Avoid the F1 HAL's longer internal BUSY wait on this single-master bus. */
    return !__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) &&
           HAL_I2C_Master_Transmit(&hi2c1, I2C_ADDRESS, command, sizeof(command),
                                   I2C_TIMEOUT_MS) == HAL_OK;
#endif
}

static bool read_frame(uint8_t frame[FRAME_SIZE])
{
#ifdef DHT20_HOST_TEST
    return dht20_test_read(I2C_ADDRESS, frame, FRAME_SIZE, I2C_TIMEOUT_MS);
#else
    return !__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) &&
           HAL_I2C_Master_Receive(&hi2c1, I2C_ADDRESS, frame, FRAME_SIZE,
                                  I2C_TIMEOUT_MS) == HAL_OK;
#endif
}

static uint8_t frame_crc(const uint8_t frame[FRAME_SIZE])
{
    uint8_t crc = 0xFFU;
    for (uint8_t i = 0U; i < FRAME_SIZE - 1U; ++i) {
        crc ^= frame[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint8_t polynomial = (crc & 0x80U) != 0U ? 0x31U : 0U;
            crc = (uint8_t)((crc << 1U) ^ polynomial);
        }
    }
    return crc;
}

static void finish(dht20_status_t result)
{
    status = result;
    phase = WAIT_TRIGGER;
}

void dht20_init(void)
{
    phase = WAIT_TRIGGER;
    status = DHT20_NO_SAMPLE;
    cycle_start_ms = now_ms();
    trigger_interval_ms = POWER_UP_MS;
}

void dht20_poll(void)
{
    if (phase == UNINITIALIZED) {
        return;
    }
    const uint32_t now = now_ms();
    const uint32_t elapsed_ms = now - cycle_start_ms;
    if (phase == WAIT_TRIGGER) {
        if (elapsed_ms >= trigger_interval_ms) {
            const bool triggered = trigger_measurement();
            cycle_start_ms = now_ms();
            trigger_interval_ms = DHT20_PERIOD_MS;
            last_read_ms = cycle_start_ms;
            if (triggered) {
                phase = WAIT_RESULT;
            } else {
                finish(DHT20_ERROR);
            }
        }
        return;
    }

    if (elapsed_ms >= DEADLINE_MS) {
        finish(DHT20_ERROR);
        return;
    }
    if (elapsed_ms < CONVERSION_MS || now - last_read_ms < DHT20_POLL_MS) {
        return;
    }
    last_read_ms = now;

    uint8_t frame[FRAME_SIZE];
    if (!read_frame(frame)) {
        finish(DHT20_ERROR);
        return;
    }
    if ((frame[0] & STATUS_BUSY) != 0U) {
        return;
    }
    if ((frame[0] & STATUS_VALID) != STATUS_VALID || frame_crc(frame) != frame[6]) {
        finish(DHT20_ERROR);
        return;
    }

    const uint32_t humidity = ((uint32_t)frame[1] << 12U) |
                              ((uint32_t)frame[2] << 4U) | (frame[3] >> 4U);
    const uint32_t temperature = ((uint32_t)(frame[3] & 0x0FU) << 16U) |
                                 ((uint32_t)frame[4] << 8U) | frame[5];
    latest.humidity_x10 = (uint16_t)((humidity * 1000U + RAW_SCALE / 2U) / RAW_SCALE);
    latest.temperature_x10 = (int16_t)((int32_t)((temperature * 2000U + RAW_SCALE / 2U) /
                                                 RAW_SCALE) - 500);
    finish(DHT20_OK);
}

dht20_status_t dht20_get_latest(dht20_sample_t *out)
{
    if (out == NULL) {
        return DHT20_ERROR;
    }
    if (status == DHT20_OK) {
        *out = latest;
    }
    return status;
}
