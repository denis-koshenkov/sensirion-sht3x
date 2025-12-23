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

/* From the datasheet - there must be at least 1 ms delay between two I2C commands received by the sensor. */
#define SHT3X_MIN_DELAY_BETWEEN_TWO_I2C_CMDS_MS 1

/* From the datasheet, rounded up */
#define SHT3X_MAX_MEASUREMENT_DURATION_HIGH_REPEATBILITY_MS 16
#define SHT3X_MAX_MEASUREMENT_DURATION_MEDIUM_REPEATBILITY_MS 7
#define SHT3X_MAX_MEASUREMENT_DURATION_LOW_REPEATBILITY_MS 5

/* Single shot measurement command codes */
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS 0x24
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS_REPEATABILITY_HIGH 0x00
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS_REPEATABILITY_MEDIUM 0x0B
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS_REPEATABILITY_LOW 0x16
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN 0x2C
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN_REPEATABILITY_HIGH 0x06
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN_REPEATABILITY_MEDIUM 0x0D
#define SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN_REPEATABILITY_LOW 0x10

typedef enum {
    SHT3X_SEQUENCE_TYPE_SINGLE_SHOT_MEAS,
    SHT3X_SEQUENCE_TYPE_READ_MEAS,
} SHT3xSequenceType;

/**
 * @brief Check whether SHT3X I2C address is valid.
 *
 * @param[in] i2c_addr I2C address.
 *
 * @retval true I2C address is valid.
 * @retval false I2C address is invalid.
 */
static bool is_valid_i2c_addr(uint8_t i2c_addr)
{
    return ((i2c_addr == 0x44) || (i2c_addr == 0x45));
}

/**
 * @brief Check whether initialization config is valid.
 *
 * @param[in] cfg Initialization config.
 *
 * @retval true Config is valid.
 * @retval false Config is invalid.
 */
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
 * @brief Check whether repeatability option is valid.
 *
 * @param[in] repeatability Repeatability option.
 *
 * @retval true Valid option.
 * @retval false Invalid option.
 */
static bool is_valid_repeatability(uint8_t repeatability)
{
    // clang-format off
    return (
        (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH)
        || (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM)
        || (repeatability == SHT3X_MEAS_REPEATABILITY_LOW)
    );
    // clang-format on
}

/**
 * @brief Check whether clock stretching option is valid.
 *
 * @param[in] clock_stretching Clock stretching option.
 *
 * @retval true Valid option.
 * @retval false Invalid option.
 */
static bool is_valid_clock_stretching(uint8_t clock_stretching)
{
    // clang-format off
    return (
        (clock_stretching == SHT3X_CLOCK_STRETCHING_ENABLED)
        || (clock_stretching == SHT3X_CLOCK_STRETCHING_DISABLED)
    );
    // clang-format on
}

/**
 * @brief Convert two bytes in big endian to an integer of type uint16_t.
 *
 * @param[in] bytes The two bytes at this address are used for conversion.
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
 * @param[in] raw_temp Should point to 2 bytes that are raw temperature measurement read out from the device.
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
 * @param[in] raw_humidity Should point to 2 bytes that are raw humidity measurement read out from the device.
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

/**
 * @brief Get the number of ms to wait between sending the single shot measurement command and the subsequent read
 * command.
 *
 * @param[in] repeatability Repeatability option for this single shot measurement sequence.
 * @param[in] clock_stretching Clock stretching option for this single shot measurement sequence.
 * @param[out] period If SHT3X_RESULT_CODE_OK is returned, the period is written to this parameter.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully computed the period.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p period is NULL, @p repeatability option is invalid, or @p clock_stretching
 * option is invalid.
 */
static uint8_t get_single_shot_meas_timer_period(uint8_t repeatability, uint8_t clock_stretching,
                                                 uint32_t *const period)
{
    if (!period) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    if (clock_stretching == SHT3X_CLOCK_STRETCHING_ENABLED) {
        /* When clock stretching is enabled, we do not have to wait for measurements to become available before issuing
         * a read command. We send the read command as soon as mandatory delay elapses, and the sensor holds the SCL
         * line low until the measurement is ready (clock stretching). When the measurement is ready, the sensor sends
         * the measurement data. */
        *period = SHT3X_MIN_DELAY_BETWEEN_TWO_I2C_CMDS_MS;
    } else if (clock_stretching == SHT3X_CLOCK_STRETCHING_DISABLED) {
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            *period = SHT3X_MAX_MEASUREMENT_DURATION_HIGH_REPEATBILITY_MS;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            *period = SHT3X_MAX_MEASUREMENT_DURATION_MEDIUM_REPEATBILITY_MS;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            *period = SHT3X_MAX_MEASUREMENT_DURATION_LOW_REPEATBILITY_MS;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else {
        /* Invalid clock stretching option */
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    return SHT3X_RESULT_CODE_OK;
}

static void generic_i2c_complete_cb(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    SHT3XCompleteCb cb = (SHT3XCompleteCb)self->sequence_cb;
    if (cb) {
        uint8_t rc = (result_code == SHT3X_I2C_RESULT_CODE_OK) ? SHT3X_RESULT_CODE_OK : SHT3X_RESULT_CODE_IO_ERR;
        cb(rc, self->sequence_cb_user_data);
    }
}

static void meas_i2c_complete_cb(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }
    SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
    if (!cb) {
        return;
    }

    /* Address NACK is not considered an error as a part of read measurement sequence. It is a valid scenario if the
     * measurements are not available. To let the caller distinguish between this scenario and a generic IO error,
     * return a different code when address NACK occurred as a part of read measurement sequence. */
    bool return_no_data_if_address_nack = (self->sequence_type == SHT3X_SEQUENCE_TYPE_READ_MEAS);

    if (result_code == SHT3X_I2C_RESULT_CODE_ADDRESS_NACK && return_no_data_if_address_nack) {
        cb(SHT3X_RESULT_CODE_NO_DATA, NULL, self->sequence_cb_user_data);
    } else if (result_code == SHT3X_I2C_RESULT_CODE_OK) {
        /* i2c_read_buf now contains the raw measurements. Need to convert them to temperature in Celsius and
         * humidity in RH%. */
        SHT3XMeasurement meas;
        /* Temperature is the first two bytes in the received data. */
        meas.temperature = convert_raw_temp_meas_to_celsius(&(self->i2c_read_buf[0]));
        /* Bytes 3 and 4 in the received data form the raw humidity measurement. */
        meas.humidity = convert_raw_humidity_meas_to_rh(&(self->i2c_read_buf[3]));

        cb(SHT3X_RESULT_CODE_OK, &meas, self->sequence_cb_user_data);
    } else {
        cb(SHT3X_RESULT_CODE_IO_ERR, NULL, self->sequence_cb_user_data);
    }
}

static void read_single_shot_measurement_part_3(void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    self->i2c_read(self->i2c_read_buf, 6, self->i2c_addr, meas_i2c_complete_cb, (void *)self);
}

static void read_single_shot_measurement_part_2(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }
    SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C write failed, execute meas complete cb to indicate failure */
        if (cb) {
            cb(SHT3X_RESULT_CODE_IO_ERR, NULL, self->sequence_cb_user_data);
        }
        return;
    }

    uint32_t timer_period = 0;
    uint8_t rc = get_single_shot_meas_timer_period(self->repeatability, self->clock_stretching, &timer_period);
    if (rc != SHT3X_RESULT_CODE_OK) {
        /* We should never end up here, because we verify repeatability and clock stretching options before starting the
         * sequence. */
        if (cb) {
            cb(SHT3X_RESULT_CODE_DRIVER_ERR, NULL, self->sequence_cb_user_data);
        }
        return;
    }

    self->start_timer(timer_period, read_single_shot_measurement_part_3, (void *)self);
}

/**
 * @brief Write single shot measurement command code to @p cmd.
 *
 * @param[in] repeatability Repeatability option, use @ref SHT3XMeasRepeatability.
 * @param[in] clock_stretching Clock stretching option, use @ref SHT3XClockStretching.
 * @param[out] cmd Must be a uint8_t array of size 2. Command code is written here, if SHT3X_RESULT_CODE_OK is returned.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully generated command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p cmd is NULL, @p repeatability option is invalid, or @p clock_stretching
 * option is invalid.
 */
static uint8_t get_single_shot_meas_command_code(uint8_t repeatability, uint8_t clock_stretching, uint8_t *const cmd)
{
    if (!cmd) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    if (clock_stretching == SHT3X_CLOCK_STRETCHING_DISABLED) {
        cmd[0] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_DIS_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else if (clock_stretching == SHT3X_CLOCK_STRETCHING_ENABLED) {
        cmd[0] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_SINGLE_SHOT_MEAS_CLK_STRETCH_EN_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else {
        /* Invalid clock stretching option */
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }
    return SHT3X_RESULT_CODE_OK;
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

uint8_t sht3x_send_single_shot_measurement_cmd(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                               SHT3XCompleteCb cb, void *user_data)
{
    if (!self || !is_valid_repeatability(repeatability) || !is_valid_clock_stretching(clock_stretching)) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2];
    uint8_t rc = get_single_shot_meas_command_code(repeatability, clock_stretching, cmd);
    if (rc != SHT3X_RESULT_CODE_OK) {
        /* We should never end up here, because we verify repeatability and clock stretching options above. */
        return SHT3X_RESULT_CODE_DRIVER_ERR;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;
    self->i2c_write(cmd, sizeof(cmd), self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_read_measurement(SHT3X self, uint32_t flags, SHT3XMeasCompleteCb cb, void *user_data)
{
    self->sequence_cb = cb;
    self->sequence_cb_user_data = user_data;
    self->sequence_type = SHT3X_SEQUENCE_TYPE_READ_MEAS;

    self->i2c_read(self->i2c_read_buf, 5, self->i2c_addr, meas_i2c_complete_cb, (void *)self);

    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                           SHT3XMeasCompleteCb cb, void *user_data)
{
    if (!self || !is_valid_repeatability(repeatability) || !is_valid_clock_stretching(clock_stretching)) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2];
    uint8_t rc = get_single_shot_meas_command_code(repeatability, clock_stretching, cmd);
    if (rc != SHT3X_RESULT_CODE_OK) {
        /* We should never end up here, because we verify repeatability and clock stretching options above. */
        return SHT3X_RESULT_CODE_DRIVER_ERR;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;
    self->sequence_type = SHT3X_SEQUENCE_TYPE_SINGLE_SHOT_MEAS;
    self->repeatability = repeatability;
    self->clock_stretching = clock_stretching;

    /* Passing self as user data, so that we can invoke SHT3XMeasCompleteCb in read_single_shot_measurement_part_x
     */
    self->i2c_write(cmd, sizeof(cmd), self->i2c_addr, read_single_shot_measurement_part_2, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

void sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data)
{
    if (free_instance_memory) {
        free_instance_memory((void *)self, user_data);
    }
}
