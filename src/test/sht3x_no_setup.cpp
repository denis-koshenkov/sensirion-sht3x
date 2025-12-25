#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

/* To return from mock_sht3x_get_instance_memory */
static struct SHT3XStruct instance_memory;

/* These tests are in a separate test group, because they test sht3x_create, including unhappy scenarios. In the
 * other test group, expected call to mock_sht3x_get_instance_memory is set in the common setup function before each
 * test. mock_sht3x_get_instance_memory only gets called from sht3x_create in the happy scenario. In order to test
 * unhappy scenarios, this test group is created, and there are no expected mock calls in the setup function of this
 * test group.
 */

// clang-format off
TEST_GROUP(SHT3XNoSetup)
{
    void setup() {
        /* Order of expected calls is important for these tests. Fail the test if the expected mock calls do not happen
        in the specified order. */
        mock().strictOrder();
    }
};
// clang-format on

TEST(SHT3XNoSetup, CreateReturnsInvalidArgIfGetInstMemoryIsNull)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = NULL,
        .get_instance_memory_user_data = (void *)0x1,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_addr = 0x44,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsInvalidArgIfCfgIsNull)
{
    SHT3X sht3x;
    uint8_t rc = sht3x_create(&sht3x, NULL);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsInvalidArgIfInstanceIsNull)
{
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = (void *)0x1,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x44,
    };
    uint8_t rc = sht3x_create(NULL, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsInvalidArgInvalidI2cAddr)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x46, /* Only 0x44 and 0x45 are valid addresses */
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsInvalidArgI2cWriteNull)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
        .i2c_write = NULL,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x45,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsInvalidArgI2cReadNull)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = NULL,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x44,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsInvalidArgStartTimerNull)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = NULL,
        .i2c_addr = 0x45,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3XNoSetup, CreateReturnsOutOfMemoryIfGetInstanceMemoryReturnsNull)
{
    void *user_data = (void *)0x2;
    mock()
        .expectOneCall("mock_sht3x_get_instance_memory")
        .withParameter("user_data", user_data)
        .andReturnValue((void *)NULL);

    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = user_data,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x44,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OUT_OF_MEMORY, rc);
}

TEST(SHT3XNoSetup, CreateCallsGetInstanceMemory)
{
    void *get_instance_memory_user_data = (void *)0x42;
    mock()
        .expectOneCall("mock_sht3x_get_instance_memory")
        .withParameter("user_data", get_instance_memory_user_data)
        .andReturnValue((void *)&instance_memory);

    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = get_instance_memory_user_data,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x44,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
}

TEST(SHT3XNoSetup, CreateSucceedsWithI2cAddr0x45)
{
    mock()
        .expectOneCall("mock_sht3x_get_instance_memory")
        .withParameter("user_data", (void *)NULL)
        .andReturnValue((void *)&instance_memory);

    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
        .i2c_write = mock_sht3x_i2c_write,
        .i2c_read = mock_sht3x_i2c_read,
        .start_timer = mock_sht3x_start_timer,
        .i2c_addr = 0x45,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
}

TEST(SHT3XNoSetup, IsCrcOfLastWriteTransferCorrectTrue)
{
    uint16_t status_reg_val = 0xFFFE;
    bool ret = sht3x_is_crc_of_last_write_transfer_correct(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsCrcOfLastWriteTransferCorrectFalse)
{
    uint16_t status_reg_val = 0x0001;
    bool ret = sht3x_is_crc_of_last_write_transfer_correct(status_reg_val);
    CHECK_FALSE(ret);
}

TEST(SHT3XNoSetup, IsLastCommandExecutedSuccessfullyTrue)
{
    uint16_t status_reg_val = 0xFFFD;
    bool ret = sht3x_is_last_command_executed_successfully(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsLastCommandExecutedSuccessfullyFalse)
{
    uint16_t status_reg_val = 0x0002;
    bool ret = sht3x_is_last_command_executed_successfully(status_reg_val);
    CHECK_FALSE(ret);
}

TEST(SHT3XNoSetup, IsSystemResetDetectedTrue)
{
    uint16_t status_reg_val = 0x0010;
    bool ret = sht3x_is_system_reset_detected(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsSystemResetDetectedFalse)
{
    uint16_t status_reg_val = 0xFFEF;
    bool ret = sht3x_is_system_reset_detected(status_reg_val);
    CHECK_FALSE(ret);
}

TEST(SHT3XNoSetup, IsTemperatureAlertActiveTrue)
{
    uint16_t status_reg_val = 0x0400;
    bool ret = sht3x_is_temperature_alert_raised(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsTemperatureAlertActiveFalse)
{
    uint16_t status_reg_val = 0xFBFF;
    bool ret = sht3x_is_temperature_alert_raised(status_reg_val);
    CHECK_FALSE(ret);
}

TEST(SHT3XNoSetup, IsHumidityAlertActiveTrue)
{
    uint16_t status_reg_val = 0x0800;
    bool ret = sht3x_is_humidity_alert_raised(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsHumidityAlertActiveFalse)
{
    uint16_t status_reg_val = 0xF7FF;
    bool ret = sht3x_is_humidity_alert_raised(status_reg_val);
    CHECK_FALSE(ret);
}

TEST(SHT3XNoSetup, IsHeaterOnTrue)
{
    uint16_t status_reg_val = 0x2000;
    bool ret = sht3x_is_heater_on(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsHeaterOnFalse)
{
    uint16_t status_reg_val = 0xDFFF;
    bool ret = sht3x_is_heater_on(status_reg_val);
    CHECK_FALSE(ret);
}

TEST(SHT3XNoSetup, IsAtLeastOneAlertPendingTrue)
{
    uint16_t status_reg_val = 0x8000;
    bool ret = sht3x_is_at_least_one_alert_pending(status_reg_val);
    CHECK_TRUE(ret);
}

TEST(SHT3XNoSetup, IsAtLeastOneAlertPendingFalse)
{
    uint16_t status_reg_val = 0x7FFF;
    bool ret = sht3x_is_at_least_one_alert_pending(status_reg_val);
    CHECK_FALSE(ret);
}
