#include "CppUTestExt/MockSupport.h"
#include "mock_cfg_functions.h"

void *mock_sht3x_get_instance_memory(void *user_data)
{
    mock().actualCall("mock_sht3x_get_instance_memory").withParameter("user_data", user_data);
    return mock().pointerReturnValue();
}

void mock_sht3x_free_instance_memory(void *instance_memory, void *user_data)
{
    mock()
        .actualCall("mock_sht3x_free_instance_memory")
        .withParameter("instance_memory", instance_memory)
        .withParameter("user_data", user_data);
}

void mock_sht3x_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                          SHT3X_I2CTransactionCompleteCb cb, void *cb_user_data)
{
    SHT3X_I2CTransactionCompleteCb *cb_p =
        (SHT3X_I2CTransactionCompleteCb *)mock().getData("i2cWriteCompleteCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("i2cWriteCompleteCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", data, length)
        .withParameter("length", length)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

void mock_sht3x_i2c_read(uint8_t *data, size_t length, uint8_t i2c_addr, void *user_data,
                         SHT3X_I2CTransactionCompleteCb cb, void *cb_user_data)
{
    SHT3X_I2CTransactionCompleteCb *cb_p =
        (SHT3X_I2CTransactionCompleteCb *)mock().getData("i2cReadCompleteCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("i2cReadCompleteCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_sht3x_i2c_read")
        .withOutputParameter("data", data)
        .withParameter("length", length)
        .withParameter("i2c_addr", i2c_addr)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

void mock_sht3x_start_timer(uint32_t duration_ms, SHT3XTimerExpiredCb cb, void *user_data)
{
    SHT3XTimerExpiredCb *cb_p = (SHT3XTimerExpiredCb *)mock().getData("timerExpiredCb").getPointerValue();
    void **user_data_p = (void **)mock().getData("timerExpiredCbUserData").getPointerValue();
    *cb_p = cb;
    *user_data_p = user_data;

    mock()
        .actualCall("mock_sht3x_start_timer")
        .withParameter("duration_ms", duration_ms)
        .withParameter("cb", cb)
        .withParameter("user_data", user_data);
}
