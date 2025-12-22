#ifndef SRC_SHT3X_PRIVATE_H
#define SRC_SHT3X_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "sht3x_defs.h"

/* This header should be included only by the user module implementing the SHT3XGetInstanceMemory callback which is a
 * part of InitConfig passed to sht3x_create. All other user modules are not allowed to include this header, because
 * otherwise they would know about the SHT3XStruct struct definition and can manipulate private data of a SHT3X instance
 * directly. */

/* Defined in a separate header, so that both sht3x.c and the user module implementing SHT3XGetInstanceMemory callback
 * can include this header. The user module needs to know sizeof(SHT3XStruct), so that it knows the size of SHT3X
 * instances at compile time. This way, it has an option to allocate a static array with size equal to the required
 * number of instances. */
struct SHT3XStruct {
    SHT3X_I2CWrite i2c_write;
    /** Callback to execute once the current sequence is complete. Since different sequences can have different callback
     * complete types, use a void *. */
    void *sequence_cb;
};

#ifdef __cplusplus
}
#endif

#endif /* SRC_SHT3X_PRIVATE_H */
