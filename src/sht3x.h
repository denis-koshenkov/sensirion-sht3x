#ifndef SRC_SHT3X_H
#define SRC_SHT3X_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "sht3x_defs.h"

typedef struct SHT3XStruct *SHT3X;

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
 * @param user_data User data pointer that was passed to user_data parameter of @ref sht3x_read_single_shot_measurement.
 *
 * @note The @p meas pointer only points to valid memory during the execution of this callback. It is not allowed to
 * dereference this pointer after this callback finished executing.
 */
typedef void (*SHT3XMeasCompleteCb)(uint8_t result_code, SHT3XMeasurement *meas, void *user_data);

typedef void (*SHT3XCompleteCb)(uint8_t result_code, void *user_data);

typedef enum {
    SHT3X_RESULT_CODE_OK = 0,
    SHT3X_RESULT_CODE_DRIVER_ERR,
    SHT3X_RESULT_CODE_INVALID_ARG,
    SHT3X_RESULT_CODE_OUT_OF_MEMORY,
    SHT3X_RESULT_CODE_IO_ERR,
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

typedef struct {
    SHT3XGetInstanceMemory get_instance_memory;
    /** User data to pass to get_instance_memory function. */
    void *get_instance_memory_user_data;
    SHT3X_I2CWrite i2c_write;
    SHT3X_I2CRead i2c_read;
    SHT3XStartTimer start_timer;
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
 */
uint8_t sht3x_send_single_shot_measurement_cmd(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                               SHT3XCompleteCb cb, void *user_data);

uint8_t sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                           SHT3XMeasCompleteCb cb, void *user_data);

/**
 * @brief Destroy a SHT3X instance.
 *
 * @param[in] self Instance created by @ref sht3x_create.
 * @param[in] free_instance_memory Optional user-defined function to free SHT3X instance memory. See @ref
 * SHT3XFreeInstanceMemory. Pass NULL if not needed.
 * @param[in] user_data Optional user data to pass to @p free_instance_memory function.
 */
void sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_SHT3X_H */
