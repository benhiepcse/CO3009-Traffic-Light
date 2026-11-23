#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dht20.h"

static uint32_t now_ms;
static unsigned writes;
static unsigned reads;
static bool tx_ok;
static bool rx_ok;
static uint8_t frame[7];

uint32_t dht20_test_now_ms(void)
{
    return now_ms;
}

bool dht20_test_write(uint16_t address, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    const uint8_t trigger[] = { 0xACU, 0x33U, 0x00U };
    assert(address == 0x70U && len == sizeof(trigger));
    assert(memcmp(data, trigger, sizeof(trigger)) == 0 && timeout_ms <= 3U);
    ++writes;
    return tx_ok;
}

bool dht20_test_read(uint16_t address, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    assert(address == 0x70U && len == sizeof(frame) && timeout_ms <= 3U);
    ++reads;
    if (rx_ok) {
        memcpy(data, frame, sizeof(frame));
    }
    return rx_ok;
}

static void poll_at(uint32_t time_ms)
{
    now_ms = time_ms;
    const unsigned before = reads + writes;
    dht20_poll();
    assert(reads + writes - before <= 1U);
}

static void setup(uint32_t time_ms)
{
    now_ms = time_ms;
    writes = 0U;
    reads = 0U;
    tx_ok = true;
    rx_ok = true;
    dht20_init();
}

static void expect_status(dht20_status_t expected)
{
    dht20_sample_t sample = { -999, 9999U };
    const unsigned before = reads + writes;
    assert(dht20_get_latest(&sample) == expected);
    assert(sample.temperature_x10 == -999 && sample.humidity_x10 == 9999U);
    assert(reads + writes == before);
}

static void measurement(uint32_t trigger_ms, const uint8_t data[7], int16_t temp, uint16_t rh)
{
    dht20_sample_t sample;
    memcpy(frame, data, sizeof(frame));
    poll_at(trigger_ms);
    poll_at(trigger_ms + 80U);
    assert(dht20_get_latest(&sample) == DHT20_OK);
    assert(sample.temperature_x10 == temp && sample.humidity_x10 == rh);
}

static void test_periodic_and_wraparound(void)
{
    /* Precomputed CRC bytes for factory-calibrated readings. */
    const uint8_t typical[] = { 0x18, 0x80, 0, 0x06, 0, 0, 0x23 };
    const uint8_t negative[] = { 0x18, 0x00, 0, 0x02, 0xFF, 0xAB, 0x2E };
    const uint8_t high[] = { 0x18, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x17 };
    setup(0U);
    expect_status(DHT20_NO_SAMPLE);
    assert(dht20_get_latest(NULL) == DHT20_ERROR);
    poll_at(99U);
    assert(writes == 0U);
    memcpy(frame, typical, sizeof(frame));
    poll_at(100U);
    poll_at(179U);
    assert(writes == 1U && reads == 0U);
    expect_status(DHT20_NO_SAMPLE);
    poll_at(180U);
    poll_at(2099U);
    assert(writes == 1U && reads == 1U);
    measurement(2100U, negative, -125, 0U);
    measurement(4100U, high, 1500, 1000U);
    assert(writes == 3U && reads == 3U);

    const uint32_t start_ms = UINT32_MAX - 110U;
    setup(start_ms);
    measurement(start_ms + 100U, typical, 250, 500U);
    assert(writes == 1U && reads == 1U);
}

static void test_faults_and_retry(void)
{
    const uint8_t valid[] = { 0x18, 0x80, 0, 0x06, 0, 0, 0x23 };
    setup(0U);
    tx_ok = false;
    poll_at(100U);
    expect_status(DHT20_ERROR);
    poll_at(2099U);
    assert(writes == 1U && reads == 0U);
    tx_ok = true;
    rx_ok = false;
    poll_at(2100U);
    poll_at(2180U);
    expect_status(DHT20_ERROR);
    rx_ok = true;

    frame[0] = 0x98U; /* Busy, so CRC and previous measurement bytes are ignored. */
    poll_at(4100U);
    poll_at(4180U);
    poll_at(4189U);
    assert(reads == 2U);
    poll_at(4190U);
    poll_at(4300U);
    expect_status(DHT20_ERROR);
    assert(reads == 3U); /* Deadline does not issue a late bus transaction. */

    measurement(6100U, valid, 250, 500U);
    memcpy(frame, valid, sizeof(frame));
    frame[6] ^= 1U;
    poll_at(8100U);
    poll_at(8180U);
    expect_status(DHT20_ERROR);
    memset(frame, 0, sizeof(frame));
    frame[6] = 0x6AU; /* Valid CRC but missing calibration and OTP status. */
    poll_at(10100U);
    poll_at(10180U);
    expect_status(DHT20_ERROR);
    measurement(12100U, valid, 250, 500U);
}

int main(void)
{
    test_periodic_and_wraparound();
    puts("PASS: DHT20 periodic timing, raw conversion and tick wrap");
    test_faults_and_retry();
    puts("PASS: DHT20 NACK, busy timeout, CRC, status and recovery");
    return 0;
}
