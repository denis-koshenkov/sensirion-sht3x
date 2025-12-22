#ifndef SRC_SHT3X_H
#define SRC_SHT3X_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

typedef struct SHT3XStruct *SHT3X;

/**
 * @brief Gets called in @ref sht3x_create to get memory for a SHT31 instance.
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

#ifdef __cplusplus
}
#endif

#endif /* SRC_SHT3X_H */
