#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

#define SHT3X_TEST_DEFAULT_I2C_ADDR 0x44
#define SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD 0.01

/* User data that i2c_write/i2c_read should be invoked with. Passed to SHT3X instance in the init config. */
static void *i2c_write_user_data = (void *)0x42;
static void *i2c_read_user_data = (void *)0xF5;

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

static size_t read_status_reg_complete_cb_call_count;
static uint8_t read_status_reg_complete_cb_result_code;
static uint16_t read_status_reg_complete_cb_reg_val;
static void *read_status_reg_complete_cb_user_data;

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

static void sht3x_read_status_reg_complete_cb(uint8_t result_code, uint16_t reg_val, void *user_data)
{
    read_status_reg_complete_cb_call_count++;
    read_status_reg_complete_cb_result_code = result_code;
    read_status_reg_complete_cb_reg_val = reg_val;
    read_status_reg_complete_cb_user_data = user_data;
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

        /* Reset values populated whenever sht3x_read_status_reg_complete_cb gets called */
        read_status_reg_complete_cb_call_count = 0;
        read_status_reg_complete_cb_result_code = 0xFF; /* 0 is a valid code, reset to an invalid code */
        read_status_reg_complete_cb_reg_val = 0x00FF;
        read_status_reg_complete_cb_user_data = NULL;
        
        sht3x = NULL;
        memset(&init_cfg, 0, sizeof(SHT3XInitConfig));
        memset(&instance_memory, 0, sizeof(struct SHT3XStruct));

        /* Test should call sht3x_create at the beginning, which will call this mock */
        mock()
            .expectOneCall("mock_sht3x_get_instance_memory")
            .withParameter("user_data", (void *)NULL)
            .andReturnValue((void *)&instance_memory);

        /* Populate init cfg with default values*/
        init_cfg.get_instance_memory = mock_sht3x_get_instance_memory;
        init_cfg.get_instance_memory_user_data = NULL;
        init_cfg.i2c_write = mock_sht3x_i2c_write;
        init_cfg.i2c_write_user_data = i2c_write_user_data;
        init_cfg.i2c_read = mock_sht3x_i2c_read;
        init_cfg.i2c_read_user_data = i2c_read_user_data;
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

    uint8_t destroy_rc = sht3x_destroy(sht3x, mock_sht3x_free_instance_memory, free_instance_memory_user_data);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, destroy_rc);
}

TEST(SHT3X, DestroyCalledWithFreeInstanceMemoryNullDoesNotCrash)
{
    uint8_t rc = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    uint8_t destroy_rc = sht3x_destroy(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, destroy_rc);
}

typedef struct {
    uint8_t i2c_addr;
    uint8_t *i2c_write_data;
    uint8_t i2c_write_rc;
    uint32_t timer_period;
    uint8_t *i2c_read_data;
    size_t i2c_data_len;
    uint8_t i2c_read_rc;
    uint8_t repeatability;
    uint8_t clk_stretching;
    uint8_t flags;
    uint8_t expected_complete_cb_rc;
    void *complete_cb_user_data_expected;
    float *temperature;
    float *humidity;
    SHT3XMeasCompleteCb meas_complete_cb;
} ReadSingleShotMeasTestCfg;

static void test_read_single_shot_measurement(const ReadSingleShotMeasTestCfg *const cfg)
{
    if (!cfg || !(cfg->i2c_write_data)) {
        FAIL_TEST("Invalid cfg");
    }

    init_cfg.i2c_addr = cfg->i2c_addr;
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", cfg->i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", cfg->i2c_addr)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (cfg->i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock()
            .expectOneCall("mock_sht3x_start_timer")
            .withParameter("duration_ms", cfg->timer_period)
            .ignoreOtherParameters();
        if (cfg->i2c_read_data != NULL) {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withOutputParameterReturning("data", cfg->i2c_read_data, cfg->i2c_data_len)
                .withParameter("length", cfg->i2c_data_len)
                .withParameter("i2c_addr", cfg->i2c_addr)
                .withParameter("user_data", i2c_read_user_data)
                .ignoreOtherParameters();
        } else {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withParameter("length", cfg->i2c_data_len)
                .withParameter("i2c_addr", cfg->i2c_addr)
                .withParameter("user_data", i2c_read_user_data)
                .ignoreOtherParameters();
        }
    }

    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, cfg->repeatability, cfg->clk_stretching, cfg->flags,
                                                    cfg->meas_complete_cb, cfg->complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(cfg->i2c_write_rc, i2c_write_complete_cb_user_data);
    if (cfg->i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        timer_expired_cb(timer_expired_cb_user_data);
        i2c_read_complete_cb(cfg->i2c_read_rc, i2c_read_complete_cb_user_data);
    }

    if (cfg->meas_complete_cb) {
        CHECK_EQUAL(1, meas_complete_cb_call_count);
        CHECK_EQUAL(cfg->expected_complete_cb_rc, meas_complete_cb_result_code);
        POINTERS_EQUAL(cfg->complete_cb_user_data_expected, meas_complete_cb_user_data);
        if (cfg->temperature) {
            DOUBLES_EQUAL(*(cfg->temperature), meas_complete_cb_meas.temperature, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
        }
        if (cfg->humidity) {
            DOUBLES_EQUAL(*(cfg->humidity), meas_complete_cb_meas.humidity, SHT3X_TEST_DOUBLES_EQUAL_THRESHOLD);
        }
    } else {
        /* meas_complete_cb is NULL, this test ensures that the program does not crash in that scenario */
    }
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailAddressNack)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
        /* Don't care */
        .timer_period = 0,
        .i2c_read_data = NULL,
        .i2c_data_len = 0,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        /* Care */
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .complete_cb_user_data_expected = (void *)0x23,
        .temperature = NULL,
        .humidity = NULL,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailBusError)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR,
        /* Don't care */
        .timer_period = 0,
        .i2c_read_data = NULL,
        .i2c_data_len = 0,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        /* Care */
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .complete_cb_user_data_expected = (void *)0x78,
        .temperature = NULL,
        .humidity = NULL,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasI2cWriteFailMeasCompleteCbNull)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR,
        /* Don't care */
        .timer_period = 0,
        .i2c_read_data = NULL,
        .i2c_data_len = 0,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        /* Care */
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        /* meas_complete_cb is NULL, so these will not be checked*/
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .complete_cb_user_data_expected = NULL,
        .temperature = NULL,
        .humidity = NULL,
        .meas_complete_cb = NULL,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasI2cReadFailAddressNack)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        /* Do not write anything to the "data" output parameter, because I2C read fails */
        .i2c_read_data = NULL,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .complete_cb_user_data_expected = (void *)0x53,
        .temperature = NULL,
        .humidity = NULL,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasI2cReadFailBusError)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        /* Do not write anything to the "data" output parameter, because I2C read fails */
        .i2c_read_data = NULL,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .complete_cb_user_data_expected = (void *)0xAA,
        .temperature = NULL,
        .humidity = NULL,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasI2cReadFailMeasCompleteCbNull)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        .i2c_read_data = NULL,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        /* meas_complete_cb is NULL, so these will not be checked*/
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .complete_cb_user_data_expected = NULL,
        .temperature = NULL,
        .humidity = NULL,
        .meas_complete_cb = NULL,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasSuccess)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    /* Taken from real device output, temp 22.31 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x76, 0x53, 0x73, 0xD3};
    float temperature = 22.31f;
    float humidity = 45.24f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0x29,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasSuccess2)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0x38,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasSuccessMeasCompleteCbNull)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    /* Taken from real device output, temp 22.31 Celsius, humidity 45.24 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x76, 0x53, 0x73, 0xD3};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        /* meas_complete_cb is NULL, so these will not be checked */
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = NULL,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = NULL,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xBB;
    uint8_t rc = sht3x_read_single_shot_measurement(
        NULL, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
        SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb, user_data);
    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, ReadSingleShotMeasMediumRepeatability)
{
    /* Single shot meas with medium repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0B};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 7,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xDC,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasLowRepeatability)
{
    /* Single shot meas with low repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x16};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 5,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_LOW,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xAF,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasHighRepeatabilityClkStretch)
{
    /* Single shot meas with high repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x06};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0x41,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasMediumRepeatabilityClkStretch)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0x43,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasLowRepeatabilityClkStretch)
{
    /* Single shot meas with low repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x10};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_LOW,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0x49,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasInvalidRepeatability)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t invalid_repeatability = 0xFF;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, invalid_repeatability, SHT3X_CLOCK_STRETCHING_DISABLED,
                                           SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

TEST(SHT3X, ReadSingleShotMeasInvalidClockStretching)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t invalid_clock_stretching = 0xFA;
    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_LOW, invalid_clock_stretching,
                                           SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, sht3x_meas_complete_cb, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

static void test_read_single_shot_measurement_invalid_flags(uint8_t flags)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t rc =
        sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_CLOCK_STRETCHING_ENABLED,
                                           flags, sht3x_meas_complete_cb, (void *)0x55);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
}

TEST(SHT3X, ReadSingleShotMeasFlags0)
{
    test_read_single_shot_measurement_invalid_flags(0);
}

TEST(SHT3X, ReadSingleShotMeasCrcHum)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadSingleShotMeasCrcTemp)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_VERIFY_CRC_TEMP);
}

TEST(SHT3X, ReadSingleShotMeasCrcTempCrcHum)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadSingleShotMeasHum)
{
    /* Single shot meas with low repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x10};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float *temperature = NULL;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_LOW,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xC3,
        .temperature = temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasHumCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    float *temperature = NULL;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xC4,
        .temperature = temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasHumWrongCrcHum)
{
    /* Single shot meas with high repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x06};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Last byte modify to yield wrong humidity
     * CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0xDD};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xC5,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasHumCrcTemp)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP);
}

TEST(SHT3X, ReadSingleShotMeasHumCrcTempCrcHum)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                                    SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadSingleShotMeasTemp)
{
    /* Single shot meas with low repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x16};
    /* Taken from real device output, temp 22.25 Celsius */
    uint8_t i2c_read_data[] = {0x62, 0x60};
    float temperature = 22.25;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 5,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 2,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_LOW,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xC6,
        .temperature = &temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempCrcHum)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadSingleShotMeasTempCrcTemp)
{
    /* Single shot meas with medium repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0B};
    /* Taken from real device output, temp 22.25 Celsius */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6};
    float temperature = 22.25f;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 7,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 3,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xC7,
        .temperature = &temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempWrongCrcTemp)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    /* Taken from real device output, temp 22.25 Celsius. Third byte modified to yield wrong temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xCC};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 16,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 3,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_HIGH,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xC8,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempCrcTempCrcHum)
{
    test_read_single_shot_measurement_invalid_flags(SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_VERIFY_CRC_TEMP |
                                                    SHT3X_FLAG_VERIFY_CRC_HUM);
}

TEST(SHT3X, ReadSingleShotMeasTempHum)
{
    /* Single shot meas with low repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x16};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 5,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_LOW,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_DISABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xC9,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xCA,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumWrongCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Last byte modified to yield wrong humidity
     * CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8E};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xCB,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumCrcTemp)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xCC,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumWrongCrcTemp)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong
     * temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xBA, 0x72, 0xB3};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = SHT3X_TEST_DEFAULT_I2C_ADDR,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 5,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xCD,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumCrcTempCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH% */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0x8F};
    float temperature = 22.25f;
    float humidity = 44.80f;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .complete_cb_user_data_expected = (void *)0xCE,
        .temperature = &temperature,
        .humidity = &humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumWrongCrcTempCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third byte modified to yield wrong
     * temperature CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xFF, 0x72, 0xB3, 0x8F};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xCF,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumCrcTempWrongCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Last byte modified to yield wrong humidity
     * CRC. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0xB6, 0x72, 0xB3, 0xFF};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xD0,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
}

TEST(SHT3X, ReadSingleShotMeasTempHumWrongCrcTempWrongCrcHum)
{
    /* Single shot meas with medium repeatability and clock stretching enabled command */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    /* Taken from real device output, temp 22.25 Celsius, humidity 44.80 RH%. Third and last bytes are modified to yield
     * wrong both temperature and humidity CRCs. */
    uint8_t i2c_read_data[] = {0x62, 0x60, 0x0, 0x72, 0xB3, 0xFF};
    float *temperature = NULL;
    float *humidity = NULL;
    ReadSingleShotMeasTestCfg cfg = {
        .i2c_addr = 0x45,
        .i2c_write_data = i2c_write_data,
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .timer_period = 1,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 6,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .repeatability = SHT3X_MEAS_REPEATABILITY_MEDIUM,
        .clk_stretching = SHT3X_CLOCK_STRETCHING_ENABLED,
        .flags = SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .complete_cb_user_data_expected = (void *)0xD1,
        .temperature = temperature,
        .humidity = humidity,
        .meas_complete_cb = sht3x_meas_complete_cb,
    };
    test_read_single_shot_measurement(&cfg);
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_read_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
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
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
        if (i2c_read_data != NULL) {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withOutputParameterReturning("data", i2c_read_data, i2c_data_len)
                .withParameter("length", i2c_data_len)
                .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
                .withParameter("user_data", i2c_read_user_data)
                .ignoreOtherParameters();
        } else {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withParameter("length", i2c_data_len)
                .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
                .withParameter("user_data", i2c_read_user_data)
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

static void test_send_read_status_register_cmd(uint8_t i2c_write_rc, uint8_t expected_rc,
                                               void *complete_cb_user_data_expected)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Read status reg command */
    uint8_t i2c_write_data[] = {0xF3, 0x2D};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_send_read_status_register_cmd(sht3x, sht3x_complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    CHECK_EQUAL(1, complete_cb_call_count);
    CHECK_EQUAL(expected_rc, complete_cb_result_code);
    POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
}

TEST(SHT3X, SendReadStatusRegCmd)
{
    void *complete_cb_user_data_expected = (void *)0xC2;
    test_send_read_status_register_cmd(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, complete_cb_user_data_expected);
}

TEST(SHT3X, SendReadStatusRegCmdAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0xC3;
    test_send_read_status_register_cmd(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR,
                                       complete_cb_user_data_expected);
}

TEST(SHT3X, SendReadStatusRegCmdBusError)
{
    void *complete_cb_user_data_expected = (void *)0xD4;
    test_send_read_status_register_cmd(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR,
                                       complete_cb_user_data_expected);
}

TEST(SHT3X, SendReadStatusRegCmdSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xD5;
    uint8_t rc = sht3x_send_read_status_register_cmd(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

static void test_soft_reset_with_delay(uint8_t i2c_write_rc, uint8_t expected_rc, SHT3XCompleteCb complete_cb,
                                       void *complete_cb_user_data_expected)
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
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 2).ignoreOtherParameters();
    }

    uint8_t rc = sht3x_soft_reset_with_delay(sht3x, complete_cb, complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        timer_expired_cb(timer_expired_cb_user_data);
    }

    if (complete_cb) {
        CHECK_EQUAL(1, complete_cb_call_count);
        CHECK_EQUAL(expected_rc, complete_cb_result_code);
        POINTERS_EQUAL(complete_cb_user_data_expected, complete_cb_user_data);
    }
}

TEST(SHT3X, SoftResetWithDelay)
{
    void *complete_cb_user_data_expected = (void *)0xD6;
    test_soft_reset_with_delay(SHT3X_I2C_RESULT_CODE_OK, SHT3X_RESULT_CODE_OK, sht3x_complete_cb,
                               complete_cb_user_data_expected);
}

TEST(SHT3X, SoftResetWithDelayAddressNack)
{
    void *complete_cb_user_data_expected = (void *)0xD7;
    test_soft_reset_with_delay(SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, SHT3X_RESULT_CODE_IO_ERR, sht3x_complete_cb,
                               complete_cb_user_data_expected);
}

TEST(SHT3X, SoftResetWithDelayBusError)
{
    void *complete_cb_user_data_expected = (void *)0xD8;
    test_soft_reset_with_delay(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, sht3x_complete_cb,
                               complete_cb_user_data_expected);
}

TEST(SHT3X, SoftResetWithDelaySelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xD9;
    uint8_t rc = sht3x_soft_reset_with_delay(NULL, sht3x_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, complete_cb_call_count);
}

TEST(SHT3X, SoftResetWithDelayCompleteCbNull)
{
    test_soft_reset_with_delay(SHT3X_I2C_RESULT_CODE_BUS_ERROR, SHT3X_RESULT_CODE_IO_ERR, NULL, NULL);
}

typedef uint8_t (*SHT3XFunction)();

/**
 * @brief Test that when a public SHT3X function is called when another sequence is in progress, BUSY result code is
 * returned.
 *
 * Starts a "enable heater" sequence and does not execute the I2C write callback. Then, invokes @p function and expects
 * that it will return BUSY result code, because the "enable heater" sequence is still in progress.
 *
 * @param function Function that should return BUSY result code.
 */
static void test_busy_if_seq_in_progress(SHT3XFunction function)
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
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc_enable_heater = sht3x_enable_heater(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_enable_heater);
    /* I2C write callback is not executed yet, so enable heater sequence is still in progres. The driver should reject
     * attempts to start new sequences. */

    uint8_t rc = function();
    CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, rc);
    /* User cb should not be called when busy is returned */
    CHECK_EQUAL(0, complete_cb_call_count);
    CHECK_EQUAL(0, meas_complete_cb_call_count);
    CHECK_EQUAL(0, read_status_reg_complete_cb_call_count);
}

static uint8_t send_single_shot_meas_cmd()
{
    return sht3x_send_single_shot_measurement_cmd(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED,
                                                  sht3x_complete_cb, NULL);
}

TEST(SHT3X, SendSingleShotMeasCmdBusy)
{
    test_busy_if_seq_in_progress(send_single_shot_meas_cmd);
}

static uint8_t read_measurement()
{
    return sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb, NULL);
}

TEST(SHT3X, ReadMeasurementBusy)
{
    test_busy_if_seq_in_progress(read_measurement);
}

static uint8_t start_periodic_measurement()
{
    return sht3x_start_periodic_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_MPS_2, sht3x_complete_cb, NULL);
}

TEST(SHT3X, StartPeriodicMeasurementBusy)
{
    test_busy_if_seq_in_progress(start_periodic_measurement);
}

static uint8_t start_periodic_measurement_art()
{
    return sht3x_start_periodic_measurement_art(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, StartPeriodicMeasurementArtBusy)
{
    test_busy_if_seq_in_progress(start_periodic_measurement_art);
}

static uint8_t fetch_periodic_measurement_data()
{
    return sht3x_fetch_periodic_measurement_data(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, FetchPeriodicMeasurementDataBusy)
{
    test_busy_if_seq_in_progress(fetch_periodic_measurement_data);
}

static uint8_t stop_periodic_measurement()
{
    return sht3x_stop_periodic_measurement(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, StopPeriodicMeasurementBusy)
{
    test_busy_if_seq_in_progress(stop_periodic_measurement);
}

static uint8_t soft_reset()
{
    return sht3x_soft_reset(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, SoftResetBusy)
{
    test_busy_if_seq_in_progress(soft_reset);
}

static uint8_t enable_heater()
{
    return sht3x_enable_heater(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, EnableHeaterBusy)
{
    test_busy_if_seq_in_progress(enable_heater);
}

static uint8_t disable_heater()
{
    return sht3x_disable_heater(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, DisableHeaterBusy)
{
    test_busy_if_seq_in_progress(disable_heater);
}

static uint8_t send_read_status_register_cmd()
{
    return sht3x_send_read_status_register_cmd(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, SendReadStatusRegisterCmdBusy)
{
    test_busy_if_seq_in_progress(send_read_status_register_cmd);
}

static uint8_t clear_status_register()
{
    return sht3x_clear_status_register(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, ClearStatusRegisterBusy)
{
    test_busy_if_seq_in_progress(clear_status_register);
}

static uint8_t read_single_shot_measurement()
{
    return sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_CLOCK_STRETCHING_ENABLED,
                                              SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb, NULL);
}

TEST(SHT3X, ReadSingleShotMeasurementBusy)
{
    test_busy_if_seq_in_progress(read_single_shot_measurement);
}

static uint8_t read_periodic_measurement()
{
    return sht3x_read_periodic_measurement(sht3x, SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb, NULL);
}

TEST(SHT3X, ReadPeriodicMeasurementBusy)
{
    test_busy_if_seq_in_progress(read_periodic_measurement);
}

static uint8_t soft_reset_with_delay()
{
    return sht3x_soft_reset_with_delay(sht3x, sht3x_complete_cb, NULL);
}

TEST(SHT3X, SoftResetWithDelayBusy)
{
    test_busy_if_seq_in_progress(soft_reset_with_delay);
}

static uint8_t read_status_register()
{
    return sht3x_read_status_register(sht3x, SHT3X_VERIFY_CRC_NO, sht3x_read_status_reg_complete_cb, NULL);
}

TEST(SHT3X, ReadStatusRegisterBusy)
{
    test_busy_if_seq_in_progress(read_status_register);
}

static uint8_t destroy()
{
    return sht3x_destroy(sht3x, NULL, NULL);
}

TEST(SHT3X, DestroyBusy)
{
    test_busy_if_seq_in_progress(destroy);
}

TEST(SHT3X, DestroySelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t rc = sht3x_destroy(NULL, NULL, NULL);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
}

static void test_i2c_write_seq_cannot_be_interrupted(uint8_t *i2c_write_data, uint8_t i2c_write_rc,
                                                     SHT3XFunction start_seq)
{
    if (!start_seq || !i2c_write_data) {
        FAIL_TEST("Invalid args");
    }
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    /* Clear status register command */
    uint8_t i2c_write_data_clear_status_reg[] = {0x30, 0x41};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_clear_status_reg, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc = start_seq();
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    uint8_t other_cmd_rc;
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);

    /* Sequence finished, other operations are now allowed */
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, other_cmd_rc);
}

TEST(SHT3X, SingleShotMeasCmdCannotBeInterrupted)
{
    /* Single shot meas with high repeatability and clock stretching disabled command */
    uint8_t i2c_write_data[] = {0x24, 0x0};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_OK, send_single_shot_meas_cmd);
}

TEST(SHT3X, ReadMeasurementCannotBeInterrupted)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t i2c_read_data[] = {0x62, 0x60};
    mock()
        .expectOneCall("mock_sht3x_i2c_read")
        .withOutputParameterReturning("data", i2c_read_data, sizeof(i2c_read_data))
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_read_user_data)
        .ignoreOtherParameters();
    /* Clear status register command */
    uint8_t i2c_write_data_clear_status_reg[] = {0x30, 0x41};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_clear_status_reg, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_read_measurement(sht3x, SHT3X_FLAG_READ_TEMP, sht3x_meas_complete_cb, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    uint8_t other_cmd_rc;
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

    i2c_read_complete_cb(SHT3X_I2C_RESULT_CODE_OK, i2c_read_complete_cb_user_data);

    /* Sequence finished, other operations are now allowed */
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, other_cmd_rc);
}

TEST(SHT3X, StartPeriodicMeasurementCannotBeInterrupted)
{
    /* Start periodic data acquisition: high repeatability, 2 mps */
    uint8_t i2c_write_data[] = {0x22, 0x36};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
                                             start_periodic_measurement);
}

TEST(SHT3X, StartPeriodicMeasurementArtCannotBeInterrupted)
{
    /* ART command */
    uint8_t i2c_write_data[] = {0x2B, 0x32};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_BUS_ERROR,
                                             start_periodic_measurement_art);
}

TEST(SHT3X, FetchPeriodicMeasurementDataCannotBeInterrupted)
{
    /* Fetch data command */
    uint8_t i2c_write_data[] = {0xE0, 0x0};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_OK, fetch_periodic_measurement_data);
}

TEST(SHT3X, StopPeriodicMeasurementCannotBeInterrupted)
{
    /* Stop periodic meas command */
    uint8_t i2c_write_data[] = {0x30, 0x93};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
                                             stop_periodic_measurement);
}

TEST(SHT3X, SoftResetCannotBeInterrupted)
{
    /* Soft reset command */
    uint8_t i2c_write_data[] = {0x30, 0xA2};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_BUS_ERROR, soft_reset);
}

TEST(SHT3X, EnableHeaterCannotBeInterrupted)
{
    /* Enable heater command */
    uint8_t i2c_write_data[] = {0x30, 0x6D};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_OK, enable_heater);
}

TEST(SHT3X, DisableHeaterCannotBeInterrupted)
{
    /* Disable heater command */
    uint8_t i2c_write_data[] = {0x30, 0x66};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, disable_heater);
}

TEST(SHT3X, SendReadStatusRegCmdCannotBeInterrupted)
{
    /* Read status reg command */
    uint8_t i2c_write_data[] = {0xF3, 0x2D};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_BUS_ERROR,
                                             send_read_status_register_cmd);
}

TEST(SHT3X, ClearStatusRegCannotBeInterrupted)
{
    /* Clear status reg command */
    uint8_t i2c_write_data[] = {0x30, 0x41};
    test_i2c_write_seq_cannot_be_interrupted(i2c_write_data, SHT3X_I2C_RESULT_CODE_OK, clear_status_register);
}

static void test_write_read_seq_cannot_be_interrupted(SHT3XFunction start_seq, uint8_t *i2c_write_data,
                                                      uint8_t i2c_write_rc, uint8_t *i2c_read_data, uint8_t i2c_read_rc)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    size_t i2c_data_len = 2;
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
        mock()
            .expectOneCall("mock_sht3x_i2c_read")
            .withOutputParameterReturning("data", i2c_read_data, i2c_data_len)
            .withParameter("length", i2c_data_len)
            .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
            .withParameter("user_data", i2c_read_user_data)
            .ignoreOtherParameters();
    }
    /* Clear status register command */
    uint8_t i2c_write_data_clear_status_reg[] = {0x30, 0x41};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_clear_status_reg, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc = start_seq();
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    uint8_t other_cmd_rc;
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        /* Timer period is ongoing */
        other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
        CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

        timer_expired_cb(timer_expired_cb_user_data);

        /* I2C read is ongoing */
        other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
        CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

        i2c_read_complete_cb(i2c_read_rc, i2c_read_complete_cb_user_data);
    }

    /* Sequence finished, other operations are now allowed */
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, other_cmd_rc);
}

TEST(SHT3X, ReadSingleShotMeasurementWriteFailCannotBeInterrupted)
{
    /* Single shot measurement cmd with medium repeatability and clock stretching enabled */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    uint8_t i2c_read_data[] = {0x62, 0x60};
    test_write_read_seq_cannot_be_interrupted(read_single_shot_measurement, i2c_write_data,
                                              SHT3X_I2C_RESULT_CODE_ADDRESS_NACK, i2c_read_data,
                                              SHT3X_I2C_RESULT_CODE_ADDRESS_NACK);
}

TEST(SHT3X, ReadSingleShotMeasurementReadFailCannotBeInterrupted)
{
    /* Single shot measurement cmd with medium repeatability and clock stretching enabled */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    uint8_t i2c_read_data[] = {0x62, 0x60};
    test_write_read_seq_cannot_be_interrupted(read_single_shot_measurement, i2c_write_data, SHT3X_I2C_RESULT_CODE_OK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK);
}

TEST(SHT3X, ReadSingleShotMeasurementSuccessCannotBeInterrupted)
{
    /* Single shot measurement cmd with medium repeatability and clock stretching enabled */
    uint8_t i2c_write_data[] = {0x2C, 0x0D};
    uint8_t i2c_read_data[] = {0x62, 0x60};
    test_write_read_seq_cannot_be_interrupted(read_single_shot_measurement, i2c_write_data, SHT3X_I2C_RESULT_CODE_OK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_OK);
}

TEST(SHT3X, ReadPeriodicMeasWriteFailCannotBeInterrupted)
{
    /* Fetch data cmd */
    uint8_t i2c_write_data[] = {0xE0, 0x0};
    uint8_t i2c_read_data[] = {0x62, 0x60};
    test_write_read_seq_cannot_be_interrupted(read_periodic_measurement, i2c_write_data,
                                              SHT3X_I2C_RESULT_CODE_BUS_ERROR, i2c_read_data,
                                              SHT3X_I2C_RESULT_CODE_BUS_ERROR);
}

TEST(SHT3X, ReadPeriodicMeasReadFailCannotBeInterrupted)
{
    /* Fetch data cmd */
    uint8_t i2c_write_data[] = {0xE0, 0x0};
    uint8_t i2c_read_data[] = {0x62, 0x60};
    test_write_read_seq_cannot_be_interrupted(read_periodic_measurement, i2c_write_data, SHT3X_I2C_RESULT_CODE_OK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_BUS_ERROR);
}

TEST(SHT3X, ReadPeriodicMeasSuccessCannotBeInterrupted)
{
    /* Fetch data cmd */
    uint8_t i2c_write_data[] = {0xE0, 0x0};
    uint8_t i2c_read_data[] = {0x62, 0x60};
    test_write_read_seq_cannot_be_interrupted(read_periodic_measurement, i2c_write_data, SHT3X_I2C_RESULT_CODE_OK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_OK);
}

TEST(SHT3X, ReadStatusRegWriteFailCannotBeInterrupted)
{
    /* Read status register command */
    uint8_t i2c_write_data[] = {0xF3, 0x2D};
    uint8_t i2c_read_data[] = {0x80, 0x00};
    test_write_read_seq_cannot_be_interrupted(read_status_register, i2c_write_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK);
}

TEST(SHT3X, ReadStatusRegReadFailCannotBeInterrupted)
{
    /* Read status register command */
    uint8_t i2c_write_data[] = {0xF3, 0x2D};
    uint8_t i2c_read_data[] = {0x80, 0x00};
    test_write_read_seq_cannot_be_interrupted(read_status_register, i2c_write_data, SHT3X_I2C_RESULT_CODE_OK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_ADDRESS_NACK);
}

TEST(SHT3X, ReadStatusRegSuccessCannotBeInterrupted)
{
    /* Read status register command */
    uint8_t i2c_write_data[] = {0xF3, 0x2D};
    uint8_t i2c_read_data[] = {0x80, 0x00};
    test_write_read_seq_cannot_be_interrupted(read_status_register, i2c_write_data, SHT3X_I2C_RESULT_CODE_OK,
                                              i2c_read_data, SHT3X_I2C_RESULT_CODE_OK);
}

static void test_soft_reset_with_delay_cannot_be_interrupted(uint8_t i2c_write_rc)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    uint8_t i2c_write_data[] = {0x30, 0xA2};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 2).ignoreOtherParameters();
    }
    /* Clear status register command */
    uint8_t i2c_write_data_clear_status_reg[] = {0x30, 0x41};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data_clear_status_reg, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();

    uint8_t rc = sht3x_soft_reset_with_delay(sht3x, sht3x_complete_cb, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);

    uint8_t other_cmd_rc;
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

    i2c_write_complete_cb(i2c_write_rc, i2c_write_complete_cb_user_data);
    if (i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        /* Timer period is ongoing */
        other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
        CHECK_EQUAL(SHT3X_RESULT_CODE_BUSY, other_cmd_rc);

        timer_expired_cb(timer_expired_cb_user_data);
    }

    /* Sequence finished, other operations are now allowed */
    other_cmd_rc = sht3x_clear_status_register(sht3x, NULL, NULL);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, other_cmd_rc);
}

TEST(SHT3X, SoftResetWithDelayWriteFailCannotBeInterrupted)
{
    test_soft_reset_with_delay_cannot_be_interrupted(SHT3X_I2C_RESULT_CODE_BUS_ERROR);
}

TEST(SHT3X, SoftResetWithDelaySuccessCannotBeInterrupted)
{
    test_soft_reset_with_delay_cannot_be_interrupted(SHT3X_I2C_RESULT_CODE_OK);
}

typedef struct {
    uint8_t i2c_write_rc;
    uint8_t *i2c_read_data;
    uint8_t i2c_data_len;
    uint8_t i2c_read_rc;
    bool verify_crc;
    SHT3XReadStatusRegCompleteCb complete_cb;
    void *complete_cb_user_data_expected;
    uint8_t expected_complete_cb_rc;
    uint16_t *status_reg_val_expected;
} TestReadStatusRegCfg;

static void test_read_status_register(const TestReadStatusRegCfg *const cfg)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    /* Read status reg command */
    uint8_t i2c_write_data[] = {0xF3, 0x2D};
    mock()
        .expectOneCall("mock_sht3x_i2c_write")
        .withMemoryBufferParameter("data", i2c_write_data, 2)
        .withParameter("length", 2)
        .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
        .withParameter("user_data", i2c_write_user_data)
        .ignoreOtherParameters();
    if (cfg->i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        mock().expectOneCall("mock_sht3x_start_timer").withParameter("duration_ms", 1).ignoreOtherParameters();
        if (cfg->i2c_read_data != NULL) {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withOutputParameterReturning("data", cfg->i2c_read_data, cfg->i2c_data_len)
                .withParameter("length", cfg->i2c_data_len)
                .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
                .withParameter("user_data", i2c_read_user_data)
                .ignoreOtherParameters();
        } else {
            mock()
                .expectOneCall("mock_sht3x_i2c_read")
                .withParameter("length", cfg->i2c_data_len)
                .withParameter("i2c_addr", SHT3X_TEST_DEFAULT_I2C_ADDR)
                .withParameter("user_data", i2c_read_user_data)
                .ignoreOtherParameters();
        }
    }

    uint16_t status_reg_val;
    uint8_t rc =
        sht3x_read_status_register(sht3x, cfg->verify_crc, cfg->complete_cb, cfg->complete_cb_user_data_expected);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc);
    i2c_write_complete_cb(cfg->i2c_write_rc, i2c_write_complete_cb_user_data);
    if (cfg->i2c_write_rc == SHT3X_I2C_RESULT_CODE_OK) {
        timer_expired_cb(timer_expired_cb_user_data);
        i2c_read_complete_cb(cfg->i2c_read_rc, i2c_read_complete_cb_user_data);
    }

    if (cfg->complete_cb) {
        CHECK_EQUAL(1, read_status_reg_complete_cb_call_count);
        CHECK_EQUAL(cfg->expected_complete_cb_rc, read_status_reg_complete_cb_result_code);
        POINTERS_EQUAL(cfg->complete_cb_user_data_expected, read_status_reg_complete_cb_user_data);
        if (cfg->status_reg_val_expected) {
            CHECK_EQUAL(*(cfg->status_reg_val_expected), read_status_reg_complete_cb_reg_val);
        }
    }
}

TEST(SHT3X, ReadStatusRegI2cWriteAddressNack)
{
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
        /* Don't care */
        .i2c_read_data = NULL,
        .i2c_data_len = 0,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .verify_crc = SHT3X_VERIFY_CRC_NO,
        /* Care */
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE0,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .status_reg_val_expected = NULL,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegI2cWriteBusError)
{
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR,
        /* Don't care */
        .i2c_read_data = NULL,
        .i2c_data_len = 0,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .verify_crc = SHT3X_VERIFY_CRC_NO,
        /* Care */
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE1,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .status_reg_val_expected = NULL,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegI2cReadAddressNack)
{
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .i2c_read_data = NULL,
        .i2c_data_len = 2,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_ADDRESS_NACK,
        .verify_crc = SHT3X_VERIFY_CRC_NO,
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE2,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .status_reg_val_expected = NULL,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegI2cReadBusError)
{
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .i2c_read_data = NULL,
        .i2c_data_len = 3,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_BUS_ERROR,
        .verify_crc = SHT3X_VERIFY_CRC_YES,
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE3,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_IO_ERR,
        .status_reg_val_expected = NULL,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegNoCrc)
{
    uint8_t i2c_read_data[] = {0x80, 0x00};
    uint16_t status_reg_val = 0x8000;
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 2,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .verify_crc = SHT3X_VERIFY_CRC_NO,
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE4,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .status_reg_val_expected = &status_reg_val,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegWrongCrc)
{
    uint8_t i2c_read_data[] = {0x80, 0x03, 0x42};
    uint16_t status_reg_val = 0x8003;
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 3,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .verify_crc = SHT3X_VERIFY_CRC_YES,
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE5,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_CRC_MISMATCH,
        .status_reg_val_expected = &status_reg_val,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegCrc)
{
    uint8_t i2c_read_data[] = {0x80, 0x03, 0xF1};
    uint16_t status_reg_val = 0x8003;
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 3,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .verify_crc = SHT3X_VERIFY_CRC_YES,
        .complete_cb = sht3x_read_status_reg_complete_cb,
        .complete_cb_user_data_expected = (void *)0xE6,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .status_reg_val_expected = &status_reg_val,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegCompleteCbNull)
{
    uint8_t i2c_read_data[] = {0x80, 0x03, 0xF1};
    uint16_t status_reg_val = 0x8003;
    TestReadStatusRegCfg cfg = {
        .i2c_write_rc = SHT3X_I2C_RESULT_CODE_OK,
        .i2c_read_data = i2c_read_data,
        .i2c_data_len = 3,
        .i2c_read_rc = SHT3X_I2C_RESULT_CODE_OK,
        .verify_crc = SHT3X_VERIFY_CRC_YES,
        .complete_cb = NULL,
        .complete_cb_user_data_expected = NULL,
        .expected_complete_cb_rc = SHT3X_RESULT_CODE_OK,
        .status_reg_val_expected = &status_reg_val,
    };
    test_read_status_register(&cfg);
}

TEST(SHT3X, ReadStatusRegSelfNull)
{
    uint8_t rc_create = sht3x_create(&sht3x, &init_cfg);
    CHECK_EQUAL(SHT3X_RESULT_CODE_OK, rc_create);

    void *user_data = (void *)0xE7;
    uint8_t rc = sht3x_read_status_register(NULL, SHT3X_VERIFY_CRC_NO, sht3x_read_status_reg_complete_cb, user_data);

    CHECK_EQUAL(SHT3X_RESULT_CODE_INVALID_ARG, rc);
    CHECK_EQUAL(0, read_status_reg_complete_cb_call_count);
}
