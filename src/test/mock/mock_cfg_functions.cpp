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
