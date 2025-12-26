#ifndef SRC_SHT3X_DEFS_H
#define SRC_SHT3X_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief SHT3X definitions.
 *
 * These definitions are placed in a separate header, so that they can be included both by sht3x_private.h and sht3x.h.
 * They need to be a part of the public interface, but they also need to be included in sht3x_private.h, because they
 * contain data types that are present in the struct SHT3XStruct definition.
 */

/** Result codes describing outcomes of a I2C transaction. */
typedef enum {
    /** Successful I2C transaction. */
    SHT3X_I2C_RESULT_CODE_OK = 0,
    /** @brief NACK received after sending the address.
     *
     * Not considered a bus error, because it is a valid scenario in the following cases:
     *   - No data present when reading measurements after issuing a single-shot measurement command without clock
     * stretching.
     *   - No data present when reading measurements after issuing a periodic measurement command.
     */
    SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
    /** NACK received after sending a data byte, or an unexpected transition occurred on the bus. SHT3X should never
     * NACK after a data byte, so it is considered an error. */
    SHT3X_I2C_RESULT_CODE_BUS_ERROR,
} SHT3X_I2CResultCode;

/**
 * @brief Callback type to execute when a I2C transaction to SHT3X is complete.
 *
 * @param[in] result_code Pass one of the values from @ref SHT3X_I2CResultCode to describe the transaction result.
 * @param[in] user_data The caller must pass user_data parameter that the SHT3X driver passed to @ref SHT3X_I2CRead or
 * @ref SHT3X_I2CWrite.
 */
typedef void (*SHT3X_I2CTransactionCompleteCb)(uint8_t result_code, void *user_data);

/**
 * @brief Definition of callback type to execute when a SHT3X timer expires.
 *
 * @param user_data User data that was passed to the user_data parameter of @ref SHT3XStartTimer.
 */
typedef void (*SHT3XTimerExpiredCb)(void *user_data);

/**
 * @brief Perform a I2C write transaction to the SHT3X device.
 *
 * @param[in] data Data to write to the device.
 * @param[in] length Number of bytes in the @p data array.
 * @param[in] i2c_addr I2C address of the SHT3X device.
 * @param[in] user_data When this function is called, this parameter will be equal to i2c_write_user_data from the init
 * config passed to @ref sht3x_create.
 * @param[in] cb Callback to execute once the I2C transaction is complete. This callback must be executed from the
 * same context that the SHT3X driver API functions get called from.
 * @param[in] cb_user_data User data to pass to @p cb.
 */
typedef void (*SHT3X_I2CWrite)(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                               SHT3X_I2CTransactionCompleteCb cb, void *cb_user_data);

/**
 * @brief Perform a I2C read transaction to the SHT3X device.
 *
 * @param[out] data Data that is read from the device is written to this parameter in case of success. I2C read is
 * successful if the result_code parameter of @p cb is equal to SHT3X_I2C_RESULT_CODE_OK.
 * @param[in] length Number of bytes in the @p data array.
 * @param[in] i2c_addr I2C address of the SHT3X device.
 * @param[in] user_data When this function is called, this parameter will be equal to i2c_read_user_data from the init
 * config passed to @ref sht3x_create.
 * @param[in] cb Callback to execute once the I2C transaction is complete. This callback must be executed from the
 * same context that the SHT3X driver API functions get called from.
 * @param[in] cb_user_data User data to pass to @p cb.
 */
typedef void (*SHT3X_I2CRead)(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                              SHT3X_I2CTransactionCompleteCb cb, void *cb_user_data);

/**
 * @brief Execute @p cb after @p duration_ms ms pass.
 *
 * The driver calls this function when it needs to have a delay between two actions. For example, a command was sent to
 * the device, and the result of the command will be available only after some time. The driver will call this function
 * after it sent the command, and @p cb will contain the implementation of reading the result of the command. The
 * implementation of this callback should call @p cb after at least @p duration_ms pass.
 *
 * @p cb must be executed from the same execution context as all other driver functions are called from.
 *
 * @param[in] duration_ms @p cb must be called after at least this number of ms pass.
 * @param[in] user_data This parameter will be equal to start_timer_user_data from the init config passed to @ref
 * sht3x_create.
 * @param[in] cb Callback to execute.
 * @param[in] cb_ser_data User data to pass to the @p cb callback.
 */
typedef void (*SHT3XStartTimer)(uint32_t duration_ms, void *user_data, SHT3XTimerExpiredCb cb, void *cb_user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_SHT3X_DEFS_H */
