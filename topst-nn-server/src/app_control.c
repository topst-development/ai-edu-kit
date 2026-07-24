#define _DEFAULT_SOURCE

#include "app_control.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

static app_context_t *g_app_ctx;
static int g_keyboard_enabled;
static pthread_t g_keyboard_thread;

static void app_control_on_signal(int sig)
{
    (void)sig;
    if (g_app_ctx != NULL) {
        g_app_ctx->stop = 1;
    }
}

static void *app_control_keyboard_thread(void *arg)
{
    app_context_t *app = (app_context_t *)arg;

    while (!app->stop) {
        char line[32];
        fd_set readfds;
        struct timeval timeout;
        int ret;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;

        ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (ret <= 0 || !FD_ISSET(STDIN_FILENO, &readfds)) {
            continue;
        }

        if (fgets(line, sizeof(line), stdin) == NULL) {
            continue;
        }
        if (line[0] == 'x' || line[0] == 'X') {
            app->stop = 1;
            raise(SIGINT);
            break;
        }
    }

    return NULL;
}


void app_control_install_signal_handlers(app_context_t *app)
{
    g_app_ctx = app;
    signal(SIGINT, app_control_on_signal);
    signal(SIGTERM, app_control_on_signal);
}

void app_control_uninstall_signal_handlers(void)
{
    g_app_ctx = NULL;
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
}

int app_control_start_keyboard(app_context_t *app)
{
    if (!isatty(STDIN_FILENO)) {
        return 0;
    }

    if (pthread_create(&g_keyboard_thread, NULL, app_control_keyboard_thread, app) != 0) {
        return -1;
    }

    g_keyboard_enabled = 1;
    return 0;
}


void app_control_stop_keyboard(void)
{
    if (g_keyboard_enabled && g_app_ctx != NULL) {
        g_app_ctx->stop = 1;
        (void)pthread_join(g_keyboard_thread, NULL);
        g_keyboard_enabled = 0;
    }
}
