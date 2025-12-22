#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "sht3x.h"
/* Included to know the size of SHT3X instance we need to define to return from mock_sht3x_get_instance_memory. */
#include "sht3x_private.h"
#include "mock_cfg_functions.h"

/* To return from mock_sht3x_get_instance_memory */
static struct SHT3XStruct instance_memory;

TEST_GROUP(SHT3X){};

TEST(SHT3X, CreateReturnsInvalidArgIfGetInstMemoryIsNull)
{
    SHT3X sht3x;
    SHT3XInitConfig cfg = {
        .get_instance_memory = NULL,
        .get_instance_memory_user_data = (void *)0x1,
    };
    uint8_t rc = sht3x_create(&sht3x, &cfg);

    CHECK_EQUAL(SHT3X_RETURN_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, CreateReturnsInvalidArgIfCfgIsNull)
{
    SHT3X sht3x;
    uint8_t rc = sht3x_create(&sht3x, NULL);

    CHECK_EQUAL(SHT3X_RETURN_CODE_INVALID_ARG, rc);
}

TEST(SHT3X, CreateReturnsInvalidArgIfInstanceIsNull)
{
    SHT3XInitConfig cfg = {
        .get_instance_memory = mock_sht3x_get_instance_memory,
        .get_instance_memory_user_data = (void *)0x1,
    };
    uint8_t rc = sht3x_create(NULL, &cfg);

    CHECK_EQUAL(SHT3X_RETURN_CODE_INVALID_ARG, rc);
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

    CHECK_EQUAL(SHT3X_RETURN_CODE_OUT_OF_MEMORY, rc);
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

    CHECK_EQUAL(SHT3X_RETURN_CODE_OK, rc);
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

    CHECK_EQUAL(SHT3X_RETURN_CODE_OK, rc_create);
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

    CHECK_EQUAL(SHT3X_RETURN_CODE_OK, rc_create);
}
