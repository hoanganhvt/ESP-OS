#include "shell.h"
#include "commands/command.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart_vfs.h"
#include "esp_stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void shell_print(const char *text)
{
    while (*text) {
        putchar(*text++);
    }
}

static void run_line(char *line)
{
    char *argv[16];
    int argc = 0;
    char *out = line;

    while (*line) {
        if (*line == ' ' || *line == '\t') {
            ++line;
            continue;
        }
        if (argc == sizeof(argv) / sizeof(argv[0])) {
            shell_print("Too many arguments\n");
            return;
        }

        argv[argc++] = out;
        bool quoted = false;
        while (*line && (quoted || (*line != ' ' && *line != '\t'))) {
            if (*line == '"') {
                quoted = !quoted;
                ++line;
            } else {
                if (*line == '\\' && line[1]) {
                    ++line;
                }
                *out++ = *line++;
            }
        }
        if (quoted) {
            shell_print("Unclosed quote\n");
            return;
        }
        if (*line) {
            ++line;
        }
        *out++ = '\0';
    }

    if (argc == 0) {
        return;
    }
    for (size_t i = 0; i < shell_command_count; ++i) {
        if (strcmp(shell_commands[i]->name, argv[0]) == 0) {
            shell_commands[i]->run(argc, argv);
            return;
        }
    }
    shell_print("Unknown command: ");
    shell_print(argv[0]);
    putchar('\n');
}

static void shell_task(void *arg)
{
    (void)arg;
    char line[128];
    size_t length = 0;
    bool overflow = false;
    bool after_cr = false;
    setvbuf(stdin, NULL, _IONBF, 0);
    shell_print("> ");
    for (;;) {
        fflush(stdout);
        int ch = getchar();
        if (ch == EOF) {
            clearerr(stdin);
            vTaskDelay(1);
            continue;
        }

        bool skip_lf = after_cr && ch == '\n';
        after_cr = ch == '\r';
        if (skip_lf) {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            putchar('\n');
            if (overflow) {
                shell_print("Line too long\n");
            } else {
                line[length] = '\0';
                run_line(line);
            }
            length = 0;
            overflow = false;
            shell_print("> ");
        } else if (overflow) {
            continue;  // Discard the entire oversized command through Enter.
        } else if (ch == '\b' || ch == 127) {
            if (length > 0) {
                --length;
                shell_print("\b \b");
            }
        } else if (ch >= ' ' || ch == '\t') {
            if (length == sizeof(line) - 1) {
                overflow = true;
                putchar('\a');
            } else {
                line[length++] = ch == '\t' ? ' ' : (char)ch;
                putchar(line[length - 1]);
            }
        }
    }
}

esp_err_t shell_start(void)
{
    esp_err_t err = esp_stdio_install_io_driver();
    if (err != ESP_OK) {
        return err;
    }
#if CONFIG_ESP_CONSOLE_UART
    // Keep CR and LF distinct so a terminal sending CRLF submits just once.
    if (uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_LF) < 0) {
        esp_stdio_uninstall_io_driver();
        return ESP_FAIL;
    }
#endif
    if (xTaskCreate(shell_task, "shell", 4096, NULL, 5, NULL) != pdPASS) {
        esp_stdio_uninstall_io_driver();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
