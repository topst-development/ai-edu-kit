#include "app_render.h"
#include "app_vision.h"
#include "custom_postproc.h"

#include <sys/mman.h>

#include "opencv_api.h"

static void overlay_results(app_context_t *app, uint8_t *output_map_base)
{
    static const Color_t colors[APP_MAX_MODELS] = {
        RGB(80, 255, 120),
        RGB(80, 255, 120),
    };
    const int compact_ui = (app->display_width <= 800 || app->display_height <= 480);
    const int left_overlay_margin = compact_ui ? 28 : 36;
    const int right_overlay_margin = compact_ui ? -2 : 6;
    const int perf_line_step = compact_ui ? 22 : 18;
    const int perf_group_gap = compact_ui ? 6 : 2;
    const int perf_column_width = compact_ui ? 125 : 200;
    const double perf_font_size = compact_ui ? 0.4 : 0.8;
    const double box_font_size = compact_ui ? 0.5 : 0.8;
    const int box_label_offset = compact_ui ? 8 : 5;
    const double cls_font_size = compact_ui ? 0.4 : 0.8;
    const int fps_y = compact_ui ? 40 : 36;
    const int cls_start_y = fps_y + perf_line_step + (compact_ui ? 8 : 6);
    const int cls_line_step = compact_ui ? 26 : 30;
    int i;
    int info_x;
    int info_y = fps_y;

    if (output_map_base == NULL || output_map_base == MAP_FAILED) {
        return;
    }

    /* ??????????????????????? ???????? ??? ?????? ???????????. */
    info_x = (int)app->display_width - perf_column_width - right_overlay_margin;
    if (info_x < left_overlay_margin) {
        info_x = left_overlay_margin;
    }

    for (i = 0; i < APP_MAX_MODELS; ++i) {
        const model_context_t *model = &app->models[i];
        Color_t color = colors[i];

        if (model->post_type == TELECHIPS_NPU_POST_DETECTOR) {
            /* detector??tracker?? ????????????????????????? */
            int j;
            Box_t boxes[256];
            int box_count = model->tracked_result.count;

            if (box_count > 256) {
                box_count = 256;
            }
            for (j = 0; j < box_count; ++j) {
                const tracked_object_t *obj = &model->tracked_result.objects[j];
                boxes[j].cls = obj->cls;
                boxes[j].track_id = obj->track_id;
                boxes[j].score = obj->score;
                boxes[j].xmin = (int)(obj->x_min + 0.5f);
                boxes[j].ymin = (int)(obj->y_min + 0.5f);
                boxes[j].xmax = (int)(obj->x_max + 0.5f);
                boxes[j].ymax = (int)(obj->y_max + 0.5f);
            }
            if (box_count > 0) {
                int box_image_width = (int)app->camera_width;
                int box_image_height = (int)app->camera_height;

                if (box_image_width <= 0) {
                    box_image_width = (int)app->camera_width;
                }
                if (box_image_height <= 0) {
                    box_image_height = (int)app->camera_height;
                }

                cvDrawBoxes(output_map_base, boxes, box_count,
                            app->display_width, app->display_height,
                            (uint32_t)box_image_width,
                            (uint32_t)box_image_height,
                            color, color, box_font_size, box_label_offset);
            }
        } else if (model->post_type == TELECHIPS_NPU_POST_CUSTOM) {
            app_custom_postproc_render(app, model, output_map_base);
        } else if (model->post_type == TELECHIPS_NPU_POST_CLASSIFIER) {
            cvDrawCls(output_map_base, app->display_width, app->display_height,
                      model->cls_result.class_ids[0], left_overlay_margin, cls_start_y + i * cls_line_step,
                      color, cls_font_size);
        }

        cvDrawInfo(output_map_base, app->display_width, app->display_height,
                   DRAW_INFO_NETWORK, model->perf.elapsed_in_us / 1000.0,
                   model->index, info_x, info_y, perf_font_size, color);
        info_y += perf_line_step;
        cvDrawInfo(output_map_base, app->display_width, app->display_height,
                   DRAW_INFO_NPU, model->npuUtilization,
                   model->index, info_x, info_y, perf_font_size, color);
        info_y += perf_line_step + perf_group_gap;
    }

    /* ?????? ???????? ???(FPS/CPU/MEM)????? ???????????. */
    cvDrawInfo(output_map_base, app->display_width, app->display_height,
               DRAW_INFO_FPS, app->perf.fps,
               0, left_overlay_margin, fps_y, perf_font_size, RGB(255, 255, 255));
    cvDrawInfo(output_map_base, app->display_width, app->display_height,
               DRAW_INFO_CPU, app->perf.cpuUtil[0],
               0, info_x, info_y, perf_font_size, RGB(255, 255, 255));
    info_y += perf_line_step;
    cvDrawInfo(output_map_base, app->display_width, app->display_height,
               DRAW_INFO_MEMORY, app->perf.memUsage,
               0, info_x, info_y, perf_font_size, RGB(255, 255, 255));
}

int render_output_frame(app_context_t *app)
{
    int output_idx = app->display_buffer_index % APP_DISPLAY_BUFFER_COUNT;

    if (app->memory.map_base_output[output_idx] == NULL ||
        app->memory.map_base_output[output_idx] == MAP_FAILED) {
        fprintf(stderr, "invalid output buffer mapping: %d\n", output_idx);
        return -1;
    }

    if (app->input_mode == APP_INPUT_CAMERA) {
        /* ?????????? ??? ??? ???????????????????????? ??????. */
        scaler_image_t src;
        scaler_image_t dst;

        src.paddr = app->camera_phys_addr;
        src.width = app->camera_width;
        src.height = app->camera_height;
        src.format = SCALER_FORMAT_ARGB8888;

        dst.paddr = app->memory.phy_base_output[output_idx];
        dst.width = app->display_width;
        dst.height = app->display_height;
        dst.format = SCALER_FORMAT_RGB888;

        if (scaler_resize(app->scaler, SCALER_INDEX_0, src, dst) != 0) {
            return -1;
        }
        if (scaler_poll(app->scaler, SCALER_INDEX_0) != 0) {
            return -1;
        }
    } else {
        /* ?? ??? TCP staging ?? Vision Protocol ?? ?? ??? ????. */
        scaler_image_t src;
        scaler_image_t dst;

        if (app->input_mode == APP_INPUT_TCP || app->input_mode == APP_INPUT_VISION) {
            uint64_t frame_phys = app_vision_frame_phys(app);
            if (frame_phys == 0) {
                return -1;
            }
            src.paddr = frame_phys;
            src.format = SCALER_FORMAT_RGB888;
        } else {
            if (app->tcp_stage_buf == NULL) {
                return -1;
            }
            src.paddr = app->tcp_stage_buf->paddr;
            src.format = SCALER_FORMAT_ARGB8888;
        }

        src.width = app->camera_width;
        src.height = app->camera_height;

        dst.paddr = app->memory.phy_base_output[output_idx];
        dst.width = app->display_width;
        dst.height = app->display_height;
        dst.format = SCALER_FORMAT_RGB888;

        if (scaler_resize(app->scaler, SCALER_INDEX_0, src, dst) != 0) {
            return -1;
        }
        if (scaler_poll(app->scaler, SCALER_INDEX_0) != 0) {
            return -1;
        }
    }

    /* ???????? ??? ??? detector/lane/perf ???????????. */
    overlay_results(app, app->memory.map_base_output[output_idx]);

    /* ???????? ?????overlay/display ????????????????? */
    if (display_show(app->display, app->memory.phy_base_output[output_idx], app->display_x,
                     app->display_y, app->display_width, app->display_height) != 0) {
        return -1;
    }

    app->display_buffer_index++;
    return 0;
}

