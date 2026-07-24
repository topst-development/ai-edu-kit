#ifndef CUSTOM_POSTPROC_H
#define CUSTOM_POSTPROC_H

#include <stdint.h>

#include "app_types.h"

const char *app_custom_postproc_name(void);
int app_custom_postproc_is_enabled(void);
int app_custom_postproc_run(model_context_t *model);
void app_custom_postproc_render(const app_context_t *app, const model_context_t *model,
                            uint8_t *output_map_base);

#endif
