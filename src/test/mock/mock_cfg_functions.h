#ifndef SRC_TEST_MOCK_MOCK_CFG_FUNCTIONS_H
#define SRC_TEST_MOCK_MOCK_CFG_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "sht3x.h"

void *mock_sht3x_get_instance_memory(void *user_data);

void mock_sht3x_free_instance_memory(void *instance_memory, void *user_data);

void mock_sht3x_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, SHT3X_I2CTransactionCompleteCb cb,
                          void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_TEST_MOCK_MOCK_CFG_FUNCTIONS_H */
