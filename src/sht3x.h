#ifndef SRC_SHT3X_H
#define SRC_SHT3X_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#include "sht3x_defs.h"

typedef struct SHT3XStruct *SHT3X;

/**
 * @brief SHT3X device driver.
 *
 * # Single Shot Measurements
 * The easiest way to perform and read out a single shot measurement is by calling @ref
 * sht3x_read_single_shot_measurement. This function performs all the necessary steps for initiating and reading out the
 * measurement.
 *
 * # Periodic measurements
 * 1. Start periodic measurements by calling @ref sht3x_start_periodic_measurement or @ref
 * sht3x_start_periodic_measurement_art.
 * 2. Periodically call @ref sht3x_read_periodic_measurement to read out the measurements.
 * 3. Call @ref sht3x_stop_periodic_measurement to stop periodic measurements. After this, single shot measurements can
 * be performed again.
 *
 * # Soft Reset
 * Device can be reset via a soft reset command. After the device receives the command, it takes up to 1.5 ms to perform
 * the reset until it is able to process I2C commands again.
 *
 * It is recommended to do soft reset by calling @ref sht3x_soft_reset_with_delay. This function executes its complete
 * callback after 2 ms (rounded up from 1.5 ms) from the moment the device receives the soft reset command.
 *
 * This ensures that the caller can immediately start communicating with the device after that callback is executed.
 *
 * On the contrary, if soft reset is performed by calling @ref sht3x_soft_reset, it is the caller's responsibility to
 * wait until the device becomes responsive after the reset.
 *
 * # Mandatory delay between commands
 * The SHT3X device has a mandatory delay of at least 1 ms between I2C commands that it receives.
 *
 * Some driver functions perform several I2C transactions before they execute the complete callback. This delay is
 * always maintained between those transactions.
 *
 * However, this driver does not guarantee this delay when two different driver functions are invoked after each other.
 * It is the caller's responsibility to ensure that there is a delay of at least 1 ms between calls to functions of this
 * driver.
 */

/**
 * @brief Gets called in @ref sht3x_create to get memory for a SHT3X instance.
 *
 * The implementation of this function should return a pointer to memory of size sizeof(struct SHT3XStruct). All private
 * data for the created SHT3X instance will reside in that memory.
 *
 * The implementation of this function should be defined in a separate source file. That source file should include
 * sht3x_private.h, which contains the definition of struct SHT3XStruct. The implementation of this function then knows
 * at compile time the size of memory that it needs to provide.
 *
 * This function will be called as many times as @ref sht3x_create is called (given that all parameters passed to @ref
 * sht3x_create are valid). The implementation should be capable of returning memory for that many distinct instances.
 *
 * Implementation example - two statically allocated instances:
 * ```
 * void *get_instance_memory(void *user_data) {
 *     static struct SHT3XStruct instances[2];
 *     static size_t idx = 0;
 *     return (idx < 2) ? (&(instances[idx++])) : NULL;
 * }
 * ```
 *
 * If the application uses dynamic memory allocation, another implementation option is to allocate sizeof(struct
 * SHT3XStruct) bytes dynamically.
 *
 * @param user_data When this function is called, this parameter will be equal to the get_instance_memory_user_data
 * field in the SHT3XInitConfig passed to @ref sht3x_create.
 *
 * @return void * Pointer to instance memory of size sizeof(struct SHT3XStruct). If failed to get memory, should return
 * NULL. In that case, @ref sht3x_create will return @ref SHT3X_RESULT_CODE_OUT_OF_MEMORY.
 */
typedef void *(*SHT3XGetInstanceMemory)(void *user_data);

/**
 * @brief Gets called in @ref sht3x_destroy to free memory of a SHT3X instance.
 *
 * If the implementation of @ref SHT3XGetInstanceMemory requires that the memory is freed when the instance is
 * destroyed, this function provides a hook to do so.
 *
 * We expect that in most use cases, this function will not be used, as usually sensor driver instances are used for the
 * entire duration of the program. In these use cases, @ref sht3x_destroy will never be called. However, if the user
 * does need to destroy SHT3X instances during runtime, this function allows to do so.
 *
 * For example, the user dynamically allocated memory for the SHT3X instance inside @ref SHT3XGetInstanceMemory. The
 * user now wants to destroy the SHT3X instance and reuse that memory elsewhere. The user should implement this function
 * to call free() and pass @p instance_memory as the argument. Then, the user calls @ref sht3x_destroy and provides the
 * implementation of this function as the free_instance_memory argument. The implementation of @ref sht3x_destroy will
 * then invoke this function, and the instance memory will be freed.
 *
 * @param instance_memory This will be the instance memory that was returned by @ref SHT3XGetInstanceMemory for this
 * SHT3X instance.
 * @param user_data This will be user_data argument passed to @ref sht3x_destroy.
 */
typedef void (*SHT3XFreeInstanceMemory)(void *instance_memory, void *user_data);

/** Represents a single measurement that can be read out from the device. */
typedef struct {
    float temperature; /**< Temperature in degress celsius. */
    float humidity;    /**< Humidity in RH%. */
} SHT3XMeasurement;

/**
 * @brief Callback type to execute when the driver finishes reading out a measurement.
 *
 * @param result_code Indicates success or the reason for failure.
 * @param meas Measurement that was read out. Undefined value if @p result_code is not SHT3X_RESULT_CODE_OK. Do not
 * dereference the pointer in that case, it may be NULL.
 * @param user_data User data.
 *
 * @note The @p meas pointer only points to valid memory during the execution of this callback. It is not allowed to
 * dereference this pointer after this callback finished executing.
 */
typedef void (*SHT3XMeasCompleteCb)(uint8_t result_code, SHT3XMeasurement *meas, void *user_data);

/**
 * @brief Callback type to execute when the driver finishes a sequence.
 *
 * @param result_code Indicates success or the reason for failure.
 * @param user_data User data.
 */
typedef void (*SHT3XCompleteCb)(uint8_t result_code, void *user_data);

/**
 * @brief Callback type to execute when the driver finishes reading out status register.
 *
 * @param result_code Indicates success or the reason for failure.
 * @param reg_val Status register value that was read out from the device. Undefined value if @p result_code is not
 * SHT3X_RESULT_CODE_OK.
 * @param user_data User data.
 */
typedef void (*SHT3XReadStatusRegCompleteCb)(uint8_t result_code, uint16_t reg_val, void *user_data);

/** @brief Flag indicating that temperature measurement will be read. */
#define SHT3X_FLAG_READ_TEMP (1U << 0)
/** @brief Flag indicating that humidity measurement will be read. */
#define SHT3X_FLAG_READ_HUM (1U << 1)
/** @brief Flag indicating that temperature measurement CRC will be validated. */
#define SHT3X_FLAG_VERIFY_CRC_TEMP (1U << 2)
/** @brief Flag indicating that humidity measurement CRC will be validated. */
#define SHT3X_FLAG_VERIFY_CRC_HUM (1U << 3)

/* Macros for readability to pass to verify_crc parameter of sht3x_read_status_register */
#define SHT3X_VERIFY_CRC_YES true
#define SHT3X_VERIFY_CRC_NO false

typedef enum {
    SHT3X_RESULT_CODE_OK = 0,
    SHT3X_RESULT_CODE_DRIVER_ERR,
    SHT3X_RESULT_CODE_INVALID_ARG,
    SHT3X_RESULT_CODE_OUT_OF_MEMORY,
    SHT3X_RESULT_CODE_IO_ERR,
    SHT3X_RESULT_CODE_NO_DATA,
    SHT3X_RESULT_CODE_CRC_MISMATCH,
    /** Previous operation is still ongoing, cannot start a new one. */
    SHT3X_RESULT_CODE_BUSY,
} SHT3XResultCode;

typedef enum {
    SHT3X_MEAS_REPEATABILITY_HIGH,
    SHT3X_MEAS_REPEATABILITY_MEDIUM,
    SHT3X_MEAS_REPEATABILITY_LOW,
} SHT3XMeasRepeatability;

typedef enum {
    SHT3X_CLOCK_STRETCHING_ENABLED,
    SHT3X_CLOCK_STRETCHING_DISABLED,
} SHT3XClockStretching;

/** @brief Measurement per second (MPS) options for periodic data acquisition. */
typedef enum {
    SHT3X_MPS_0_5,
    SHT3X_MPS_1,
    SHT3X_MPS_2,
    SHT3X_MPS_4,
    SHT3X_MPS_10,
} SHT3XMps;

typedef struct {
    SHT3XGetInstanceMemory get_instance_memory;
    /** User data to pass to get_instance_memory function. */
    void *get_instance_memory_user_data;
    SHT3X_I2CWrite i2c_write;
    /** User data to pass to i2c_write function. */
    void *i2c_write_user_data;
    SHT3X_I2CRead i2c_read;
    /** User data to pass to i2c_read function. */
    void *i2c_read_user_data;
    SHT3XStartTimer start_timer;
    /** User data to pass to start_timer function. */
    void *start_timer_user_data;
    /** Can be only 0x44 or 0x45 according to the datasheet. */
    uint8_t i2c_addr;
} SHT3XInitConfig;

/**
 * @brief Create SHT3X instance.
 *
 * @param[out] instance Created instance is written to this parameter, if SHT3X_RESULT_CODE_OK is returned. Otherwise,
 * the value is undefined.
 * @param[in] cfg Init config. Can be allocated on the stack, it does not have to persist through the entire lifecycle
 * of the instance. The implementation copies all necessary data to internal structures.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully created instance.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG Invalid argument. @p instance, @p cfg, or one of the required function pointers
 * in @p cfg is NULL; or i2c_addr is not a valid SHT3X I2C address.
 * @retval SHT3X_RESULT_CODE_OUT_OF_MEMORY cfg->get_instance_memory returned NULL.
 */
uint8_t sht3x_create(SHT3X *const instance, const SHT3XInitConfig *const cfg);

/**
 * @brief Send a single shot measurement command to the device.
 *
 * This function only sends a command to the device, it does not read out the measurements.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] repeatability Repeatability option, use @ref SHT3XMeasRepeatability.
 * @param[in] clock_stretching Clock stretching option, use @ref SHT3XClockStretching.
 * @param[in] cb Callback to execute once the command is complete. Can be NULL if not needed.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully sent the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL, @p repeatability option is invalid, or @p clock_stretching
 * option is invalid.
 * @retval SHT3X_RESULT_CODE_IO_ERR I2C transaction failed - SHT3X_I2CWrite function did not return
 * SHT3X_I2C_RESULT_CODE_OK.
 * @retval SHT3X_RESULT_CODE_DRIVER_ERR Something went wrong in this driver code.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_send_single_shot_measurement_cmd(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                               SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Read previously requested measurements.
 *
 * This function can be used in two ways: reading out previously requested single shot measurement and reading out
 * periodic measurements.
 *
 * Reading out single shot measurements.
 * Requesting single shot measurements is done by calling @ref sht3x_send_single_shot_measurement_cmd. That function can
 * be called with clock stretching enabled or disabled. This option affects the behavior of this function.
 *
 * Measurements are not ready right after a single shot measurement command is sent. They take time to be ready. The
 * times it takes for measurements to be ready depends on the repeatability parameter (high, medium, low) passed to @ref
 * sht3x_send_single_shot_measurement_cmd. See the datasheet for details.
 *
 * If @ref sht3x_send_single_shot_measurement_cmd was called with clock stretching enabled, the I2C transaction
 * initiated by this function will continue until measurements are ready. Measurements will then be read out in the same
 * I2C transaction. SHT3X pulls the SCL line low (clock stretching) until the measurements are ready.
 *
 * If @ref sht3x_send_single_shot_measurement_cmd was called with clock stretching disabled, and this function is called
 * before the measurements are ready, there will be a NAK after the address byte on the I2C bus. This is expected
 * behavior described in the datasheet. In order to notify the caller of this function that this occurred, result_code
 * parameter in @p cb will be set to @ref SHT3X_RESULT_CODE_NO_DATA.
 *
 * In that case, this function will need to be called again later. If the measurements are ready, they will be read out
 * and result_code parameter in @p cb will be set to @ref SHT3X_RESULT_CODE_OK.
 *
 * Reading out periodic measurements.
 * Periodic measurements first need to be started by calling @ref sht3x_start_periodic_measurement or @ref
 * sht3x_start_periodic_measurement_art. This needs to be done once.
 *
 * After that, the device starts performing periodic measurements. Every time measurements need to be read out, do the
 * following:
 * 1. Call @ref sht3x_fetch_periodic_measurement_data.
 * 2. Call this function to read out the measurements.
 *
 * If there are no measurements available, there will be a NAK after the address byte on the I2C bus. In that case, the
 * result_code parameter in @p cb will be set to @ref SHT3X_RESULT_CODE_NO_DATA. Once a specific measurement is read
 * out, result_code will be @ref SHT3X_RESULT_CODE_NO_DATA until the device generates a new measurement.
 *
 * The following flags can be passed to the @p flags parameter:
 * - @ref SHT3X_FLAG_READ_TEMP Temperature measurement will be read out. If result_code parameter in @p cb is set to
 * SHT3X_RESULT_CODE_OK, the temperature measurement is available in the meas parameter of @p cb.
 * - @ref SHT3X_FLAG_READ_HUM Humidity measurement will be read out. If result_code parameter in @p cb is set to
 * SHT3X_RESULT_CODE_OK, the humidity measurement is available in the meas parameter of @p cb.
 * - @ref SHT3X_FLAG_VERIFY_CRC_TEMP Temperature CRC will be verified. If the CRC verification fails, result_code
 * parameter in @p cb will be SHT3X_RESULT_CODE_CRC_MISMATCH.
 * - @ref SHT3X_FLAG_VERIFY_CRC_HUM Humidity CRC will be verified. If the CRC verification fails, result_code parameter
 * in @p cb will be SHT3X_RESULT_CODE_CRC_MISMATCH.
 *
 * Rules for setting flags:
 * - At least one of @ref SHT3X_FLAG_READ_TEMP and @ref SHT3X_FLAG_READ_HUM must be set.
 * - It is not allowed to set @ref SHT3X_FLAG_VERIFY_CRC_TEMP unless @ref SHT3X_FLAG_READ_TEMP is also set.
 * - It is not allowed to set @ref SHT3X_FLAG_VERIFY_CRC_HUM unless @ref SHT3X_FLAG_READ_HUM is also set.
 *
 * Possible values of result_code parameter in @p cb and their meaning:
 * - @ref SHT3X_RESULT_CODE_OK Successfully read out measurements. The requested measurements are available in meas
 * parameter of @p cb. All requested CRC checks were successful.
 * - @ref SHT3X_RESULT_CODE_CRC_MISMATCH The measurements were read out successfully, but one of the requested CRC
 * checks failed. Do not access meas parameter in @p cb. There are no measurements available there.
 * - @ref SHT3X_RESULT_CODE_NO_DATA SHT3X responded with a NAK after the address byte. As described above, this means
 * that measurements are not ready. If @ref sht3x_send_single_shot_measurement_cmd was called with clock stretching
 * enabled, this case should be treated as an error.
 * - @ref SHT3X_RESULT_CODE_IO_ERR Another IO error occurred. Failed to read out measurements.
 *
 * @note It is only allowed to access meas parameter in @p cb if result_code parameter in @p cb is @ref
 * SHT3X_RESULT_CODE_OK. In all other cases, that pointer should not be dereferenced.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] flags Read measurement options.
 * @param[in] cb Callback to execute once complete. Can be NULL if not required. The result_code parameter will signify
 * success or reason for failure, and meas parameter will contain a pointer to requested measurements if result_code is
 * SHT3X_RESULT_CODE_OK. Note that only requested measurements will be available. For example, if only @ref
 * SHT3X_FLAG_READ_HUM flag was set, meas->temperature inside @p cb has an undefined value and should not be used.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully triggered measurement reaodut. Note that this does not mean that
 * measurement readout was successful - this is indicated by the result_code parameter of @p cb.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL, or combination of @p flags is invalid.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 * @retval SHT3X_RESULT_CODE_DRIVER_ERR Something went wrong in this driver code.
 */
uint8_t sht3x_read_measurement(SHT3X self, uint8_t flags, SHT3XMeasCompleteCb cb, void *user_data);

/**
 * @brief Send start periodic measurement command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] repeatability Repeatability option. Use @ref SHT3XMeasRepeatability.
 * @param[in] mps Measurement per second (MPS) option. Use @ref SHT3XMps.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL, @p repeatability option is invalid, or @p mps option is
 * invalid.
 * @retval SHT3X_RESULT_CODE_DRIVER_ERR Something went wrong in this driver code.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_start_periodic_measurement(SHT3X self, uint8_t repeatability, uint8_t mps, SHT3XCompleteCb cb,
                                         void *user_data);

/**
 * @brief Send accelerated response time (ART) start periodic measurement command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_start_periodic_measurement_art(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send fetch periodic measurement data command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_fetch_periodic_measurement_data(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send stop periodic measurement command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_stop_periodic_measurement(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send soft reset command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent soft reset command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send reset command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending of soft reset command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_soft_reset(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send enable heater command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_enable_heater(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send disable heater command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_disable_heater(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send read status register command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_send_read_status_register_cmd(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Send clear status register command.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully sent the command.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the command.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. result_code parameter of this callback indicates success or reason
 * for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the command.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_clear_status_register(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Perform a single shot measurement and read the result.
 *
 * Performs the following steps:
 * 1. Send a single shot measurement command with the specified @p repeatability and @p clock_stretching options.
 * 2. Wait for a duration that is defined by @p repeatability and @p clock_stretching options. See below for details.
 * 3. Read out the measurements according to @p flags.
 * 4. Invoke @p cb with the read out measurements as a parameter.
 *
 * Duration of delay in step 2:
 * - Any repeatability option and clock stretching enabled: 1 ms
 * - High repeatability option and clock stretching disabled: 16 ms
 * - Medium repeatability option and clock stretching disabled: 7 ms
 * - Low repeatability option and clock stretching disabled: 5 ms
 *
 * When clock stretching is enabled, it is assumed that the caller wants to initiate the I2C read transaction as soon as
 * possible after sending a single shot measurement command. The 1 ms delay is a mandatory minimum delay between two I2C
 * transactions.
 *
 * When clock stretching is disabled, the delay is the maximum time it takes for the measurement to be ready. This
 * guarantees that when we send the I2C read transaction, the measurements are ready for readout. The values for
 * different repeatability options are taken from the datasheet and rounded up to whole ms.
 *
 * See @ref sht3x_read_measurement for the description of available flags.
 *
 * Calling this function is equivalent to:
 * 1. Calling @ref sht3x_send_single_shot_measurement_cmd with @p repeatability and @p clock_stretching.
 * 2. Waiting for the delay based on repeatability and clock stretching options.
 * 3. Calling @ref sht3x_read_measurement with @p flags.
 *
 * This function provides an easy way to fully perform a single shot measurement in one function.
 *
 * See @ref sht3x_read_measurement for the description possible values of result_code parameter in @p cb and their
 * meaning.
 *
 * @note It is only allowed to access meas parameter in @p cb if result_code parameter in @p cb is @ref
 * SHT3X_RESULT_CODE_OK. In all other cases, that pointer should not be dereferenced.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] repeatability Repeatability option, use @ref SHT3XMeasRepeatability.
 * @param[in] clock_stretching Clock stretching option, use @ref SHT3XClockStretching.
 * @param[in] flags Read measurement options.
 * @param[in] cb Callback to execute once the command is complete. Can be NULL if not needed.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully triggered single shot measurement. Note that this does not mean that
 * measurement readout was successful - this is indicated by the result_code parameter of @p cb.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL, @p repeatability option is invalid, @p clock_stretching option
 * is invalid, or combination of @p flags is invalid.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 * @retval SHT3X_RESULT_CODE_DRIVER_ERR Something went wrong in this driver code.
 */
uint8_t sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching, uint8_t flags,
                                           SHT3XMeasCompleteCb cb, void *user_data);

/**
 * @brief Read out a periodic measurements.
 *
 * @pre Periodic measurements have been started by calling @ref sht3x_start_periodic_measurement or @ref
 * sht3x_start_periodic_measurement_art.
 *
 * Steps:
 * 1. Send fetch periodic data measurement command.
 * 2. Wait for 1 ms - mandatory delay between sending two I2C commands.
 * 3. Read out the measurements according to @p flags.
 * 4. Invoke @p cb with the read out measurements as a parameter.
 *
 * Calling this function is equivalent to:
 * 1. Calling @ref sht3x_fetch_periodic_measurement_data with.
 * 2. Waiting for 1 ms.
 * 3. Calling @ref sht3x_read_measurement with @p flags.
 *
 * See @ref sht3x_read_measurement for the description of available flags.
 *
 * This function provides an easy way to read out a periodic measurement in one function.
 *
 * See @ref sht3x_read_measurement for the description possible values of result_code parameter in @p cb and their
 * meaning.
 *
 * @note It is only allowed to access meas parameter in @p cb if result_code parameter in @p cb is @ref
 * SHT3X_RESULT_CODE_OK. In all other cases, that pointer should not be dereferenced.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] flags Read measurement options.
 * @param[in] cb Callback to execute once the command is complete. Can be NULL if not needed.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully triggered reading a periodic measurement. Note that this does not mean that
 * measurement readout was successful - this is indicated by the result_code parameter of @p cb.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL, or combination of @p flags is invalid.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_read_periodic_measurement(SHT3X self, uint8_t flags, SHT3XMeasCompleteCb cb, void *user_data);

/**
 * @brief Perform soft reset and wait for 2 ms afterwards.
 *
 * After soft reset command is issued, it takes up to 1.5 ms until the device can process incoming I2C commands. This is
 * stated in device timings in the datasheet. We round up the delay to 2 ms. @p cb is executed after this delay elapses.
 *
 * Potential values of result_code parameter of @p cb:
 * - @ref SHT3X_RESULT_CODE_OK Successfully performed soft reset with delay.
 * - @ref SHT3X_RESULT_CODE_IO_ERR I2C transaction failed, failed to send the soft reset command. In this case, @p cb is
 * executed immediately after the I2C transaction, without the 2 ms delay.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] cb Callback to execute once complete. Can be NULL if not needed. result_code parameter of this callback
 * indicates success or reason for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated sending the soft reset with delay sequence.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed, there is currently another sequence in progress.
 */
uint8_t sht3x_soft_reset_with_delay(SHT3X self, SHT3XCompleteCb cb, void *user_data);

/**
 * @brief Read status register.
 *
 * Steps:
 * 1. Send read status register write command.
 * 2. Wait for 1 ms - mandatory delay between subsequent I2C commands.
 * 3. Send read command to read out the status register value. If @p verify_crc is @ref SHT3X_VERIFY_CRC_YES, also read
 * out the status register CRC and verify it.
 * 4. Call @p cb with the read out status register value as a parameter.
 *
 * Possible values of result_code parameter in @p cb and their meaning:
 * - @ref SHT3X_RESULT_CODE_OK Successfully read out status register. The read out status register value is available in
 * reg_val parameter of @p cb. If CRC verification was requested, it was successful.
 * - @ref SHT3X_RESULT_CODE_CRC_MISMATCH The status register was read out successfully, but the CRC veridication failed.
 * reg_val parameter in @p cb has undefined value.
 * - @ref SHT3X_RESULT_CODE_IO_ERR Failed to read out status register due to I2C IO error. reg_val parameter in @p cb
 * has undefined value.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param verify_crc Use @ref SHT3X_VERIFY_CRC_YES to read out status register CRC and verify it, use @ref
 * SHT3X_VERIFY_CRC_NO to not read out status register CRC and not verify it.
 * @param[in] cb Callback to execute once complete. Can be NULL if not needed. result_code parameter of this callback
 * indicates success or reason for failure.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully initiated read status register sequence.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed to initiate sequence, there is currently another sequence in progress.
 */
uint8_t sht3x_read_status_register(SHT3X self, bool verify_crc, SHT3XReadStatusRegCompleteCb cb, void *user_data);

/**
 * @brief Destroy a SHT3X instance.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] free_instance_memory Optional user-defined function to free SHT3X instance memory. See @ref
 * SHT3XFreeInstanceMemory. Pass NULL if not needed.
 * @param[in] user_data Optional user data to pass to @p free_instance_memory function.
 *
 * @retval SHT3X_RESULT_CODE_OK Successfully destroyed the instance.
 * @retval SHT3X_RESULT_CODE_INVALID_ARG @p self is NULL.
 * @retval SHT3X_RESULT_CODE_BUSY Failed to destroy the instance, because there is currently a sequence in progress.
 */
uint8_t sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data);

/**
 * @brief Check whether CRC of last write transfer was correct.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true CRC of last write transfer was correct.
 * @retval false CRC of last write transfer failed.
 */
bool sht3x_is_crc_of_last_write_transfer_correct(uint16_t status_reg_val);

/**
 * @brief Check whether last command was executed successfully.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true Last command was executed successfully.
 * @retval false Last command was not processed. It was either invalid or failed the CRC check.
 */
bool sht3x_is_last_command_executed_successfully(uint16_t status_reg_val);

/**
 * @brief Check whether system reset was detected since last clear status register command.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true System reset detected (hard reset, soft reset command, or supply fail).
 * @retval false No system reset since last clear status register command.
 */
bool sht3x_is_system_reset_detected(uint16_t status_reg_val);

/**
 * @brief Check whether temperature alert is currently raised.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true Temperature alert is raised.
 * @retval false Temperature alert is not raised.
 */
bool sht3x_is_temperature_alert_raised(uint16_t status_reg_val);

/**
 * @brief Check whether humidity alert is currently raised.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true Humidity alert is raised.
 * @retval false Humidity alert is not raised.
 */
bool sht3x_is_humidity_alert_raised(uint16_t status_reg_val);

/**
 * @brief Check whether heater is on.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true Heater is on.
 * @retval false Heater is off.
 */
bool sht3x_is_heater_on(uint16_t status_reg_val);

/**
 * @brief Check whether at least one alert is pending.
 *
 * @param status_reg_val Status register value read out using @ref sht3x_read_status_register.
 *
 * @retval true At least one alert is pending.
 * @retval false There are no pending alerts.
 */
bool sht3x_is_at_least_one_alert_pending(uint16_t status_reg_val);

#ifdef __cplusplus
}
#endif

#endif /* SRC_SHT3X_H */
