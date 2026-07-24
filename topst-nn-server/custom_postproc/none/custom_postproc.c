#include "custom_postproc.h"

const char *app_custom_postproc_name(void)
{
    return "none";
}

int app_custom_postproc_is_enabled(void)
{
    return 0;
}

int app_custom_postproc_run(model_context_t *model)
{
    (void)model;
    return 0;
}

void app_custom_postproc_render(const app_context_t *app, const model_context_t *model,
                            uint8_t *output_map_base)
{
    (void)app;
    (void)model;
    (void)output_map_base;
}
