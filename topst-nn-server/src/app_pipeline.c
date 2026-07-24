#include "app_pipeline.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "app_inference.h"
#include "app_input.h"
#include "app_json.h"
#include "app_monitor.h"
#include "app_render.h"
#include "app_tracker.h"

static void app_pipeline_print_controls(const app_context_t *app)
{
    if (isatty(STDOUT_FILENO) && !app->verbose) {
        printf("\n");
        printf("\n");
    }

    printf(" =========================\n");
    printf(" Demo : topst-nn-app Demo\n");
    printf(" =========================\n\n\n");
    printf(" x: Exit\n\n");
    printf(" Enter Choice: ");
    fflush(stdout);
}


static const char *app_pipeline_post_type_name(int post_type)
{
    if (post_type == TELECHIPS_NPU_POST_DETECTOR) {
        return "obj";
    }
    if (post_type == TELECHIPS_NPU_POST_CLASSIFIER) {
        return "class";
    }
    if (post_type == TELECHIPS_NPU_POST_CUSTOM) {
        return "custom";
    }
    return "none";
}

static int app_pipeline_model_count(const model_context_t *model)
{
    if (model->post_type == TELECHIPS_NPU_POST_DETECTOR) {
        return model->tracked_result.count;
    }
    if (model->post_type == TELECHIPS_NPU_POST_CLASSIFIER) {
        return model->cls_result.class_ids[0];
    }
    if (model->post_type == TELECHIPS_NPU_POST_CUSTOM) {
        return (model->lane_data != NULL) ? model->lane_data->num_lanes : 0;
    }
    return -1;
}

static const char *app_pipeline_model_value_name(const model_context_t *model)
{
    if (model->post_type == TELECHIPS_NPU_POST_DETECTOR) {
        return "det";
    }
    if (model->post_type == TELECHIPS_NPU_POST_CLASSIFIER) {
        return "class";
    }
    if (model->post_type == TELECHIPS_NPU_POST_CUSTOM) {
        return "lanes";
    }
    return "value";
}

static void app_pipeline_print_detector_status(int index, const model_context_t *model)
{
    int selected[2] = {-1, -1};
    int count = model->tracked_result.count;
    int show_count = (count < 2) ? count : 2;
    int rank;

    printf("[Run][NPU%d] type=obj det=%d", index, count);

    for (rank = 0; rank < show_count; ++rank) {
        int best = -1;
        int i;

        for (i = 0; i < count; ++i) {
            int used = 0;
            int j;

            for (j = 0; j < rank; ++j) {
                if (selected[j] == i) {
                    used = 1;
                    break;
                }
            }
            if (used) {
                continue;
            }
            if (best < 0 ||
                model->tracked_result.objects[i].score > model->tracked_result.objects[best].score) {
                best = i;
            }
        }

        if (best >= 0) {
            const tracked_object_t *obj = &model->tracked_result.objects[best];
            selected[rank] = best;
            printf(" obj%d={cls=%d score=%.2f box=(%.0f,%.0f,%.0f,%.0f)}",
                   rank,
                   obj->cls,
                   obj->score,
                   obj->x_min,
                   obj->y_min,
                   obj->x_max,
                   obj->y_max);
        }
    }

    if (count > show_count) {
        printf(" ...");
    }
    printf("\033[K");
}

static void app_pipeline_print_model_status(const app_context_t *app, int index)
{
    const model_context_t *model = &app->models[index];

    if (model->post_type == TELECHIPS_NPU_POST_DETECTOR) {
        app_pipeline_print_detector_status(index, model);
        return;
    }

    printf("[Run][NPU%d] type=%s %s=%d\033[K",
           index,
           app_pipeline_post_type_name(model->post_type),
           app_pipeline_model_value_name(model),
           app_pipeline_model_count(model));
}


static void app_pipeline_update_status(const app_context_t *app)
{
    (void)app;
}



int app_pipeline_run(app_context_t *app)
{
    app_pipeline_print_controls(app);

    while (!app->stop) {
        inference_task_t tasks[APP_MAX_MODELS];
        pthread_t threads[APP_MAX_MODELS];
        int i;

        /* 프레임 1장을 입력받은 뒤, 그 프레임에 대해 전체 인지 파이프라인을 수행한다. */
        if (app->verbose) { fprintf(stderr, "[stage] acquire begin\n"); fflush(stderr); }
        if (acquire_input_frame(app) <= 0) {
            app_json_poll_accept(app);
            continue;
        }
        if (app->verbose) { fprintf(stderr, "[stage] acquire done\n"); fflush(stderr); }
        app_json_poll_accept(app);

        /* 공용 입력 프레임을 각 모델의 NPU 입력 버퍼 크기에 맞게 리사이즈한다. */
        if (app->verbose) { fprintf(stderr, "[stage] prepare begin\n"); fflush(stderr); }
        if (prepare_model_input(app, &app->models[0], SCALER_INDEX_0) != 0 ||
            prepare_model_input(app, &app->models[1], SCALER_INDEX_1) != 0) {
            fprintf(stderr, "failed to prepare input frame\n");
            release_input_frame(app);
            return -1;
        }

        /* 추론이 가장 무거운 단계이므로 두 모델은 병렬 스레드로 실행한다. */
        for (i = 0; i < APP_MAX_MODELS; ++i) {
            memset(&tasks[i], 0, sizeof(tasks[i]));
            tasks[i].model = &app->models[i];
            if (pthread_create(&threads[i], NULL, run_inference_thread, &tasks[i]) != 0) {
                fprintf(stderr, "failed to create inference thread for model%d\n", i);
                app->stop = 1;
                release_input_frame(app);
                return -1;
            }
        }

        for (i = 0; i < APP_MAX_MODELS; ++i) {
            (void)pthread_join(threads[i], NULL);
            if (tasks[i].run_status != 0) {
                fprintf(stderr, "inference failed for model%d\n", i);
                app->stop = 1;
            }
        }

        /* 후처리는 raw NPU 출력을 detector/lane/classifier 결과 구조체로 변환한다. */
        for (i = 0; i < APP_MAX_MODELS; ++i) {
            if (postprocess_model(&app->models[i]) != 0) {
                fprintf(stderr, "postprocess failed for model%d\n", i);
                app->stop = 1;
            }
        }
        if (app->stop) {
            release_input_frame(app);
            return -1;
        }

        /* 트래커는 detector 결과를 받아 안정적인 track ID를 생성해 렌더링/JSON에 넘긴다. */
        app_tracker_update(app);
        app_monitor_update_fps(app);
        app->frame_index++;
        if (app->verbose) { fprintf(stderr, "[stage] json begin\n"); fflush(stderr); }
        (void)app_json_send_results(app);
        if (app->verbose) { fprintf(stderr, "[stage] json done\n"); fflush(stderr); }

        if (app->render_enabled) {
            if (app->verbose) { fprintf(stderr, "[stage] render begin\n"); fflush(stderr); }
            if (render_output_frame(app) != 0) {
                fprintf(stderr, "failed to display frame\n");
                release_input_frame(app);
                return -1;
            }
        }

        if (app->verbose) { fprintf(stderr, "[stage] render done\n"); fflush(stderr); }
        app_pipeline_update_status(app);
        release_input_frame(app);
    }

    if (isatty(STDOUT_FILENO) && !app->verbose) {
        printf("\n");
    }

    return 0;
}
