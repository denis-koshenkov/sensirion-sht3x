#include <stddef.h>
#include <stdbool.h>

#include "sht3x.h"
#include "sht3x_private.h"

#define SHT3X_CMD_SINGLE_SHOT_MEAS_REPEATABILITY_HIGH_CLK_STRETCH_DIS {0x24, 0x00}

static bool is_valid_i2c_addr(uint8_t i2c_addr)
{
    return ((i2c_addr == 0x44) || (i2c_addr == 0x45));
}

static void i2c_write_complete_cb(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
    cb(SHT3X_RESULT_CODE_IO_ERR, NULL, NULL);
}

uint8_t sht3x_create(SHT3X *const instance, const SHT3XInitConfig *const cfg)
{
    if (!instance || !cfg || !(cfg->get_instance_memory) || !(is_valid_i2c_addr(cfg->i2c_addr))) {
        return SHT3X_RESULT_CODE_INVALID_ARG;
    }

    *instance = cfg->get_instance_memory(cfg->get_instance_memory_user_data);
    if (!(*instance)) {
        /* get_instance_memory returned NULL -> no memory for this instance */
        return SHT3X_RESULT_CODE_OUT_OF_MEMORY;
    }

    (*instance)->i2c_write = cfg->i2c_write;

    return SHT3X_RESULT_CODE_OK;
}

void sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                        SHT3XMeasCompleteCb cb, void *user_data)
{
    self->sequence_cb = (void *)cb;

    uint8_t data[] = SHT3X_CMD_SINGLE_SHOT_MEAS_REPEATABILITY_HIGH_CLK_STRETCH_DIS;
    /* Passing self as user data, so that we can invoke SHT3XMeasCompleteCb in the I2C write complete cb */
    self->i2c_write(data, sizeof(data), 0x44, i2c_write_complete_cb, (void *)self);
}

void sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data)
{
    if (free_instance_memory) {
        free_instance_memory((void *)self, user_data);
    }
}
