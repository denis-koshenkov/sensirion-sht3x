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

/* From the datasheet - the max amount of time between soft reset command is issued and when sensor is ready to process
 * I2C commands again. Rounded up. */
#define SHT3X_SOFT_RESET_DELAY_MS 2

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

/* Start periodic measurement command codes */
#define SHT3X_START_PERIODIC_MEAS_MPS_0_5 0x20
#define SHT3X_START_PERIODIC_MEAS_MPS_0_5_REPEATABILITY_HIGH 0x32
#define SHT3X_START_PERIODIC_MEAS_MPS_0_5_REPEATABILITY_MEDIUM 0x24
#define SHT3X_START_PERIODIC_MEAS_MPS_0_5_REPEATABILITY_LOW 0x2F
#define SHT3X_START_PERIODIC_MEAS_MPS_1 0x21
#define SHT3X_START_PERIODIC_MEAS_MPS_1_REPEATABILITY_HIGH 0x30
#define SHT3X_START_PERIODIC_MEAS_MPS_1_REPEATABILITY_MEDIUM 0x26
#define SHT3X_START_PERIODIC_MEAS_MPS_1_REPEATABILITY_LOW 0x2D
#define SHT3X_START_PERIODIC_MEAS_MPS_2 0x22
#define SHT3X_START_PERIODIC_MEAS_MPS_2_REPEATABILITY_HIGH 0x36
#define SHT3X_START_PERIODIC_MEAS_MPS_2_REPEATABILITY_MEDIUM 0x20
#define SHT3X_START_PERIODIC_MEAS_MPS_2_REPEATABILITY_LOW 0x2B
#define SHT3X_START_PERIODIC_MEAS_MPS_4 0x23
#define SHT3X_START_PERIODIC_MEAS_MPS_4_REPEATABILITY_HIGH 0x34
#define SHT3X_START_PERIODIC_MEAS_MPS_4_REPEATABILITY_MEDIUM 0x22
#define SHT3X_START_PERIODIC_MEAS_MPS_4_REPEATABILITY_LOW 0x29
#define SHT3X_START_PERIODIC_MEAS_MPS_10 0x27
#define SHT3X_START_PERIODIC_MEAS_MPS_10_REPEATABILITY_HIGH 0x37
#define SHT3X_START_PERIODIC_MEAS_MPS_10_REPEATABILITY_MEDIUM 0x21
#define SHT3X_START_PERIODIC_MEAS_MPS_10_REPEATABILITY_LOW 0x2A

/* ART command code */
#define SHT3X_ART_CMD_MSB 0x2B
#define SHT3X_ART_CMD_LSB 0x32

/* Stop periodic measurement command code */
#define SHT3X_STOP_PERIODIC_MEAS_CMD_MSB 0x30
#define SHT3X_STOP_PERIODIC_MEAS_CMD_LSB 0x93

/* Soft reset command code */
#define SHT3X_SOFT_RESET_CMD_MSB 0x30
#define SHT3X_SOFT_RESET_CMD_LSB 0xA2

/* Enable heater command code */
#define SHT3X_ENABLE_HEATER_CMD_MSB 0x30
#define SHT3X_ENABLE_HEATER_CMD_LSB 0x6D

/* Disable heater command code */
#define SHT3X_DISABLE_HEATER_CMD_MSB 0x30
#define SHT3X_DISABLE_HEATER_CMD_LSB 0x66

/* Clear status register command code */
#define SHT3X_CLEAR_STATUS_REGISTER_CMD_MSB 0x30
#define SHT3X_CLEAR_STATUS_REGISTER_CMD_LSB 0x41

/* Fetch periodic measurement data command code */
#define SHT3X_FETCH_PERIODIC_MEAS_DATA_CMD_MSB 0xE0
#define SHT3X_FETCH_PERIODIC_MEAS_DATA_CMD_LSB 0x0

/* Read status register command code */
#define SHT3X_READ_STATUS_REG_CMD_MSB 0xF3
#define SHT3X_READ_STATUS_REG_CMD_LSB 0x2D

typedef enum {
    SHT3X_SEQUENCE_TYPE_READ_MEAS,
    SHT3X_SEQUENCE_TYPE_SINGLE_SHOT_MEAS,
    SHT3X_SEQUENCE_TYPE_READ_PERIODIC_MEAS,
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
 * @brief Check whether MPS option is valid.
 *
 * @param[in] mps MPS option.
 *
 * @retval true Valid option.
 * @retval false Invalid option.
 */
static bool is_valid_mps(uint8_t mps)
{
    // clang-format off
    return (
        (mps == SHT3X_MPS_0_5)
        || (mps == SHT3X_MPS_1)
        || (mps == SHT3X_MPS_2)
        || (mps == SHT3X_MPS_4)
        || (mps == SHT3X_MPS_10)
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
 * @brief Run SHT3X CRC algorithm on two bytes.
 *
 * @param data Two bytes at this address are used for CRC calculation.

 * @return uint8_t Resulting CRC.
 */
static uint8_t sht3x_crc8(const uint8_t *const data)
{
    uint8_t crc = 0xFF;
    const uint8_t poly = 0x31;

    for (size_t i = 0; i < 2; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
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

/**
 * @brief Thin wrapper around i2c_write for sending fetch data command.
 *
 * @param[in] self SHT3X instance.
 * @param[in] cb Callback to execute once complete.
 * @param[in] user_data User data to pass to callback.
 */
static void send_fetch_data_cmd(SHT3X self, SHT3X_I2CTransactionCompleteCb cb, void *user_data)
{
    uint8_t cmd[2] = {SHT3X_FETCH_PERIODIC_MEAS_DATA_CMD_MSB, SHT3X_FETCH_PERIODIC_MEAS_DATA_CMD_LSB};
    self->i2c_write(cmd, 2, self->i2c_addr, cb, user_data);
}

/**
 * @brief Thin wrapper around i2c_read for measurement readout command.
 *
 * @param[in] self SHT3X instance.
 * @param[in] length Number of bytes to read out from the device.
 * @param[in] cb Callback to execute once complete.
 * @param[in] user_data User data to pass to callback.
 */
static void send_read_measurement_cmd(SHT3X self, size_t length, SHT3X_I2CTransactionCompleteCb cb, void *user_data)
{
    self->i2c_read(self->i2c_read_buf, length, self->i2c_addr, cb, user_data);
}

/**
 * @brief Thin wrapper around i2c_write for sending read status register command.
 *
 * @param[in] self SHT3X instance.
 * @param[in] cb Callback to execute once complete.
 * @param[in] user_data User data to pass to callback.
 */
static void send_read_status_reg_cmd(SHT3X self, SHT3X_I2CTransactionCompleteCb cb, void *user_data)
{
    uint8_t cmd[2] = {SHT3X_READ_STATUS_REG_CMD_MSB, SHT3X_READ_STATUS_REG_CMD_LSB};
    self->i2c_write(cmd, 2, self->i2c_addr, cb, user_data);
}

/**
 * @brief Thin wrapper around i2c_write for sending soft reset command.
 *
 * @param[in] self SHT3X instance.
 * @param[in] cb Callback to execute once complete.
 * @param[in] user_data User data to pass to callback.
 */
static void send_soft_reset_cmd(SHT3X self, SHT3X_I2CTransactionCompleteCb cb, void *user_data)
{
    uint8_t cmd[2] = {SHT3X_SOFT_RESET_CMD_MSB, SHT3X_SOFT_RESET_CMD_LSB};
    self->i2c_write(cmd, 2, self->i2c_addr, cb, user_data);
}

/**
 * @brief Interpret self->sequence_cb as MeasCompleteCb and execute it, if available.
 *
 * @param[in] self SHT3X instance.
 * @param[in] rc Return code to pass to MeasCompleteCb, use @ref SHT3XResultCode.
 * @param[in] meas Measurement pointer to pass to MeasCompleteCb. Can be NULL.
 */
static void execute_meas_complete_cb(SHT3X self, uint8_t rc, SHT3XMeasurement *meas)
{
    if (!self) {
        return;
    }
    SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
    if (cb) {
        cb(rc, meas, self->sequence_cb_user_data);
    }
}

/**
 * @brief Interpret self->sequence_cb as CompleteCb and execute it, if available.
 *
 * @param[in] self SHT3X instance.
 * @param[in] rc Return code to pass to CompleteCb, use @ref SHT3XResultCode.
 */
static void execute_complete_cb(SHT3X self, uint8_t rc)
{
    if (!self) {
        return;
    }
    SHT3XCompleteCb cb = (SHT3XCompleteCb)self->sequence_cb;
    if (cb) {
        cb(rc, self->sequence_cb_user_data);
    }
}

static void generic_i2c_complete_cb(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    uint8_t rc = (result_code == SHT3X_I2C_RESULT_CODE_OK) ? SHT3X_RESULT_CODE_OK : SHT3X_RESULT_CODE_IO_ERR;
    execute_complete_cb(self, rc);
}

static void meas_i2c_complete_cb(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    /* Address NACK is not considered an error as a part of read measurement or read periodic measurement sequences. It
     * is a valid scenario if the measurements are not available. To let the caller distinguish between this scenario
     * and a generic IO error, return a different code when address NACK occurred as a part of read measurement
     * sequence.
     *
     * All other sequences are implemented in a way that address NACK should never happen, so for all other sequences it
     * is considered an IO error. */
    bool return_no_data_if_address_nack = ((self->sequence_type == SHT3X_SEQUENCE_TYPE_READ_MEAS) ||
                                           (self->sequence_type == SHT3X_SEQUENCE_TYPE_READ_PERIODIC_MEAS));
    if (result_code == SHT3X_I2C_RESULT_CODE_ADDRESS_NACK && return_no_data_if_address_nack) {
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_NO_DATA, NULL);
        return;
    } else if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_IO_ERR, NULL);
        return;
    }

    /* I2C transaction successful. Interpret the received bytes according to the flags. */

    /* Verify CRCs if the corresponding flags are set */
    if (self->sequence_flags & SHT3X_FLAG_VERIFY_CRC_HUM) {
        uint8_t expected_hum_crc = sht3x_crc8(&(self->i2c_read_buf[3]));
        uint8_t actual_hum_crc = self->i2c_read_buf[5];
        if (expected_hum_crc != actual_hum_crc) {
            execute_meas_complete_cb(self, SHT3X_RESULT_CODE_CRC_MISMATCH, NULL);
            return;
        }
    }
    if (self->sequence_flags & SHT3X_FLAG_VERIFY_CRC_TEMP) {
        uint8_t expected_temp_crc = sht3x_crc8(&(self->i2c_read_buf[0]));
        uint8_t actual_temp_crc = self->i2c_read_buf[2];
        if (expected_temp_crc != actual_temp_crc) {
            execute_meas_complete_cb(self, SHT3X_RESULT_CODE_CRC_MISMATCH, NULL);
            return;
        }
    }

    /* i2c_read_buf now contains the raw measurements. Need to convert them to temperature in Celsius and
     * humidity in RH%. */
    SHT3XMeasurement meas = {
        .temperature = 0,
        .humidity = 0,
    };
    if (self->sequence_flags & SHT3X_FLAG_READ_TEMP) {
        /* Temperature is the first two bytes in the received data. */
        meas.temperature = convert_raw_temp_meas_to_celsius(&(self->i2c_read_buf[0]));
    }
    if (self->sequence_flags & SHT3X_FLAG_READ_HUM) {
        /* Bytes 3 and 4 in the received data form the raw humidity measurement. */
        meas.humidity = convert_raw_humidity_meas_to_rh(&(self->i2c_read_buf[3]));
    }

    execute_meas_complete_cb(self, SHT3X_RESULT_CODE_OK, &meas);
}

/**
 * @brief Check whether @p flags is a valid combination of read flags.
 *
 * @param flags Flags combination.
 *
 * @retval true Flags combination is valid.
 * @retval false Flags combination is invalid.
 */
static bool read_flags_valid(uint8_t flags)
{
    // clang-format off
    bool flags_invalid = (
        (!(flags & SHT3X_FLAG_READ_TEMP) && !(flags & SHT3X_FLAG_READ_HUM))
        || ((flags & SHT3X_FLAG_VERIFY_CRC_TEMP) && !(flags & SHT3X_FLAG_READ_TEMP))
        || ((flags & SHT3X_FLAG_VERIFY_CRC_HUM) && !(flags & SHT3X_FLAG_READ_HUM))
    );
    // clang-format on
    return !flags_invalid;
}

/**
 * @brief Map read measurement flags to number of bytes to read from the device.
 *
 * @param flags Flags.
 *
 * @return size_t Number of bytes to read, or 0 if flag combination is invalid.
 */
static size_t map_read_meas_flags_to_num_bytes_to_read(uint8_t flags)
{
    size_t num_bytes = 0;
    if (flags == 0) {
        /* We should be reading at least either temperature or humidity */
        num_bytes = 0;
    } else if ((flags & SHT3X_FLAG_VERIFY_CRC_TEMP) && !(flags & SHT3X_FLAG_READ_TEMP)) {
        /* Cannot verify temperature CRC if we are not reading temperature */
        num_bytes = 0;
    } else if ((flags & SHT3X_FLAG_VERIFY_CRC_HUM) && !(flags & SHT3X_FLAG_READ_HUM)) {
        /* Cannot verify humidity CRC if we are not reading humidity */
        num_bytes = 0;
    } else if ((flags & SHT3X_FLAG_READ_HUM) && (flags & SHT3X_FLAG_VERIFY_CRC_HUM)) {
        num_bytes = 6;
    } else if (flags & SHT3X_FLAG_READ_HUM) {
        /* Last byte is humidity CRC, omit it since we are not verifying humidity CRC */
        num_bytes = 5;
    } else if ((flags & SHT3X_FLAG_READ_TEMP) && (flags & SHT3X_FLAG_VERIFY_CRC_TEMP)) {
        /* Last three bytes are humidity meas and its crc, no need to read them out */
        num_bytes = 3;
    } else if (flags & SHT3X_FLAG_READ_TEMP) {
        /* Third byte is temperature CRC, omit it since we are not verifying temperature CRC */
        num_bytes = 2;
    } else {
        // Invalid flag combination
        num_bytes = 0;
    }
    return num_bytes;
}

static void read_single_shot_measurement_part_3(void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    size_t length = map_read_meas_flags_to_num_bytes_to_read(self->sequence_flags);
    if (length == 0) {
        /* Flags are invalid, this should never happen */
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_DRIVER_ERR, NULL);
    }

    send_read_measurement_cmd(self, length, meas_i2c_complete_cb, (void *)self);
}

static void read_single_shot_measurement_part_2(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C write failed, execute meas complete cb to indicate failure */
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_IO_ERR, NULL);
        return;
    }

    uint32_t timer_period = 0;
    uint8_t rc = get_single_shot_meas_timer_period(self->repeatability, self->clock_stretching, &timer_period);
    if (rc != SHT3X_RESULT_CODE_OK) {
        /* We should never end up here, because we verify repeatability and clock stretching options before starting the
         * sequence. */
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_DRIVER_ERR, NULL);
        return;
    }

    self->start_timer(timer_period, read_single_shot_measurement_part_3, (void *)self);
}

static void read_periodic_measurement_part_3(void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    size_t length = map_read_meas_flags_to_num_bytes_to_read(self->sequence_flags);
    if (length == 0) {
        /* Flags are invalid, this should never happen */
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_DRIVER_ERR, NULL);
    }

    send_read_measurement_cmd(self, length, meas_i2c_complete_cb, (void *)self);
}

static void read_periodic_measurement_part_2(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C write failed, execute meas complete cb to indicate failure */
        execute_meas_complete_cb(self, SHT3X_RESULT_CODE_IO_ERR, NULL);
        return;
    }

    /* Give required delay between the fetch command (I2C write) and reading measurements (I2C read) */
    self->start_timer(SHT3X_MIN_DELAY_BETWEEN_TWO_I2C_CMDS_MS, read_periodic_measurement_part_3, (void *)self);
}

static void soft_reset_with_delay_part_3(void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }
    execute_complete_cb(self, SHT3X_RESULT_CODE_OK);
}

static void soft_reset_with_delay_part_2(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C write failed, execute meas complete cb to indicate failure */
        execute_complete_cb(self, SHT3X_RESULT_CODE_IO_ERR);
        return;
    }

    /* Give sensor time to perform soft reset */
    self->start_timer(SHT3X_SOFT_RESET_DELAY_MS, soft_reset_with_delay_part_3, (void *)self);
}

static uint8_t get_start_periodic_meas_cmd(uint8_t repeatability, uint8_t mps, uint8_t *const cmd)
{
    if (!cmd) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    if (mps == SHT3X_MPS_0_5) {
        cmd[0] = SHT3X_START_PERIODIC_MEAS_MPS_0_5;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_0_5_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_0_5_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_0_5_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else if (mps == SHT3X_MPS_1) {
        cmd[0] = SHT3X_START_PERIODIC_MEAS_MPS_1;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_1_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_1_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_1_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else if (mps == SHT3X_MPS_2) {
        cmd[0] = SHT3X_START_PERIODIC_MEAS_MPS_2;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_2_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_2_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_2_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else if (mps == SHT3X_MPS_4) {
        cmd[0] = SHT3X_START_PERIODIC_MEAS_MPS_4;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_4_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_4_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_4_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else if (mps == SHT3X_MPS_10) {
        cmd[0] = SHT3X_START_PERIODIC_MEAS_MPS_10;
        if (repeatability == SHT3X_MEAS_REPEATABILITY_HIGH) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_10_REPEATABILITY_HIGH;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_MEDIUM) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_10_REPEATABILITY_MEDIUM;
        } else if (repeatability == SHT3X_MEAS_REPEATABILITY_LOW) {
            cmd[1] = SHT3X_START_PERIODIC_MEAS_MPS_10_REPEATABILITY_LOW;
        } else {
            /* Invalid repeatability option */
            return SHT3X_RESULT_CODE_INVALID_ARG;
        }
    } else {
        /* Invalid mps option */
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    return SHT3X_RESULT_CODE_OK;
}

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

uint8_t sht3x_read_measurement(SHT3X self, uint8_t flags, SHT3XMeasCompleteCb cb, void *user_data)
{
    if (!self || !read_flags_valid(flags)) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = cb;
    self->sequence_cb_user_data = user_data;
    self->sequence_type = SHT3X_SEQUENCE_TYPE_READ_MEAS;
    self->sequence_flags = flags;

    size_t length = map_read_meas_flags_to_num_bytes_to_read(self->sequence_flags);
    if (length == 0) {
        /* We should never end up here, because we validate flags above. */
        return SHT3X_RESULT_CODE_DRIVER_ERR;
    }

    send_read_measurement_cmd(self, length, meas_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_start_periodic_measurement(SHT3X self, uint8_t repeatability, uint8_t mps, SHT3XCompleteCb cb,
                                         void *user_data)
{
    if (!self || !is_valid_repeatability(repeatability) || !is_valid_mps(mps)) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2];
    uint8_t rc = get_start_periodic_meas_cmd(repeatability, mps, cmd);
    if (rc != SHT3X_RESULT_CODE_OK) {
        /* We should never end up here, because we verify repeatability and mps options above. */
        return SHT3X_RESULT_CODE_DRIVER_ERR;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    self->i2c_write(cmd, 2, self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_start_periodic_measurement_art(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2] = {SHT3X_ART_CMD_MSB, SHT3X_ART_CMD_LSB};

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    self->i2c_write(cmd, 2, self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_fetch_periodic_measurement_data(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    send_fetch_data_cmd(self, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_stop_periodic_measurement(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2] = {SHT3X_STOP_PERIODIC_MEAS_CMD_MSB, SHT3X_STOP_PERIODIC_MEAS_CMD_LSB};

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    self->i2c_write(cmd, 2, self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_soft_reset(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    send_soft_reset_cmd(self, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_enable_heater(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2] = {SHT3X_ENABLE_HEATER_CMD_MSB, SHT3X_ENABLE_HEATER_CMD_LSB};

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    self->i2c_write(cmd, 2, self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_disable_heater(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2] = {SHT3X_DISABLE_HEATER_CMD_MSB, SHT3X_DISABLE_HEATER_CMD_LSB};

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    self->i2c_write(cmd, 2, self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_send_read_status_register_cmd(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    send_read_status_reg_cmd(self, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_clear_status_register(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    uint8_t cmd[2] = {SHT3X_CLEAR_STATUS_REGISTER_CMD_MSB, SHT3X_CLEAR_STATUS_REGISTER_CMD_LSB};

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    self->i2c_write(cmd, 2, self->i2c_addr, generic_i2c_complete_cb, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching, uint8_t flags,
                                           SHT3XMeasCompleteCb cb, void *user_data)
{
    if (!self || !is_valid_repeatability(repeatability) || !is_valid_clock_stretching(clock_stretching) ||
        !read_flags_valid(flags)) {
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
    self->sequence_flags = flags;

    /* Passing self as user data, so that we can invoke SHT3XMeasCompleteCb in read_single_shot_measurement_part_x
     */
    self->i2c_write(cmd, sizeof(cmd), self->i2c_addr, read_single_shot_measurement_part_2, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_read_periodic_measurement(SHT3X self, uint8_t flags, SHT3XMeasCompleteCb cb, void *user_data)
{
    if (!self || !read_flags_valid(flags)) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;
    self->sequence_type = SHT3X_SEQUENCE_TYPE_READ_PERIODIC_MEAS;
    self->sequence_flags = flags;

    send_fetch_data_cmd(self, read_periodic_measurement_part_2, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

uint8_t sht3x_soft_reset_with_delay(SHT3X self, SHT3XCompleteCb cb, void *user_data)
{
    if (!self) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    send_soft_reset_cmd(self, soft_reset_with_delay_part_2, (void *)self);
    return SHT3X_RESULT_CODE_OK;
}

void sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data)
{
    if (free_instance_memory) {
        free_instance_memory((void *)self, user_data);
    }
}
