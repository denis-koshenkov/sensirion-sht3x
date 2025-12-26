#ifndef SRC_TEST_MOCK_MOCK_CFG_FUNCTIONS_H
#define SRC_TEST_MOCK_MOCK_CFG_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "sht3x.h"

void *mock_sht3x_get_instance_memory(void *user_data);

void mock_sht3x_free_instance_memory(void *instance_memory, void *user_data);

void mock_sht3x_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                          SHT3X_I2CTransactionCompleteCb cb, void *cb_user_data);

void mock_sht3x_i2c_read(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                         SHT3X_I2CTransactionCompleteCb cb, void *cb_user_data);

void mock_sht3x_start_timer(uint32_t duration_ms, void *user_data, SHT3XTimerExpiredCb cb, void *cb_user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_TEST_MOCK_MOCK_CFG_FUNCTIONS_H */
