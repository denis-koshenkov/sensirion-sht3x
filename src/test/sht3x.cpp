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

static size_t complete_cb_call_count;
static uint8_t complete_cb_result_code;
static void *complete_cb_user_data;

static void sht3x_meas_complete_cb(uint8_t result_code, SHT3XMeasurement *meas, void *user_data)
{
    meas_complete_cb_call_count++;
    meas_complete_cb_result_code = result_code;
    if (meas) {
        memcpy(&meas_complete_cb_meas, meas, sizeof(SHT3XMeasurement));
    }
    meas_complete_cb_user_data = user_data;
}

static void sht3x_complete_cb(uint8_t result_code, void *user_data)
{
    complete_cb_call_count++;
    complete_cb_result_code = result_code;
    complete_cb_user_data = user_data;
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

        /* Reset values populated whenever sht3x_complete_cb gets called */
        complete_cb_call_count = 0;
        complete_cb_result_code = 0xFF; /* 0 is a valid code, reset to an invalid code */
        complete_cb_user_data = NULL;
        
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
        .withParameter("length", 5)
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
        .withParameter("length", 5)
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
        .withParameter("length", 5)
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
    uint8_t i2c_read_data[] = {0x62, 0x76, 0x53, 0x73, 0xD3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    uint8_t i2c_read_data[] = {0x62, 0x76, 0x53, 0x73, 0xD3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
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

TEST(SHT3X, SingleShotMeasCmdHighRepeatabilityAddressNack)
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

    void *complete_cb_user_data_expected = (void *)0x37;
    uint8_t rc =
        sht3x_send_single_shot_measurement_cmd(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                               sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write failure */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, SingleShotMeasCmdHighRepeatabilityBusError)
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

    void *complete_cb_user_data_expected = (void *)0x37;
    uint8_t rc =
        sht3x_send_single_shot_measurement_cmd(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                               sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write failure */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, SingleShotMeasCmdNoCb)
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

    uint8_t rc = sht3x_send_single_shot_measurement_cmd(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH,
                                                        SHT3X_CLOCK_STRETCHING_DISABLED, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);

    /* Nothing to check, this test makes sure that program does not crash when complete_cb is NULL */
}

/* Generic function to test success scenario of single shot measurement command. */
static void test_single_shot_meas_cmd_success(uint8_t repeatability, uint8_t clock_stretching,
                                              uint8_t *expected_i2c_write_data, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", expected_i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_send_single_shot_measurement_cmd(sht3x, repeatability, clock_stretching, sht3x_complete_cb,
                                                        complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    /* I2C write success */
    i2c_write_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, SingleShotMeasCmdHighRepeatability)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    void *complete_cb_user_data_expected = (void *)0x83;
    test_single_shot_meas_cmd_success(SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED, i2c_write_data,
                                      complete_cb_user_data_expected);
}

TEST(SHT3X, SingleShotMeasCmdMediumRepeatability)
{
    /* Single shot meas with medium repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0B};
    void *complete_cb_user_data_expected = (void *)0xA9;
    test_single_shot_meas_cmd_success(SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_CLOCK_STRETCHING_DISABLED, i2c_write_data,
                                      complete_cb_user_data_expected);
}

TEST(SHT3X, SingleShotMeasCmdLowRepeatability)
{
    /* Single shot meas with low repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x16};
    void *complete_cb_user_data_expected = (void *)0xBF;
    test_single_shot_meas_cmd_success(SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_CLOCK_STRETCHING_DISABLED, i2c_write_data,
                                      complete_cb_user_data_expected);
}

TEST(SHT3X, SingleShotMeasCmdHighRepeatabilityClkStretching)
{
    /* Single shot meas with high repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x06};
    void *complete_cb_user_data_expected = (void *)0x03;
    test_single_shot_meas_cmd_success(SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_ENABLED, i2c_write_data,
                                      complete_cb_user_data_expected);
}

TEST(SHT3X, SingleShotMeasCmdMediumRepeatabilityClkStretching)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    void *complete_cb_user_data_expected = (void *)0x32;
    test_single_shot_meas_cmd_success(SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_CLOCK_STRETCHING_ENABLED, i2c_write_data,
                                      complete_cb_user_data_expected);
}

TEST(SHT3X, SingleShotMeasCmdLowRepeatabilityClkStretching)
{
    /* Single shot meas with low repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x10};
    void *complete_cb_user_data_expected = (void *)0x93;
    test_single_shot_meas_cmd_success(SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_CLOCK_STRETCHING_ENABLED, i2c_write_data,
                                      complete_cb_user_data_expected);
}

TEST(SHT3X, SendSingleShotMeasCmdSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xBC;
    uint8_t rc = sht3x_send_single_shot_measurement_cmd(NULL, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                                                        SHT3X_CLOCK_STRETCHING_ENABLED, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

TEST(SHT3X, SendSingleShotMeasCmdInvalidRepeatability)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t invalid_repeatability = 0xAF;
    void *user_data = (void *)0x2C;
    uint8_t rc = sht3x_send_single_shot_measurement_cmd(sht3x, invalid_repeatability, SHT3X_CLOCK_STRETCHING_ENABLED,
                                                        sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

TEST(SHT3X, SendSingleShotMeasCmdInvalidClkStretching)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t invalid_clock_stretching = 0xF0;
    void *user_data = (void *)0x66;
    uint8_t rc = sht3x_send_single_shot_measurement_cmd(sht3x, SHT3X_MEAS_REPEATABILITY_LOW, invalid_clock_stretching,
                                                        sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

TEST(SHT3X, ReadMeasTempAndHumNoCrc)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    /* Expecting to read 5 bytes, because the 6th byte is the humidity CRC, and we are not verifying humidity CRC. */
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x8A;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb,
                                        meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadMeasAddressNack)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Not writing anything to the "data" output parameter, because we are simulating a NACK after the address byte -
     * this means nothing will be written to the I2C read buffer. */
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withParameter("length", 5)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xA1;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb,
                                        meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_NO_DATA, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadMeasBusError)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Not writing anything to the "data" output parameter, because we are simulating a bus error. */
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withParameter("length", 5)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x18;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb,
                                        meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_IO_ERR, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadMeasCbNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    /* Nothing to check, this test makes sure that program does not crash when meas_complete_cb is NULL */
}

TEST(SHT3X, ReadMeasTemp)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius */
    uint8_t i2c_read_data[] = {0x62, 0x60};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xDA;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb,
                                        meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadMeasHum)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, humidity 44.8 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xAD;
    uint8_t rc =
        sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

static void test_read_meas_invalid_flags(uint8_t flags)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);
    void *meas_complete_cb_user_data_expected = (void *)0xAD;
    uint8_t rc = sht3x_read_measurement(sht3x, flags, sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

TEST(SHT3X, ReadMeasFlags0)
{
    test_read_meas_invalid_flags(0);
}

TEST(SHT3X, ReadMeasFlagsCrcHum)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadMeasFlagsCrcTemp)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_VERIFY_CRC_TEMP);
}

TEST(SHT3X, ReadMeasFlagsCrcTempHum)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadMeasHumCrcHum)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x24;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM, sht3x_meas_complete_cb,
                                        meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

static void test_read_meas_wrong_crc(uint8_t *i2c_read_data, size_t length, uint8_t flags,
                                     void *meas_complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", length)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_read_measurement(sht3x, flags, sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_CRC_MISMATCH, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
}

TEST(SHT3X, ReadMeasHumCrcHumWrongCrc)
{
    /* Taken from real device output, temp 22.25 Celsius, humidity 45.24 RH%. Last byte is modified to yield incorrect
     * CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x5A};
    void *meas_complete_cb_user_data_expected = (void *)0x11;
    test_read_meas_wrong_crc(i2c_read_data, 6, SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasFlagsHumCrcTemp)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP);
}

TEST(SHT3X, ReadMeasFlagsHumCrcTempCrcHum)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadMeasFlagsTempCrcHum)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadMeasTempCrcTemp)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 3)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0x99;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP,
                                        sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadMeasTempWrongCrc)
{
    /* Taken from real device output, temp 22.25 Celsius. Last byte is modified to yield incorrect CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0x12};
    void *meas_complete_cb_user_data_expected = (void *)0x99;
    test_read_meas_wrong_crc(i2c_read_data, 3, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasFlagsTempCrcTempCrcHum)
{
    test_read_meas_invalid_flags(SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadMeasTempHumCrcHum)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xCC;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
                                        sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadMeasTempHumCrcHumWrongCrc)
{
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%, last byte modified to yield wrong CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x42};
    void *meas_complete_cb_user_data_expected = (void *)0xCC;
    test_read_meas_wrong_crc(i2c_read_data, 6, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasTempHumCrcTemp)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 5)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xDD;
    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP,
                                        sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadMeasTempHumCrcTempWrongCrc)
{
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB5, 0x72, 0xB3};
    void *meas_complete_cb_user_data_expected = (void *)0xED;
    test_read_meas_wrong_crc(i2c_read_data, 5, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasTempHumCrcTempCrcHum)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 6)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    void *meas_complete_cb_user_data_expected = (void *)0xDE;
    uint8_t rc = sht3x_read_measurement(
        sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM,
        sht3x_meas_complete_cb, meas_complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, meas_complete_cb_result_code);
    POINTERS_EQUAL(meas_complete_cb_user_data_expected, meas_complete_cb_user_data);
    DOUBLES_EQUAL(22.25, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    DOUBLES_EQUAL(44.80, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
}

TEST(SHT3X, ReadMeasTempHumWrongCrcTempCrcHum)
{
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong
     * temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0x0, 0x72, 0xB3, 0x8F};
    void *meas_complete_cb_user_data_expected = (void *)0xDE;
    test_read_meas_wrong_crc(i2c_read_data, 6,
                             SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                 SHT3X_FLAG_VERIFY_CRC_HUM,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasTempHumCrcTempWrongCrcHum)
{
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Last byte modified to yield wrong
     * humidity CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8E};
    void *meas_complete_cb_user_data_expected = (void *)0x5A;
    test_read_meas_wrong_crc(i2c_read_data, 6,
                             SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                 SHT3X_FLAG_VERIFY_CRC_HUM,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasTempHumWrongCrcTempWrongCrcHum)
{
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third and last bytes are modified to yield
     * wrong temperature and humidity CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xA6, 0x72, 0xB3, 0x8D};
    void *meas_complete_cb_user_data_expected = (void *)0x5A;
    test_read_meas_wrong_crc(i2c_read_data, 6,
                             SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                 SHT3X_FLAG_VERIFY_CRC_HUM,
                             meas_complete_cb_user_data_expected);
}

TEST(SHT3X, ReadMeasSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t rc = sht3x_read_measurement(NULL, SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb, NULL);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

/* Generic function to test success scenario of start_periodic_measurement function. */
static void test_start_periodic_meas(uint8_t i2c_write_rc, uint8_t expected_rc, uint8_t repeatability, uint8_t mps,
                                     uint8_t *expected_i2c_write_data, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", expected_i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc =
        sht3x_start_periodic_measurement(sht3x, repeatability, mps, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, StartPeriodicMeasRepeatHighMpsPoint5)
{
    /* Start periodic data acquisition: high repeatability, 0.5 mps */
    uint8_t i2c_write_data[] = {0x20, 0x32};
    void *complete_cb_user_data_expected = (void *)0xB2;
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_HIGH,
                             SHT3X_MPS_0_5, i2c_write_data, complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasRepeatMediumMpsPoint5)
{
    /* Start periodic data acquisition: medium repeatability, 0.5 mps */
    uint8_t i2c_write_data[] = {0x20, 0x24};
    void *complete_cb_user_data_expected = (void *)0xB1;
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                             SHT3X_MPS_0_5, i2c_write_data, complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasRepeatLowMpsPoint5)
{
    /* Start periodic data acquisition: low repeatability, 0.5 mps */
    uint8_t i2c_write_data[] = {0x20, 0x2F};
    void *complete_cb_user_data_expected = (void *)0x1A;
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_LOW,
                             SHT3X_MPS_0_5, i2c_write_data, complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasRepeatHighMps1)
{
    /* Start periodic data acquisition: high repeatability, 1 mps */
    uint8_t i2c_write_data[] = {0x21, 0x30};
    void *complete_cb_user_data_expected = (void *)0x3A;
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_MPS_1,
                             i2c_write_data, complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasRepeatMediumMps1)
{
    /* Start periodic data acquisition: medium repeatability, 1 mps */
    uint8_t i2c_write_data[] = {0x21, 0x26};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                             SHT3X_MPS_1, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatLowMps1)
{
    /* Start periodic data acquisition: low repeatability, 1 mps */
    uint8_t i2c_write_data[] = {0x21, 0x2D};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_MPS_1,
                             i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatHighMps2)
{
    /* Start periodic data acquisition: high repeatability, 2 mps */
    uint8_t i2c_write_data[] = {0x22, 0x36};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_MPS_2,
                             i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatMediumMps2)
{
    /* Start periodic data acquisition: medium repeatability, 2 mps */
    uint8_t i2c_write_data[] = {0x22, 0x20};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                             SHT3X_MPS_2, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatLowMps2)
{
    /* Start periodic data acquisition: low repeatability, 2 mps */
    uint8_t i2c_write_data[] = {0x22, 0x2B};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_MPS_2,
                             i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatHighMps4)
{
    /* Start periodic data acquisition: high repeatability, 4 mps */
    uint8_t i2c_write_data[] = {0x23, 0x34};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_MPS_4,
                             i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatMediumMps4)
{
    /* Start periodic data acquisition: medium repeatability, 4 mps */
    uint8_t i2c_write_data[] = {0x23, 0x22};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                             SHT3X_MPS_4, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatLowMps4)
{
    /* Start periodic data acquisition: low repeatability, 4 mps */
    uint8_t i2c_write_data[] = {0x23, 0x29};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_MPS_4,
                             i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatHighMps10)
{
    /* Start periodic data acquisition: high repeatability, 10 mps */
    uint8_t i2c_write_data[] = {0x27, 0x37};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_HIGH,
                             SHT3X_MPS_10, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatMediumMps10)
{
    /* Start periodic data acquisition: medium repeatability, 10 mps */
    uint8_t i2c_write_data[] = {0x27, 0x21};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                             SHT3X_MPS_10, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasRepeatLowMps10)
{
    /* Start periodic data acquisition: low repeatability, 10 mps */
    uint8_t i2c_write_data[] = {0x27, 0x2A};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_MPS_10,
                             i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasAddressNack)
{
    /* Start periodic data acquisition: low repeatability, 10 mps */
    uint8_t i2c_write_data[] = {0x27, 0x2A};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR, SHT3X_MEAS_REPEATABILITY_LOW,
                             SHT3X_MPS_10, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasBusError)
{
    /* Start periodic data acquisition: medium repeatability, 4 mps */
    uint8_t i2c_write_data[] = {0x23, 0x22};
    test_start_periodic_meas(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, SHT3X_MEAS_REPEATABILITY_MEDIUM,
                             SHT3X_MPS_4, i2c_write_data, NULL);
}

TEST(SHT3X, StartPeriodicMeasSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x69;
    uint8_t rc = sht3x_start_periodic_measurement(NULL, SHT3X_MEAS_REPEATABILITY_LOW, SHT3X_MPS_10, sht3x_complete_cb,
                                                  user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

TEST(SHT3X, StartPeriodicMeasInvalidRepeatability)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x92;
    uint8_t invalid_repeatability = 0xF9;
    uint8_t rc =
        sht3x_start_periodic_measurement(sht3x, invalid_repeatability, SHT3X_MPS_2, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

TEST(SHT3X, StartPeriodicMeasInvalidMps)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x8C;
    uint8_t invalid_mps = 0xFA;
    uint8_t rc = sht3x_start_periodic_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, invalid_mps, sht3x_complete_cb,
                                                  user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_start_periodic_meas_art(uint8_t i2c_write_rc, uint8_t expected_rc,
                                         void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* ART command */
    uint8_t i2c_write_data[] = {0x2B, 0x32};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_start_periodic_measurement_art(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, StartPeriodicMeasArt)
{
    void *complete_cb_user_data_expected = (void *)0x88;
    test_start_periodic_meas_art(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasArtAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0x58;
    test_start_periodic_meas_art(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR,
                                 complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasArtBusError)
{
    void *complete_cb_user_data_expected = (void *)0x15;
    test_start_periodic_meas_art(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR,
                                 complete_cb_user_data_expected);
}

TEST(SHT3X, StartPeriodicMeasArtSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x19;
    uint8_t rc = sht3x_start_periodic_measurement_art(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_stop_periodic_meas(uint8_t i2c_write_rc, uint8_t expected_rc, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Stop periodic meas command */
    uint8_t i2c_write_data[] = {0x30, 0x93};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_stop_periodic_measurement(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, StopPeriodicMeas)
{
    void *complete_cb_user_data_expected = (void *)0x9D;
    test_stop_periodic_meas(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, StopPeriodicMeasAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0x34;
    test_stop_periodic_meas(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR,
                            complete_cb_user_data_expected);
}

TEST(SHT3X, StopPeriodicMeasBusError)
{
    void *complete_cb_user_data_expected = (void *)0xAD;
    test_stop_periodic_meas(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, StopPeriodicMeasSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x39;
    uint8_t rc = sht3x_stop_periodic_measurement(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_soft_reset(uint8_t i2c_write_rc, uint8_t expected_rc, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Soft reset command */
    uint8_t i2c_write_data[] = {0x30, 0xA2};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_soft_reset(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, SoftReset)
{
    void *complete_cb_user_data_expected = (void *)0xD8;
    test_soft_reset(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, SoftResetAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0xDD;
    test_soft_reset(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, SoftResetBusError)
{
    void *complete_cb_user_data_expected = (void *)0xBE;
    test_soft_reset(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, SoftResetSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x44;
    uint8_t rc = sht3x_soft_reset(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_enable_heater(uint8_t i2c_write_rc, uint8_t expected_rc, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Enable heater command */
    uint8_t i2c_write_data[] = {0x30, 0x6D};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_enable_heater(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, EnableHeater)
{
    void *complete_cb_user_data_expected = (void *)0x66;
    test_enable_heater(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, EnableHeaterBusError)
{
    void *complete_cb_user_data_expected = (void *)0x77;
    test_enable_heater(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, EnableHeaterAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0x99;
    test_enable_heater(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, EnableHeaterSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x55;
    uint8_t rc = sht3x_enable_heater(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_disable_heater(uint8_t i2c_write_rc, uint8_t expected_rc, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Disable heater command */
    uint8_t i2c_write_data[] = {0x30, 0x66};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_disable_heater(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, DisableHeater)
{
    void *complete_cb_user_data_expected = (void *)0x43;
    test_disable_heater(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, DisableHeaterAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0x48;
    test_disable_heater(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, DisableHeaterBusError)
{
    void *complete_cb_user_data_expected = (void *)0xFF;
    test_disable_heater(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, complete_cb_user_data_expected);
}

TEST(SHT3X, DisableHeaterSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xDD;
    uint8_t rc = sht3x_disable_heater(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_clear_status_register(uint8_t i2c_write_rc, uint8_t expected_rc, void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Clear status register command */
    uint8_t i2c_write_data[] = {0x30, 0x41};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_clear_status_register(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, ClearStatusRegister)
{
    void *complete_cb_user_data_expected = (void *)0xAA;
    test_clear_status_register(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, ClearStatusRegisterAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0xCA;
    test_clear_status_register(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR,
                               complete_cb_user_data_expected);
}

TEST(SHT3X, ClearStatusRegisterBusError)
{
    void *complete_cb_user_data_expected = (void *)0xAC;
    test_clear_status_register(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR,
                               complete_cb_user_data_expected);
}

TEST(SHT3X, ClearStatusRegisterSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0x11;
    uint8_t rc = sht3x_clear_status_register(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_fetch_periodic_measurement_data(uint8_t i2c_write_rc, uint8_t expected_rc,
                                                 void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Fetch data command */
    uint8_t i2c_write_data[] = {0xE0, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_fetch_periodic_measurement_data(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, FetchPeriodicMeasData)
{
    void *complete_cb_user_data_expected = (void *)0xAB;
    test_fetch_periodic_measurement_data(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK,
                                         complete_cb_user_data_expected);
}

TEST(SHT3X, FetchPeriodicMeasDataAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0xAC;
    test_fetch_periodic_measurement_data(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR,
                                         complete_cb_user_data_expected);
}

TEST(SHT3X, FetchPeriodicMeasDataBusError)
{
    void *complete_cb_user_data_expected = (void *)0xAD;
    test_fetch_periodic_measurement_data(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR,
                                         complete_cb_user_data_expected);
}

TEST(SHT3X, FetchPeriodicMeasDataSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xAE;
    uint8_t rc = sht3x_fetch_periodic_measurement_data(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

/**
 * @brief Test read periodic measurement function.
 *
 * @param flags Flags to pass to sht3x_read_periodic_measurement.
 * @param i2c_write_rc Return code to return from i2c_write for fetch data command.
 * @param i2c_read_data Data to write to the "data" parameter of i2c_read for measurement readout command.
 * @param i2c_data_len Number of bytes in @p i2c_read_data. It is also used to check that "length" parameter to
 * mock_sht3x_i2c_read is equal to this value.
 * @param i2c_read_rc Return code to return from i2c_read for measurement readout command.
 * @param expected_complete_cb_rc Return code that the test expects the meas_complete_cb to be called with.
 * @param complete_cb_user_data_expected User data expected to be passed to meas_complete_cb.
 * @param temperature Expected temperature value in the meas->temperature of meas_complete_cb. If NULL, then expected
 * temperature value is not verified.
 * @param humidity Expected humidity value in the meas->humidity of meas_complete_cb. If NULL, then expected
 * humidity value is not verified.
 */
static void test_read_periodic_measurement(uint8_t flags, uint8_t i2c_write_rc, uint8_t *i2c_read_data,
                                           size_t i2c_data_len, uint8_t i2c_read_rc, uint8_t expected_complete_cb_rc,
                                           void *complete_cb_user_data_expected, const float *const temperature,
                                           const float *const humidity)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Fetch data command */
    uint8_t i2c_write_data[] = {0xE0, 0x0};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .ignoreOtherParameters();
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
        if (i2c_read_data != NULL) {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withOutputParameterReturning("data", i2c_read_data, i2c_data_len)
                .withParameter("length", i2c_data_len)
                .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
                .ignoreOtherParameters();
        } else {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withParameter("length", i2c_data_len)
                .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
                .ignoreOtherParameters();
        }
    }

    uint8_t rc = sht3x_read_periodic_measurement(sht3x, flags, sht3x_meas_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        timer_expired_cb(timer_expired_cb_user_data);
        i2c_read_complete_cb(i2c_read_rc, i2c_read_complete_cb_user_data);
    }

    CHECK_EQUAL(1, meas_complete_cb_call_count);
    CHECK_EQUAL(expected_complete_cb_rc, meas_complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, meas_complete_cb_user_data);
    if (temperature) {
        DOUBLES_EQUAL(*temperature, meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    }
    if (humidity) {
        DOUBLES_EQUAL(*humidity, meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
    }
}

static void test_read_periodic_measurement_invalid_flags(uint8_t flags)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t rc = sht3x_read_periodic_measurement(sht3x, flags, sht3x_meas_complete_cb, (void *)0x55);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

TEST(SHT3X, ReadPeriodicMeasFetchDataAddressNack)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK;
    /* Don't care */
    uint8_t *i2c_read_data = NULL;
    size_t i2c_data_len = 0;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK;
    /* Care again */
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR;
    void *complete_cb_user_data_expected = (void *)0xAF;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasFetchDataBusError)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR;
    /* Don't care */
    uint8_t *i2c_read_data = NULL;
    size_t i2c_data_len = 0;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK;
    /* Care again */
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR;
    void *complete_cb_user_data_expected = (void *)0xB0;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasReadMeasAddressNack)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* No data since I2C read results in address NACK */
    uint8_t *i2c_read_data = NULL;
    size_t i2c_data_len = 5;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_NO_DATA;
    void *complete_cb_user_data_expected = (void *)0xB1;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasReadMeasBusError)
{
    uint8_t flags = SHT3X_FLAG_READ_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    size_t i2c_data_len = 5;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR;
    void *complete_cb_user_data_expected = (void *)0xB2;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasFlags0)
{
    test_read_periodic_measurement_invalid_flags(0);
}

TEST(SHT3X, ReadPeriodicMeasCrcHum)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadPeriodicMeasCrcTemp)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_VERIFY_CRC_TEMP);
}

TEST(SHT3X, ReadPeriodicMeasCrcTempCrcHum)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadPeriodicMeasHum)
{
    uint8_t flags = SHT3X_FLAG_READ_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    size_t i2c_data_len = 5;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xB3;
    float *temperature = NULL;
    float humidity = 44.80f;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, &humidity);
}

TEST(SHT3X, ReadPeriodicMeasHumCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xB4;
    float *temperature = NULL;
    float humidity = 44.80f;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, &humidity);
}

TEST(SHT3X, ReadPeriodicMeasHumWrongCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%, last byte modified to yield wrong CRC */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0xAF};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xB5;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasHumCrcTemp)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP);
}

TEST(SHT3X, ReadPeriodicMeasHumCrcTempCrcHum)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                                 SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadPeriodicMeasTemp)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius */
    uint8_t i2c_read_data[] = {0x62, 0x60};
    size_t i2c_data_len = 2;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xB6;
    float temperature = 22.25f;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, &temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempCrcHum)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadPeriodicMeasTempCrcTemp)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6};
    size_t i2c_data_len = 3;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xB7;
    float temperature = 22.25f;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, &temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempWrongCrcTemp)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, last byte modified to yield wrong CRC */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xA9};
    size_t i2c_data_len = 3;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xB8;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempCrcTempCrcHum)
{
    test_read_periodic_measurement_invalid_flags(SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                                 SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadPeriodicMeasTempHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    size_t i2c_data_len = 5;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xB9;
    float temperature = 22.25f;
    float humidity = 44.80f;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, &temperature, &humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong
     * temperature CRC, that should not make the test fail since we are not verifying temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB9, 0x72, 0xB3, 0x8F};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xBA;
    float temperature = 22.25f;
    float humidity = 44.80f;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, &temperature, &humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumWrongCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Last byte modified to yield wrong humidity
     * CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0xFF};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xBB;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumCrcTemp)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    size_t i2c_data_len = 5;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xBC;
    float temperature = 22.25f;
    float humidity = 44.80f;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, &temperature, &humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumWrongCrcTemp)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong
     * temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xA5, 0x72, 0xB3};
    size_t i2c_data_len = 5;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xBD;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumCrcTempCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_OK;
    void *complete_cb_user_data_expected = (void *)0xBE;
    float temperature = 22.25f;
    float humidity = 44.80f;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, &temperature, &humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumWrongCrcTempCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong
     * temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0x88, 0x72, 0xB3, 0x8F};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xBF;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumCrcTempWrongCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Last byte modified to yield wrong
     * humidity CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x81};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xC0;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasTempHumWrongCrcTempWrongCrcHum)
{
    uint8_t flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM;
    uint8_t i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK;
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third and last bytes are modified to yield
     * wrong both temperature and humidity CRCs. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0x23, 0x72, 0xB3, 0x45};
    size_t i2c_data_len = 6;
    uint8_t i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK;
    uint8_t complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH;
    void *complete_cb_user_data_expected = (void *)0xC1;
    float *temperature = NULL;
    float *humidity = NULL;
    test_read_periodic_measurement(flags, i2c_write_rc, i2c_read_data, i2c_data_len, i2c_read_rc, complete_cb_rc,
                                   complete_cb_user_data_expected, temperature, humidity);
}

TEST(SHT3X, ReadPeriodicMeasSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xC2;
    uint8_t rc = sht3x_read_periodic_measurement(NULL, SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}
