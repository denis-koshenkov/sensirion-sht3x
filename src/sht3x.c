#include <stddef.h>

#include "sht3x.h"

uint8_t sht3x_create(SHT3X *instance, const SHT3XInitConfig *const cfg)
{
    if (!(cfg->get_instance_memory)) {
        return SHT3X_RETURN_CODE_INVALID_ARG;
    }

    cfg->get_instance_memory(cfg->get_instance_memory_user_data);
    return SHT3X_RETURN_CODE_OK;
}
