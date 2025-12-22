#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

#define SHT3X_TEST_DEFAULT_I2C_ADDR 0x44

/* To return from mock_sht3x_get_instance_memory */
static struct SHT3XStruct instance_memory;

static SHT3X sht3x;
/* Init cfg used in all tests. It is populated in the setup before each test with default values. The test has the
 * option to adjust the values before calling sht3x_create. */
static SHT3XInitConfig init_cfg;

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

        sht3x = NULL;
        memset(&init_cfg, 0, sizeof(SHT3XInitConfig));

        /* Create SHT3X instance */
        mock()
            .expectOneCall("mock_sht3x_get_instance_memory")
            .withParameter("user_data", (void *)NULL)
            .andReturnValue((void *)&instance_memory);

        /* Populate init cfg with default values*/
        init_cfg.get_instance_memory = mock_sht3x_get_instance_memory;
        init_cfg.get_instance_memory_user_data = NULL;
        init_cfg.i2c_write = mock_sht3x_i2c_write;
        init_cfg.i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR;
    }
};
// clang-format on

TEST(SHT3X, DestroyCallsFreeInstanceMemory)
{
    uint8_t rc = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    void *free_instance_memory_user_data = (void *)0x5;
    mock()
        .expectOneCall("mock_sht3x_free_instance_memory")
        .withParameter("instance_memory", (void *)&instance_memory)
        .withParameter("user_data", free_instance_memory_user_data);

    sht3x_destroy(sht3x, mock_sht3x_free_instance_memory, free_instance_memory_user_data);
}

TEST(SHT3X, DestroyCalledWithFreeInstanceMemoryNullDoesNotCrash)
{
    uint8_t rc = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    sht3x_destroy(sht3x, NULL, NULL);
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailAddressNack)
{
    uint8_t rc = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    /* Single shot meas with high repeatability and clock stretching disabled */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                       sht3x_meas_complete_cb, NULL);
    /* Simulate I2C address NACK */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(NULL, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailBusError)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    /* Single shot meas with high repeatability and clock stretching disabled */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                       sht3x_meas_complete_cb, NULL);
    /* Simulate I2C bus error */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(NULL, meas_complete_cb_user_data);
}
