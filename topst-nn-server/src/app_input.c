#define _DEFAULT_SOURCE

#include "app_input.h"

#include <stdio.h>
#include <unistd.h>

#include "app_inference.h"
#include "app_vision.h"
#include "opencv_api.h"

int app_input_init(app_context_t *app)
{
    if (app->input_mode == APP_INPUT_CAMERA) {
        return 0;
    }

    if (app->input_mode == APP_INPUT_TCP || app->input_mode == APP_INPUT_VISION) {
        return app_vision_init(app);
    }

    return -1;
}

void app_input_deinit(app_context_t *app)
{
    app_vision_deinit(app);
}

int acquire_input_frame(app_context_t *app)
{
    if (app->input_mode == APP_INPUT_CAMERA) {
        if (camera_get_buffer(app->camera, &app->camera_virt_addr,
                              &app->camera_phys_addr) <= 0) {
            usleep(1000);
            return 0;
        }
        return 1;
    }

    if (app->input_mode == APP_INPUT_TCP || app->input_mode == APP_INPUT_VISION) {
        return app_vision_recv_frame(app);
    }

    return -1;
}

void release_input_frame(app_context_t *app)
{
    if (app->input_mode == APP_INPUT_CAMERA && app->camera != NULL) {
        (void)camera_release_buffer(app->camera);
    } else if (app->input_mode == APP_INPUT_TCP || app->input_mode == APP_INPUT_VISION) {
        app_vision_release_frame(app);
    }
}

int prepare_model_input(app_context_t *app, const model_context_t *model,
                        scaler_index_t scaler_index)
{
    scaler_image_t src;
    scaler_image_t dst;

    if (app->input_mode == APP_INPUT_CAMERA) {
        src.paddr = app->camera_phys_addr;
        src.width = app->camera_width;
        src.height = app->camera_height;
        src.format = SCALER_FORMAT_ARGB8888;

        dst.paddr = model->input_buf->paddr;
        dst.width = app_align_width((uint32_t)model->input_width, 16u);
        dst.height = (uint32_t)model->input_height;
        dst.format = SCALER_FORMAT_RGB888;

        if (scaler_resize(app->scaler, scaler_index, src, dst) != 0) {
            return -1;
        }

        return scaler_poll(app->scaler, scaler_index);
    }

    if (app->input_mode == APP_INPUT_TCP || app->input_mode == APP_INPUT_VISION) {
        uint64_t frame_phys = app_vision_frame_phys(app);
        if (frame_phys == 0) {
            return -1;
        }

        src.paddr = frame_phys;
        src.width = app->camera_width;
        src.height = app->camera_height;
        src.format = SCALER_FORMAT_RGB888;

        dst.paddr = model->input_buf->paddr;
        dst.width = app_align_width((uint32_t)model->input_width, 16u);
        dst.height = (uint32_t)model->input_height;
        dst.format = SCALER_FORMAT_RGB888;

        if (scaler_resize(app->scaler, scaler_index, src, dst) != 0) {
            return -1;
        }

        return scaler_poll(app->scaler, scaler_index);
    }

    return -1;
}
