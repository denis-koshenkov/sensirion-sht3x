# Sensirion SHT3X Driver
Driver for Sensirion SHT3X temperature & humidity sensor that supports non-blocking mode of operation.

The driver is implemented in C language.

The driver was developed using Test-Driven Development (TDD). Unit testing is performed using the [CppUTest](https://cpputest.github.io/) testing framework.

# Integration Details
Add the following to your build:
- `src/sht3x.c` source file
- `src` directory as include directory

# Usage
In order to use this driver, you need to implement the following functions:
```
void sht3x_i2c_write(uint8_t *data, size_t length, uint8_t i2c_addr, SHT3X_I2CTransactionCompleteCb cb, void *user_data);
void sht3x_i2c_read(uint8_t *data, size_t length, uint8_t i2c_addr, SHT3X_I2CTransactionCompleteCb cb,
                              void *user_data);
void sht3x_start_timer(uint32_t duration_ms, SHT3XTimerExpiredCb cb, void *user_data);
void *sht3x_get_instance_memory(void *user_data);
```

Pass your implementations as function pointers when creating an instance:
```
SHT3XInitConfig cfg = {
    .get_instance_memory = sht3x_get_instance_memory,
    .get_instance_memory_user_data = NULL, // Optional
    .i2c_write = sht3x_i2c_write,
    .i2c_read = sht3x_i2c_read,
    .start_timer = sht3x_start_timer,
    .i2c_addr = 0x44, // or 0x45
};

SHT3X sht3x;
uint8_t rc = sht3x_create(&sht3x, &cfg);
if (rc != SHT3X_RESULT_CODE_OK) {
    // Error handling
}
```

## Examples
Perform a single shot measurement of temperature and humidity:
```
static void single_shot_complete(uint8_t result_code, SHT3XMeasurement *meas, void *user_data) {
    if (result_code == SHT3X_RESULT_CODE_OK) {
        float temp = meas->temperature; // Temperature in degrees Celsius
        float hum = meas->humidity; // Humidity in RH%
    }
}

void some_func() {
    SHT3XInitConfig cfg = {
        .get_instance_memory = sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL, // Optional
        .i2c_write = sht3x_i2c_write,
        .i2c_read = sht3x_i2c_read,
        .start_timer = sht3x_start_timer,
        .i2c_addr = 0x44, // or 0x45
    };

    SHT3X sht3x;
    uint8_t rc = sht3x_create(&sht3x, &cfg);
    if (rc != SHT3X_RESULT_CODE_OK) {
        // Error handling
    }

    /*  High repeatability, clock stretching disabled, no CRC verification */
    uint8_t rc = sht3x_read_single_shot_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_HIGH, SHT3X_CLOCK_STRETCHING_DISABLED, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM, single_shot_complete, NULL);
    if (rc != SHT3X_RESULT_CODE_OK) {
        // Error handling
    }
}
```

Start periodic measurements and read them out regularly:
```
static void periodic_meas_started(uint8_t result_code, void *user_data) {
    if (result_code == SHT3X_RESULT_CODE_OK) {
        // Handle completion of starting periodic measurement
    }
}

static void periodic_meas_received(uint8_t result_code, SHT3XMeasurement *meas, void *user_data) {
    if (result_code == SHT3X_RESULT_CODE_OK) {
        float temp = meas->temperature; // Temperature in degrees Celsius
        float hum = meas->humidity; // Humidity in RH%
    }
}

void some_func() {
    SHT3XInitConfig cfg = {
        .get_instance_memory = sht3x_get_instance_memory,
        .get_instance_memory_user_data = NULL, // Optional
        .i2c_write = sht3x_i2c_write,
        .i2c_read = sht3x_i2c_read,
        .start_timer = sht3x_start_timer,
        .i2c_addr = 0x44, // or 0x45
    };

    SHT3X sht3x;
    uint8_t rc;

    rc = sht3x_create(&sht3x, &cfg);
    if (rc != SHT3X_RESULT_CODE_OK) {
        // Error handling
    }

    /*  High repeatability, 4 measurements per second */
    rc = sht3x_start_periodic_measurement(sht3x, SHT3X_MEAS_REPEATABILITY_MEDIUM, SHT3X_MPS_4, periodic_meas_started, NULL);
    if (rc != SHT3X_RESULT_CODE_OK) {
        // Error handling
    }

    // Wait until periodic_meas_started callback is executed

    while (1) {
        /* Read out temperature and humidity, and verify CRC for both */
        rc = sht3x_read_periodic_measurement(sht3x, SHT3X_FLAG_READ_TEMP | SHT3X_FLAG_READ_HUM | SHT3X_FLAG_VERIFY_CRC_TEMP | SHT3X_FLAG_VERIFY_CRC_HUM, periodic_meas_received, NULL);
        if (rc != SHT3X_RESULT_CODE_OK) {
            // Error handling
        }
        
        // Sleep for 250 ms, since we get 4 measurements per second
        sleep_ms(250);
    }
}
```
The purpose of this example is to only demonstrate driver usage. This is probably not the way to go in an asynchronous system.

## Function Implementation Guidelines
`sht3x_i2c_write` and `sht3x_i2c_read` must implement write and read I2C transactions respectively. When a transaction is complete, they should invoke the provided callback with the provided user data as parameter.

`sht3x_start_timer` must invoke the provided callback with user data after at least `duration_ms` milliseconds pass from the moment `sht3x_start_timer` is invoked.

`sht3x_get_instance_memory` is called during `sht3x_create`. It provides the memory to use for a SHT3X instance. This gives the user control over how the memory for the instance is allocated (static, dynamic, pool-based, etc.).

It must return a pointer to memory of size `sizeof(struct SHT3XStruct)`. This memory should be valid for the whole lifetime of the SHT3X instance.

In order to know `sizeof(struct SHT3XStruct)` at compile time, the source file implementing `sht3x_get_instance_memory` will need to include the `sht3x_private.h` file.

The implementation of `sht3x_get_instance_memory` should be defined in a separate source file, so that all other code that interacts with the driver does not include `sht3x_private.h`.

This way, the user code is prevented from being able to access fields of `SHT3X` directly - `SHT3X` data type is defined as a pointer to `struct SHT3XStruct`.

### I2C Result Codes
When invoking the `SHT3X_I2CTransactionCompleteCb` callback, the implementations of `sht3x_i2c_write` and `sht3x_i2c_read` must pass a result code to the callback:

- `SHT3X_I2C_RESULT_CODE_OK`: I2C transaction was successful.
- `SHT3X_I2C_RESULT_CODE_ADDRESS_NACK`: Transaction failed - received a NACK on the I2C bus after sending the address byte.
- `SHT3X_I2C_RESULT_CODE_BUS_ERROR`: Transaction failed due to another reason (e.g. unexpected transition on the I2C bus).

It is important to distinguish between `SHT3X_I2C_RESULT_CODE_ADDRESS_NACK` and `SHT3X_I2C_RESULT_CODE_BUS_ERROR`, because in some cases a NACK after address byte is expected.

For example, when attempting to read out a measurement, the device uses a NACK after the address byte to indicate that the measurements are not ready. 

## Execution Context
All calls to public functions of the driver must be made from the same context/thread.

Calls to the callbacks from the user's implementations of `sht3x_i2c_write`, `sht3x_i2c_read`, and `sht3x_start_timer` also must be made **from the same context/thread**.

For example, the user implementation of `sht3x_i2c_write` calls an asynchronous I2C API which fires a callback when the transaction is done. This callback is executed from ISR context.

It is not allowed to directly call the `SHT3X_I2CTransactionCompleteCb` from that callback, because that would mean calling it from ISR context. It is the user's responsibility to pass the event of I2C transaction completion to the thread that calls the driver functions.

That thread would then invoke the `SHT3X_I2CTransactionCompleteCb` that was passed to `sht3x_i2c_write` as parameter.

The same applies to executing completion callbacks passed to `sht3x_i2c_read` and `sht3x_start_timer`.

# Running Tests
Prerequisites:
- CMake
- C compiler (e.g. gcc)
- C++ compiler (e.g. g++)

Steps:
1. Clone this repository
2. Fetch the CppUTest submodule:
```
git submodule init
git submodule update
```
3. Build and run the tests:
```
./run_tests.sh
```
