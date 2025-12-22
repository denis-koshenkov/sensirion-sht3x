#include <stddef.h>
#include <stdbool.h>

#include "sht3x.h"
#include "sht3x_private.h"

/* Result of (315 / (2^16 - 1)). Part of the formula from the datasheet that converts raw temperature measurement to a
 * value in degrees Celsius. */
#define SHT3X_TEMPERATURE_CONVERSION_MAGIC 0.002670328831921f
/* Result of (100 / (2^16 - 1)). Part of the formula from the datasheet that converts raw humidity measurement to a
 * value in RH%. */
#define SHT3X_HUMIDITY_CONVERSION_MAGIC 0.001525902189669f

/* From the datasheet, rounded up */
#define SHT3X_MAX_MEASUREMENT_DURATION_HIGH_REPEATBILITY_MS 16

#define SHT3X_CMD_SINGLE_SHOT_MEAS_REPEATABILITY_HIGH_CLK_STRETCH_DIS {0x24, 0x00}

static bool is_valid_i2c_addr(uint8_t i2c_addr)
{
    return ((i2c_addr == 0x44) || (i2c_addr == 0x45));
}

static bool is_valid_cfg(const SHT3XInitConfig *const cfg)
{
    // clang-format off
    return (
        (cfg)
        && (cfg->get_instance_memory)
        && (cfg->i2c_write)
        && (cfg->i2c_read)
        && (cfg->start_timer)
        && is_valid_i2c_addr(cfg->i2c_addr)
    );
    // clang-format on
}

/**
 * @brief Convert two bytes in big endian to an integer of type uint16_t.
 *
 * @param bytes The two bytes at this address are used for conversion.
 *
 * @return uint16_t Resulting integer.
 */
static uint16_t two_big_endian_bytes_to_uint16(const uint8_t *const bytes)
{
    return (((uint16_t)(bytes[0])) << 8) | ((uint16_t)(bytes[1]));
}

/**
 * @brief Convert raw temperature measurement to temperature in celsius.
 *
 * @param raw_temp Should point to 2 bytes that are raw temperature measurement read out from the device.
 *
 * @return float Resulting temperature in Celsius.
 */
static float convert_raw_temp_meas_to_celsius(const uint8_t *const raw_temp)
{
    /* Device sends raw measurements in big endian, convert to a 16-bit value in target endianness */
    uint16_t raw_temp_val = two_big_endian_bytes_to_uint16(raw_temp);
    /* Based on conversion formula from the SHT3X datasheet, p. 14, section 4.13. */
    float temperature_celsius = (SHT3X_TEMPERATURE_CONVERSION_MAGIC * (float)raw_temp_val) - 45;
    return temperature_celsius;
}

/**
 * @brief Convert raw humidity measurement to humidity in RH%.
 *
 * @param raw_humidity Should point to 2 bytes that are raw humidity measurement read out from the device.
 *
 * @return float Resulting humidity in RH%.
 */
static float convert_raw_humidity_meas_to_rh(const uint8_t *const raw_humidity)
{
    /* Device sends raw measurements in big endian, convert to a 16-bit value in target endianness */
    uint16_t raw_humidity_val = two_big_endian_bytes_to_uint16(raw_humidity);
    /* Based on conversion formula from the SHT3X datasheet, p. 14, section 4.13. */
    float humidity_rh = SHT3X_HUMIDITY_CONVERSION_MAGIC * (float)raw_humidity_val;
    return humidity_rh;
}

static void read_single_shot_measurement_part_4(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }
    SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
    if (!cb) {
        return;
    }

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C read failed, execute meas complete cb to indicate failure */
        cb(SHT3X_RESULT_CODE_IO_ERR, NULL, self->sequence_cb_user_data);
        return;
    }

    /* i2c_read_buf now contains the raw measurements. Need to convert them to temperature in Celsius and humidity in
     * RH%. */
    SHT3XMeasurement meas;
    /* Temperature is the first two bytes in the received data. */
    meas.temperature = convert_raw_temp_meas_to_celsius(&(self->i2c_read_buf[0]));
    /* Bytes 3 and 4 in the received data form the raw humidity measurement. */
    meas.humidity = convert_raw_humidity_meas_to_rh(&(self->i2c_read_buf[3]));

    cb(SHT3X_RESULT_CODE_OK, &meas, self->sequence_cb_user_data);
}

static void read_single_shot_measurement_part_3(void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    self->i2c_read(self->i2c_read_buf, 6, self->i2c_addr, read_single_shot_measurement_part_4, (void *)self);
}

static void read_single_shot_measurement_part_2(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C write failed, execute meas complete cb to indicate failure */
        SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
        if (!cb) {
            return;
        }
        cb(SHT3X_RESULT_CODE_IO_ERR, NULL, self->sequence_cb_user_data);
        return;
    }

    self->start_timer(SHT3X_MAX_MEASUREMENT_DURATION_HIGH_REPEATBILITY_MS, read_single_shot_measurement_part_3,
                      (void *)self);
}

uint8_t sht3x_create(SHT3X *const instance, const SHT3XInitConfig *const cfg)
{
    if (!instance || !is_valid_cfg(cfg)) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    *instance = cfg->get_instance_memory(cfg->get_instance_memory_user_data);
    if (!(*instance)) {
        /* get_instance_memory returned NULL -> no memory for this instance */
        return SHT3X_RESULT_CODE_OUT_OF_MEMORY;
    }

    (*instance)->i2c_write = cfg->i2c_write;
    (*instance)->i2c_read = cfg->i2c_read;
    (*instance)->start_timer = cfg->start_timer;
    (*instance)->i2c_addr = cfg->i2c_addr;

    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                           SHT3XMeasCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    uint8_t data[] = SHT3X_CMD_SINGLE_SHOT_MEAS_REPEATABILITY_HIGH_CLK_STRETCH_DIS;
    /* Passing self as user data, so that we can invoke SHT3XMeasCompleteCb in read_single_shot_measurement_part_x */
    self->i2c_write(data, sizeof(data), self->i2c_addr, read_single_shot_measurement_part_2, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

void sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data)
{
    if (free_instance_memory) {
        free_instance_memory((void *)self, user_data);
    }
}
