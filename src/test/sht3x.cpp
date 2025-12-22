#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

#define SHT3X_TEST_DEFAULT_I2C_ADDR 0x44
#define SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD 0.01

/* To return from mock_sht3x_get_instance_memory */
static struct SHT3XStruct instance_memory;

static SHT3X sht3x;
/* Init cfg used in all tests. It is populated in the setup before each test with default values. The test has the
 * option to adjust the values before calling sht3x_create. */
static SHT3XInitConfig init_cfg;

/* Populated by mock object whenever mock_sht3x_i2c_write is called */
static SHT3X_I2CTransactionCompleteCb i2c_write_complete_cb;
static void *i2c_write_complete_cb_user_data;

/* Populated by mock object whenever mock_sht3x_i2c_read is called */
static SHT3X_I2CTransactionCompleteCb i2c_read_complete_cb;
static void *i2c_read_complete_cb_user_data;

/* Populated by mock object whenever mock_sht3x_start_timer is called */
static SHT3XTimerExpiredCb timer_expired_cb;
static void *timer_expired_cb_user_data;

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

        /* Reset all values populated by mock object */
        i2c_write_complete_cb = NULL;
        i2c_write_complete_cb_user_data = NULL;
        i2c_read_complete_cb = NULL;
        i2c_read_complete_cb_user_data = NULL;
        timer_expired_cb = NULL;
        timer_expired_cb_user_data = NULL;

        /* Pass pointers so that the mock object populates them with callbacks and user data, so that the test can simulate
        calling these callbacks. */
        mock().setData("i2cWriteCompleteCb", (void *)&i2c_write_complete_cb);
        mock().setData("i2cWriteCompleteCbUserData", &i2c_write_complete_cb_user_data);
        mock().setData("i2cReadCompleteCb", (void *)&i2c_read_complete_cb);
        mock().setData("i2cReadCompleteCbUserData", &i2c_read_complete_cb_user_data);
        mock().setData("timerExpiredCb", (void *)&timer_expired_cb);
        mock().setData("timerExpiredCbUserData", &timer_expired_cb_user_data);

        /* Reset values populated whenever sht3x_meas_complete_cb gets called */
        meas_complete_cb_call_count = 0;
        meas_complete_cb_result_code = 0xFF; /* 0 is a valid code, reset to an invalid code */
        memset(&meas_complete_cb_meas, 0, sizeof(SHT3XMeasurement));
        meas_complete_cb_user_data = NULL;

        sht3x = NULL;
        memset(&init_cfg, 0, sizeof(SHT3XInitConfig));

        /* Test should call sht3x_create at the beginning, which will call this mock */
        mock()
            .expectOneCall("mock_sht3x_get_instance_memory")
            .withParameter("user_data", (void *)NULL)
            .andReturnValue((void *)&instance_memory);

        /* Populate init cfg with default values*/
        init_cfg.get_instance_memory = mock_sht3x_get_instance_memory;
        init_cfg.get_instance_memory_user_data = NULL;
        init_cfg.i2c_write = mock_sht3x_i2c_write;
        init_cfg.i2c_read = mock_sht3x_i2c_read;
        init_cfg.start_timer = mock_sht3x_start_timer;
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
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x23;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* Simulate I2C address NACK */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailBusError)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x78;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* Simulate I2C bus error */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailMeasCompleteCbNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH,
                                                    SHT3X_CLOCK_STRETCHING_DISABLED, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* Simulate I2C write failure */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_write_complete_cb_user_data);

    /* Nothing to check, this test makes sure that program does not crash when meas_complete_cb is NULL */
}

TEST(SHT3X, ReadSingleShotMeasI2cReadFailAddressNack)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 16).ignoreOtherParameters();
    /* Do not write anything to the "data" output parameter, because this transaction fails */
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withParameter("length", 6)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x53;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read failure */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadSingleShotMeasI2cReadFailBusError)
{
    uint8_t i2c_addr = 0x44;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 16).ignoreOtherParameters();
    /* Do not write anything to the "data" output parameter, because this transaction fails */
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withParameter("length", 6)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xAA;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read failure */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadSingleShotMeasI2cReadFailMeasCompleteCbNull)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 16).ignoreOtherParameters();
    /* Do not write anything to the "data" output parameter, because this transaction fails */
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withParameter("length", 6)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH,
                                                    SHT3X_CLOCK_STRETCHING_DISABLED, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read failure */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_read_complete_cb_user_data);

    /* Nothing to check, this test makes sure that program does not crash when meas_complete_cb is NULL */
}

TEST(SHT3X, ReadSingleShotMeasSuccess)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 16).ignoreOtherParameters();
    /* Taken from real device output, temp 22.31 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x76, 0x53, 0x73, 0xD3, 0xC0};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x29;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.31, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(45.24, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasSuccess2)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 16).ignoreOtherParameters();
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x38;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasSuccessMeasCompleteCbNull)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 16).ignoreOtherParameters();
    /* Taken from real device output, temp 22.31 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x76, 0x53, 0x73, 0xD3, 0xC0};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH,
                                                    SHT3X_CLOCK_STRETCHING_DISABLED, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    /* Nothing to check, this test makes sure that program does not crash when meas_complete_cb is NULL */
}

TEST(SHT3X, ReadSingleShotMeasSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xBB;
    uint8_t rc = sht3x_read_single_shot_measurement(NULL, SHT3X_MEAS_REPEATABILITY_HIGH,
                                                    SHT3X_CLOCK_STRETCHING_DISABLED, sht3x_meas_complete_cb, user_data);
    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, ReadSingleShotMeasMediumRepeatability)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with medium repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0B};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 7).ignoreOtherParameters();
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xDC;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasLowRepeatability)
{
    uint8_t i2c_addr = 0x45;
    init_cfg.i2c_addr = i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with low repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x16};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 5).ignoreOtherParameters();
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", i2c_addr)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xAF;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasHighRepeatabilityClkStretch)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with high repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x06};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x41;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_ENABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasMediumRepeatabilityClkStretch)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x41;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_CLOCK_STRETCHING_ENABLED,
                                           sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasLowRepeatabilityClkStretch)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Single shot meas with low repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x10};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();
    mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x49;
    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_CLOCK_STRETCHING_ENABLED,
                                                    sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);
    /* Simulate SHT3X timer expiry */
    timer_expired_cb(timer_expired_cb_user_data);
    /* I2C read success */
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadSingleShotMeasInvalidRepeatability)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t invalid_repeatability = 0xFF;
    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, invalid_repeatability, SHT3X_CLOCK_STRETCHING_DISABLED,
                                                    sht3x_meas_complete_cb, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

TEST(SHT3X, ReadSingleShotMeasInvalidClockStretching)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t invalid_clock_stretching = 0xFA;
    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_LOW, invalid_clock_stretching,
                                                    sht3x_meas_complete_cb, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}
