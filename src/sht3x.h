#ifndef SRC_SHT3X_H
#define SRC_SHT3X_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

typedef void *(*SHT3XGetInstanceMemory)(void *user_data);

typedef struct SHT3XStruct *SHT3X;

typedef enum {
    SHT3X_RETURN_CODE_OK = 0,
    SHT3X_RETURN_CODE_INVALID_ARG,
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
