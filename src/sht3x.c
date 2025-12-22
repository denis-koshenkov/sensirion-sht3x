#include <stddef.h>
#include <stdbool.h>

#include "sht3x.h"
#include "sht3x_private.h"

/* From the datasheet, rounded up */
#define SHT3X_MAX_MEASUREMENT_DURATION_HIGH_REPEATBILITY_MS 16

#define SHT3X_CMD_SINGLE_SHOT_MEAS_REPEATABILITY_HIGH_CLK_STRETCH_DIS {0x24, 0x00}

static bool is_valid_i2c_addr(uint8_t i2c_addr)
{
    return ((i2c_addr == 0x44) || (i2c_addr == 0x45));
}

static bool is_valid_cfg(const SHT3XInitConfig *const cfg)
{
    // clang-format off
    return (
        (cfg)
        && (cfg->get_instance_memory)
        && (cfg->i2c_write)
        && is_valid_i2c_addr(cfg->i2c_addr)
    );
    // clang-format on
}

static void read_single_shot_measurement_part_4(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
    cb(SHT3X_RESULT_CODE_IO_ERR, NULL, self->sequence_cb_user_data);
}

static void read_single_shot_measurement_part_3(void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    self->i2c_read(NULL, 6, self->i2c_addr, read_single_shot_measurement_part_4, (void *)self);
}

static void read_single_shot_measurement_part_2(uint8_t result_code, void *user_data)
{
    SHT3X self = (SHT3X)user_data;
    if (!self) {
        return;
    }

    if (result_code != SHT3X_I2C_RESULT_CODE_OK) {
        /* Previous I2C write failed, execute meas complete cb to indicate failure */
        SHT3XMeasCompleteCb cb = (SHT3XMeasCompleteCb)self->sequence_cb;
        if (!cb) {
            return;
        }
        cb(SHT3X_RESULT_CODE_IO_ERR, NULL, self->sequence_cb_user_data);
        return;
    }

    self->start_timer(SHT3X_MAX_MEASUREMENT_DURATION_HIGH_REPEATBILITY_MS, read_single_shot_measurement_part_3,
                      (void *)self);
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

void sht3x_read_single_shot_measurement(SHT3X self, uint8_t repeatability, uint8_t clock_stretching,
                                        SHT3XMeasCompleteCb cb, void *user_data)
{
    self->sequence_cb = (void *)cb;
    self->sequence_cb_user_data = user_data;

    uint8_t data[] = SHT3X_CMD_SINGLE_SHOT_MEAS_REPEATABILITY_HIGH_CLK_STRETCH_DIS;
    /* Passing self as user data, so that we can invoke SHT3XMeasCompleteCb in read_single_shot_measurement_part_x */
    self->i2c_write(data, sizeof(data), self->i2c_addr, read_single_shot_measurement_part_2, (void *)self);
}

void sht3x_destroy(SHT3X self, SHT3XFreeInstanceMemory free_instance_memory, void *user_data)
{
    if (free_instance_memory) {
        free_instance_memory((void *)self, user_data);
    }
}
