#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

/* To return from mock_sht3x_get_instance_memory */
static struct SHT3XStruct instance_memory;

/* Populated by mock object whenever mock_sht3x_i2c_write is called */
static SHT3X_I2CTransactionCompleteCb i2c_write_complete_cb;
static void *i2c_write_complete_cb_user_data;

static size_t meas_complete_cb_call_count;
static uint8_t meas_complete_cb_result_code;
static SHT3XMeasurement meas_complete_cb_meas;
static void *meas_complete_cb_user_data;

static void sht3x_meas_complete_cb(uint8_t result_code, SHT3XMeasurement *meas, void *user_data)
{
    meas_complete_cb_call_count++;
    meas_complete_cb_result_code = result_code;
    if (meas) {
        memcpy(&meas_complete_cb_meas, meas, sizeof(SHT3XMeasurement));
    }
    meas_complete_cb_user_data = user_data;
}

// clang-format off
TEST_GROUP(SHT3X)
{
    void setup() {
        /* Order of expected calls is important for these tests. Fail the test if the expected mock calls do not happen
        in the specified order. */
        mock().strictOrder();

        /* Pass pointers so that the mock object populates them with callbacks, so that the test can simulate calling
        these callbacks. */
        mock().setData("i2cWriteCompleteCb", (void *)&i2c_write_complete_cb);
        mock().setData("i2cWriteCompleteCbUserData", &i2c_write_complete_cb_user_data);

        /* Reset values populated whenever sht3x_meas_complete_cb gets called */
        meas_complete_cb_call_count = 0;
        meas_complete_cb_result_code = 0xFF; /* 0 is a valid code, reset to an invalid code */
        memset(&meas_complete_cb_meas, 0, sizeof(SHT3XMeasurement));
        meas_complete_cb_user_data = NULL;
    }
};
// clang-format on

TEST(SHT3X, CreateReturnsInvalidArgIfGetInstMemoryIsNull)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = NULL,
        .get_instance_memory_user_data = (void *)0x1,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, CreateReturnsInvalidArgIfCfgIsNull)
{
    SHT3X sht3x;
    uint8_t rc = sht3x_create(&sht3x, NULL);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, CreateReturnsInvalidArgIfInstanceIsNull)
{
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = (void *)0x1,
    };
    uint8_t rc = sht3x_create(NULL, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, CreateReturnsOutOfMemoryIfGetInstanceMemoryReturnsNull)
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
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OUT_OF_MEMORY, rc);
}

TEST(SHT3X, CreateCallsGetInstanceMemory)
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
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
}

TEST(SHT3X, DestroyCallsFreeInstanceMemory)
{
    /* sht3x_create */
    void *get_instance_memory_user_data = (void *)0x42;
    mock()
        .expectOneCall("mock_sht3x_get_instance_memory")
        .withParameter("user_data", get_instance_memory_user_data)
        .andReturnValue((void *)&instance_memory);
    /* sht3x_destroy */
    void *free_instance_memory_user_data = (void *)0x5;
    mock()
        .expectOneCall("mock_sht3x_free_instance_memory")
        .withParameter("instance_memory", (void *)&instance_memory)
        .withParameter("user_data", free_instance_memory_user_data);

    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = get_instance_memory_user_data,
    };
    uint8_t rc_create = sht3x_create(&sht3x, &cfg);
    sht3x_destroy(sht3x, mock_sht3x_free_instance_memory, free_instance_memory_user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);
}

TEST(SHT3X, DestroyCalledWithFreeInstanceMemoryNullDoesNotCrash)
{
    /* sht3x_create */
    mock()
        .expectOneCall("mock_sht3x_get_instance_memory")
        .withParameter("user_data", (void *)NULL)
        .andReturnValue((void *)&instance_memory);

    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
    };
    uint8_t rc_create = sht3x_create(&sht3x, &cfg);
    sht3x_destroy(sht3x, NULL, NULL);

    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);
}

TEST(SHT3X, ReadSingleShotMeasFirstI2cWriteFail)
{
    uint8_t i2c_addr = 0x44;

    /* sht3x_create */
    mock()
        .expectOneCall("mock_sht3x_get_instance_memory")
        .withParameter("user_data", (void *)NULL)
        .andReturnValue((void *)&instance_memory);
    /* sht3x_read_single_shot_measurement */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL,
        .i2c_write = mock_sht3x_i2c_write,
    };
    uint8_t rc_create = sht3x_create(&sht3x, &cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                       sht3x_meas_complete_cb, NULL);
    /* Simulate I2C transaction failure */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(NULL, meas_complete_cb_user_data);
}
