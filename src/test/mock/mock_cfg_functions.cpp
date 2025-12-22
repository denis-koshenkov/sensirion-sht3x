#include "CppUTestExt/MockSupport.h"
#include "mock_cfg_functions.h"

void *mock_sht3x_get_instance_memory(void *user_data)
{
    mock().actualCall("mock_sht3x_get_instance_memory").withParameter("user_data", user_data);
    return mock().pointerReturnValue();
}
