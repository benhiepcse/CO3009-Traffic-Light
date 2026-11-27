#include "dht20.h"

#include <stdbool.h>
#include <stddef.h>

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

#define DHT20_I2C_ADDRESS           (0x38U << 1)
#define DHT20_POWER_UP_MS           100U
#define DHT20_CONVERSION_MS          80U
#define DHT20_MEASUREMENT_LIMIT_MS  150U
#define DHT20_SAMPLE_INTERVAL_MS   2000U
#define DHT20_I2C_TIMEOUT_MS           5U

#define DHT20_FRAME_SIZE               7U
#define DHT20_STATUS_BUSY           0x80U
#define DHT20_RAW_SCALE        1048576UL

static dht20_sample_t latest_sample;
static dht20_status_t latest_status;
static bool measurement_pending;
static uint32_t last_trigger_ms;
static uint32_t conversion_start_ms;
static uint32_t trigger_interval_ms;

static uint8_t calculate_crc(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFFU;

    for (size_t index = 0U; index < length; ++index) {
        crc ^= data[index];

        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const bool top_bit_set = (crc & 0x80U) != 0U;
            crc = (uint8_t)(crc << 1U);

            if (top_bit_set) {
                crc ^= 0x31U;
            }
        }
    }

    return crc;
}

static void start_measurement(uint32_t now)
{
    uint8_t command[] = { 0xACU, 0x33U, 0x00U };

    last_trigger_ms = now;
    trigger_interval_ms = DHT20_SAMPLE_INTERVAL_MS;

    if (HAL_I2C_Master_Transmit(&hi2c1, DHT20_I2C_ADDRESS, command,
                                sizeof command, DHT20_I2C_TIMEOUT_MS) != HAL_OK) {
        latest_status = DHT20_ERROR;
        return;
    }

    conversion_start_ms = HAL_GetTick();
    measurement_pending = true;
}

static void read_measurement(uint32_t elapsed_ms)
{
    uint8_t frame[DHT20_FRAME_SIZE];

    if (HAL_I2C_Master_Receive(&hi2c1, DHT20_I2C_ADDRESS, frame,
                               sizeof frame, DHT20_I2C_TIMEOUT_MS) != HAL_OK) {
        measurement_pending = false;
        latest_status = DHT20_ERROR;
        return;
    }

    if ((frame[0] & DHT20_STATUS_BUSY) != 0U) {
        if (elapsed_ms >= DHT20_MEASUREMENT_LIMIT_MS) {
            measurement_pending = false;
            latest_status = DHT20_ERROR;
        }
        return;
    }

    measurement_pending = false;

    if (calculate_crc(frame, DHT20_FRAME_SIZE - 1U)
        != frame[DHT20_FRAME_SIZE - 1U]) {
        latest_status = DHT20_ERROR;
        return;
    }

    const uint32_t raw_humidity = ((uint32_t)frame[1] << 12U)
                                | ((uint32_t)frame[2] << 4U)
                                | ((uint32_t)frame[3] >> 4U);

    const uint32_t raw_temperature = ((uint32_t)(frame[3] & 0x0FU) << 16U)
                                   | ((uint32_t)frame[4] << 8U)
                                   | (uint32_t)frame[5];

    const uint32_t half_scale = DHT20_RAW_SCALE / 2U;

    latest_sample.humidity_x10 = (uint16_t)(
        (raw_humidity * 1000U + half_scale) / DHT20_RAW_SCALE
    );

    const int32_t temperature_x10 = (int32_t)(
        (raw_temperature * 2000U + half_scale) / DHT20_RAW_SCALE
    ) - 500;

    latest_sample.temperature_x10 = (int16_t)temperature_x10;
    latest_status = DHT20_OK;
}

void dht20_init(void)
{
    latest_sample = (dht20_sample_t){0};
    latest_status = DHT20_NO_SAMPLE;
    measurement_pending = false;
    last_trigger_ms = HAL_GetTick();
    conversion_start_ms = 0U;
    trigger_interval_ms = DHT20_POWER_UP_MS;
}

void dht20_poll(void)
{
    const uint32_t now = HAL_GetTick();

    if (measurement_pending) {
        const uint32_t elapsed_ms = now - conversion_start_ms;

        if (elapsed_ms >= DHT20_CONVERSION_MS) {
            read_measurement(elapsed_ms);
        }
        return;
    }

    if ((now - last_trigger_ms) >= trigger_interval_ms) {
        start_measurement(now);
    }
}

dht20_status_t dht20_get_latest(dht20_sample_t *out)
{
    if (out == NULL) {
        return DHT20_ERROR;
    }

    if (latest_status == DHT20_OK) {
        *out = latest_sample;
    }

    return latest_status;
}
