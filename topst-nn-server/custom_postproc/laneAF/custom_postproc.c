#include "custom_postproc.h"

#include <string.h>
#include <dlfcn.h>

#include "opencv_api.h"

const char *app_custom_postproc_name(void)
{
    return "laneAF";
}

int app_custom_postproc_is_enabled(void)
{
    return 1;
}

int app_custom_postproc_run(model_context_t *model)
{
    laneaf_result_t *lane_data = NULL;
    int ret;
    int has_pointer_result_abi = 0;

    if (model == NULL || model->net == NULL) {
        return -1;
    }

    memset(&model->laneaf_result, 0, sizeof(model->laneaf_result));
    model->laneaf_result.img_w = model->input_width;
    model->laneaf_result.img_h = model->input_height;

    if (model->net->dl && dlsym(model->net->dl, "laneaf_get_last_result")) {
        has_pointer_result_abi = 1;
    }

    if (has_pointer_result_abi) {
        ret = network_run_postprocess(model->net, model->output_buf, &lane_data);
        if (ret) {
            return ret;
        }
        if (lane_data) {
            model->laneaf_result = *lane_data;
        }
    } else {
        ret = network_run_postprocess(model->net, model->output_buf, &model->laneaf_result);
        if (ret) {
            return ret;
        }
    }

    if (model->laneaf_result.img_w <= 0) {
        model->laneaf_result.img_w = model->input_width;
    }
    if (model->laneaf_result.img_h <= 0) {
        model->laneaf_result.img_h = model->input_height;
    }
    if (model->laneaf_result.num_lanes < 0 || model->laneaf_result.num_lanes > MAX_LANES) {
        model->laneaf_result.num_lanes = 0;
    }

    return 0;
}

void app_custom_postproc_render(const app_context_t *app, const model_context_t *model,
                            uint8_t *output_map_base)
{
    static const Color_t lane_colors[6] = {
        RGB(255, 0, 0),
        RGB(0, 255, 0),
        RGB(0, 255, 255),
        RGB(255, 255, 0),
        RGB(255, 0, 255),
        RGB(255, 255, 255),
    };
    const laneaf_result_t *res;
    int lane_idx;

    if (app == NULL || model == NULL || output_map_base == NULL) {
        return;
    }

    res = model->lane_data;
    if (res == NULL || res->num_lanes <= 0 || res->img_w <= 0 || res->img_h <= 0) {
        return;
    }

    for (lane_idx = 0; lane_idx < res->num_lanes && lane_idx < MAX_LANES; ++lane_idx) {
        const lane_polyline_t *ln = &res->lane[lane_idx];
        Point_t line_start[MAX_POINTS - 1];
        Point_t line_end[MAX_POINTS - 1];
        Color_t lane_color = lane_colors[lane_idx % 6];
        int line_count = 0;
        int prev_valid = 0;
        int prev_x = 0;
        int prev_y = 0;
        int p;

        if (ln->n < 2) {
            continue;
        }

        for (p = 0; p < ln->n && p < MAX_POINTS; ++p) {
            int x;
            int y;
            int clamped_x;
            int clamped_y;

            if (ln->conf[p] < 0.0f) {
                prev_valid = 0;
                continue;
            }

            x = (int)((ln->x[p] / (float)res->img_w) * (float)app->display_width + 0.5f);
            y = (int)((ln->y[p] / (float)res->img_h) * (float)app->display_height + 0.5f);
            clamped_x = x < 0 ? 0 : x;
            clamped_y = y < 0 ? 0 : y;
            if (clamped_x >= (int)app->display_width) {
                clamped_x = (int)app->display_width - 1;
            }
            if (clamped_y >= (int)app->display_height) {
                clamped_y = (int)app->display_height - 1;
            }

            if (prev_valid && line_count < (MAX_POINTS - 1)) {
                line_start[line_count].x = prev_x;
                line_start[line_count].y = prev_y;
                line_end[line_count].x = clamped_x;
                line_end[line_count].y = clamped_y;
                ++line_count;
            }
            prev_x = clamped_x;
            prev_y = clamped_y;
            prev_valid = 1;
        }

        if (line_count > 0) {
            cvDrawLines(output_map_base, app->display_width, app->display_height,
                        line_start, line_end, line_count, lane_color, 3);
        }
    }
}
