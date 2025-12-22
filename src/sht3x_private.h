#ifndef SRC_SHT3X_PRIVATE_H
#define SRC_SHT3X_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "sht3x_defs.h"

/* This header should be included only by the user module implementing the SHT3XGetInstanceMemory callback which is a
 * part of InitConfig passed to sht3x_create. All other user modules are not allowed to include this header, because
 * otherwise they would know about the SHT3XStruct struct definition and can manipulate private data of a SHT3X instance
 * directly. */

/* SHT3X responds with at most 6 bytes to a I2C read transaction. */
#define SHT3X_I2C_READ_BUF_SIZE 6

/* Defined in a separate header, so that both sht3x.c and the user module implementing SHT3XGetInstanceMemory callback
 * can include this header. The user module needs to know sizeof(SHT3XStruct), so that it knows the size of SHT3X
 * instances at compile time. This way, it has an option to allocate a static array with size equal to the required
 * number of instances. */
struct SHT3XStruct {
    SHT3X_I2CWrite i2c_write;
    SHT3X_I2CRead i2c_read;
    SHT3XStartTimer start_timer;
    /** Callback to execute once the current sequence is complete. Since different sequences can have different callback
     * complete types, use a (void *). */
    void *sequence_cb;
    void *sequence_cb_user_data;
    uint8_t i2c_read_buf[SHT3X_I2C_READ_BUF_SIZE];
    uint8_t i2c_addr;
    /* Repeatability option of the current sequence. */
    uint8_t repeatability;
    /* Clock stretching option of the current sequence. */
    uint8_t clock_stretching;
};

#ifdef __cplusplus
}
#endif

#endif /* SRC_SHT3X_PRIVATE_H */
