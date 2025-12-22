#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

/* To return from mock_sht3x_get_instance_memory */
static struct SHT3XStruct instance_memory;

/* These tests are in a separate test group, because they test sht3x_create, including unhappy scenarios. In the
 * other test group, sht3x_create is called in common setup function before each test. These tests need to be able to
 * call sht3x_create themselves. */

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
        .i2c_addr = 0x45,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
}
