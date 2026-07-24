#include "app_config.h"

#include "custom_postproc.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void app_config_print_banner(void)
{
    printf("\n");
    printf(" ===================================================================================================\n");
    printf(" ==                                                                                               ==\n");
    printf(" ==   TOPST Model Zoo Runtime                                                                     ==\n");
    printf(" ==   Shared tc-nn runtime for camera/display and Vision Protocol demos                           ==\n");
    printf(" ==                                                                                               ==\n");
    printf(" ===================================================================================================\n");
    printf("\n");
    printf(" topst-nn-server Demo\n");
    printf(" ========================================================\n\n");
}

static void app_config_print_summary(const app_context_t *app)
{
    printf("----------------Parameter Info----------------\n\n");
    printf("[Network1]Network Path         : %s\n", app->models[0].path);
    printf("[Network2]Network Path         : %s\n\n", app->models[1].path);

    printf("[Common]Input Mode             : %s\n", input_mode_to_string(app->input_mode));
    printf("[Common]Output Mode            : %s\n", app->render_enabled ? "display" : "none");
    printf("[Common]JSON Result            : %s\n", app->json_enabled ? "on" : "off");
    printf("[Common]Custom Postprocess     : %s\n", app_custom_postproc_name());
    printf("[Common]Input Size             : %u x %u\n", app->camera_width, app->camera_height);
    printf("[Common]Output Size            : %u x %u\n", app->display_width, app->display_height);

    if (app->input_mode == APP_INPUT_VISION) {
        printf("[Common]Vision Target          : %s\n", app->vision.target_ip);
        printf("[Common]Vision Stream Port     : %d\n", app->vision.stream_port);
        printf("[Common]Vision Message Port    : %d\n", app->vision.message_port);
    } else {
        printf("[Common]Input Path             : %s\n", app->camera_device);
    }

    printf("[Common]Output Path            : %s\n", app->display_device);
    printf("[Common]Output Position X      : %u\n", app->display_x);
    printf("[Common]Output Position Y      : %u\n\n", app->display_y);

    printf("[Debug]Debug Mode              : %s\n", app->verbose ? "on" : "off");
    printf("[Debug]NPU Running Mode        : SyncMode\n");
    printf("----------------Parameter End----------------\n\n");
}

void app_config_print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  -n <dir>   Cluster 0 model directory, default: %s\n"
            "  -N <dir>   Cluster 1 model directory, default: %s\n"
            "  -c <path>  Camera device, default: %s\n"
            "  -d <path>  Display device, default: %s\n"
            "  -w <num>   Source frame width, default: %d\n"
            "  -h <num>   Source frame height, default: %d\n"
            "  -W <num>   Display width, default: %d\n"
            "  -H <num>   Display height, default: %d\n"
            "  -x <num>   Display x position, default: 0\n"
            "  -y <num>   Display y position, default: 0\n"
            "  -t <num>   Timeout ms, default: 1000\n"
            "  -v         Verbose perf logging\n"
            "  --vision   Use Vision Protocol input instead of camera input\n"
            "  --vision-target <ip>  PC/RTPM Vision Protocol server IP, default: %s\n"
            "  --stream-port <num>   Vision stream port, default: %d\n"
            "  --message-port <num>  Vision message port, default: %d\n"
            "\n"
            "Default mode uses camera input and display output only.\n"
            "JSON result messages are disabled in default camera mode.\n"
            "Vision mode expects raw RGB888 frames of size width*height*3 bytes.\n"
            "Press 'x' in the terminal to stop the app cleanly.\n",
            prog, DEFAULT_MODEL0_DIR, DEFAULT_MODEL1_DIR,
            DEFAULT_CAMERA_DEVICE, DEFAULT_DISPLAY_DEVICE,
            DEFAULT_CAMERA_WIDTH, DEFAULT_CAMERA_HEIGHT,
            DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT,
            DEFAULT_VISION_TARGET_IP,
            DEFAULT_VISION_STREAM_PORT, DEFAULT_VISION_MESSAGE_PORT);
}

void app_config_set_defaults(app_context_t *app)
{
    memset(app, 0, sizeof(*app));

    (void)snprintf(app->camera_device, sizeof(app->camera_device), "%s",
                   DEFAULT_CAMERA_DEVICE);
    (void)snprintf(app->display_device, sizeof(app->display_device), "%s",
                   DEFAULT_DISPLAY_DEVICE);
    (void)snprintf(app->scaler_device[SCALER_INDEX_0],
                   sizeof(app->scaler_device[SCALER_INDEX_0]), "%s",
                   DEFAULT_SCALER0_DEVICE);
    (void)snprintf(app->scaler_device[SCALER_INDEX_1],
                   sizeof(app->scaler_device[SCALER_INDEX_1]), "%s",
                   DEFAULT_SCALER1_DEVICE);
    (void)snprintf(app->vision.target_ip, sizeof(app->vision.target_ip), "%s",
                   DEFAULT_VISION_TARGET_IP);

    app->camera_width = DEFAULT_CAMERA_WIDTH;
    app->camera_height = DEFAULT_CAMERA_HEIGHT;
    app->display_width = DEFAULT_DISPLAY_WIDTH;
    app->display_height = DEFAULT_DISPLAY_HEIGHT;
    app->timeout_ms = 1000;
    app->input_mode = APP_INPUT_CAMERA;
    app->json_enabled = 0;
    app->render_enabled = 1;
    app->tcp_input.server_fd = -1;
    app->tcp_input.client_fd = -1;
    app->tcp_input.port = DEFAULT_TCP_PORT;
    app->json_output.server_fd = -1;
    app->json_output.client_fd = -1;
    app->json_output.port = DEFAULT_JSON_PORT;
    app->vision.stream_port = DEFAULT_VISION_STREAM_PORT;
    app->vision.message_port = DEFAULT_VISION_MESSAGE_PORT;

    app->models[0].index = 0;
    app->models[0].cluster = 0;
    (void)snprintf(app->models[0].path, sizeof(app->models[0].path), "%s",
                   DEFAULT_MODEL0_DIR);
    app->models[1].index = 1;
    app->models[1].cluster = 1;
    (void)snprintf(app->models[1].path, sizeof(app->models[1].path), "%s",
                   DEFAULT_MODEL1_DIR);
}

int app_config_parse_args(app_context_t *app, int argc, char **argv)
{
    int opt;
    static const struct option long_options[] = {
        {"vision", no_argument, NULL, 1000},
        {"vision-target", required_argument, NULL, 1001},
        {"stream-port", required_argument, NULL, 1002},
        {"message-port", required_argument, NULL, 1003},
        {0, 0, 0, 0},
    };

    while ((opt = getopt_long(argc, argv, "i:n:N:c:d:w:h:W:H:x:y:t:v",
                              long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                if (strcmp(optarg, "camera") == 0) {
                    app->input_mode = APP_INPUT_CAMERA;
                    app->json_enabled = 0;
                } else if (strcmp(optarg, "vision") == 0) {
                    app->input_mode = APP_INPUT_VISION;
                    app->json_enabled = 1;
                } else if (strcmp(optarg, "tcp") == 0) {
                    app->input_mode = APP_INPUT_VISION;
                    app->json_enabled = 1;
                    (void)snprintf(app->vision.target_ip, sizeof(app->vision.target_ip), "%s",
                                   DEFAULT_TCP_VISION_TARGET_IP);
                    app->vision.stream_port = DEFAULT_VISION_STREAM_PORT;
                    app->vision.message_port = DEFAULT_VISION_MESSAGE_PORT;
                } else {
                    fprintf(stderr, "unsupported input mode: %s\n", optarg);
                    return -1;
                }
                break;
            case 'n':
                (void)snprintf(app->models[0].path, sizeof(app->models[0].path), "%s", optarg);
                break;
            case 'N':
                (void)snprintf(app->models[1].path, sizeof(app->models[1].path), "%s", optarg);
                break;
            case 'c':
                (void)snprintf(app->camera_device, sizeof(app->camera_device), "%s", optarg);
                break;
            case 'd':
                (void)snprintf(app->display_device, sizeof(app->display_device), "%s", optarg);
                break;
            case 'w':
                app->camera_width = (uint32_t)atoi(optarg);
                break;
            case 'h':
                app->camera_height = (uint32_t)atoi(optarg);
                break;
            case 'W':
                app->display_width = (uint32_t)atoi(optarg);
                break;
            case 'H':
                app->display_height = (uint32_t)atoi(optarg);
                break;
            case 'x':
                app->display_x = (uint32_t)atoi(optarg);
                break;
            case 'y':
                app->display_y = (uint32_t)atoi(optarg);
                break;
            case 't':
                app->timeout_ms = atoi(optarg);
                break;
            case 'v':
                app->verbose = 1;
                break;
            case 1000:
                app->input_mode = APP_INPUT_VISION;
                app->json_enabled = 1;
                break;
            case 1001:
                app->input_mode = APP_INPUT_VISION;
                app->json_enabled = 1;
                (void)snprintf(app->vision.target_ip, sizeof(app->vision.target_ip), "%s", optarg);
                break;
            case 1002:
                app->input_mode = APP_INPUT_VISION;
                app->json_enabled = 1;
                app->vision.stream_port = atoi(optarg);
                break;
            case 1003:
                app->input_mode = APP_INPUT_VISION;
                app->json_enabled = 1;
                app->vision.message_port = atoi(optarg);
                break;
            default:
                return -1;
        }
    }

    app->models[0].timeout_ms = app->timeout_ms;
    app->models[1].timeout_ms = app->timeout_ms;
    app->models[0].verbose = app->verbose;
    app->models[1].verbose = app->verbose;

    app_config_print_banner();
    app_config_print_summary(app);

    return 0;
}
