#ifndef SRC_SHT3X_H
#define SRC_SHT3X_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

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
 * NULL. In that case, @ref sht3x_create will return @ref SHT3X_RETURN_CODE_OUT_OF_MEMORY.
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

typedef enum {
    SHT3X_RETURN_CODE_OK = 0,
    SHT3X_RETURN_CODE_INVALID_ARG,
    SHT3X_RETURN_CODE_OUT_OF_MEMORY,
} SHT3XReturnCode;

typedef struct {
    SHT3XGetInstanceMemory get_instance_memory;
    /** User data to pass to get_instance_memory function. */
    void *get_instance_memory_user_data;
} SHT3XInitConfig;

uint8_t sht3x_create(SHT3X *instance, const SHT3XInitConfig *const cfg);

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
