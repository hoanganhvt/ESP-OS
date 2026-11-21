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

enum { LINE_SIZE = 128, HISTORY_SIZE = 8 };

typedef struct {
    char entries[HISTORY_SIZE][LINE_SIZE];
    size_t next;
    size_t count;
} shell_history_t;

static shell_history_t history;

// Rewrite the tail while leaving the terminal cursor at the editing position.
static void redraw_tail(const char *tail)
{
    shell_print("\x1b" "7");
    shell_print(tail);
    shell_print("\x1b[K\x1b" "8");
}

static void shell_task(void *arg)
{
    (void)arg;
    char line[LINE_SIZE] = "";
    char draft[LINE_SIZE];
    size_t draft_cursor = 0;
    size_t history_age = 0;
    size_t cursor = 0;
    enum { NORMAL, ESCAPE, ARROW, SKIP_SEQUENCE } input_state = NORMAL;
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
            input_state = NORMAL;
            putchar('\n');
            if (overflow) {
                shell_print("Line too long\n");
            } else {
                if (strspn(line, " ") < length) {
                    strcpy(history.entries[history.next], line);
                    history.next = (history.next + 1) % HISTORY_SIZE;
                    if (history.count < HISTORY_SIZE) {
                        ++history.count;
                    }
                }
                run_line(line);
            }
            length = 0;
            cursor = 0;
            line[0] = '\0';
            history_age = 0;
            overflow = false;
            shell_print("> ");
        } else if (overflow) {
            continue;  // Discard the entire oversized command through Enter.
        } else if (ch == 0x1b) {
            input_state = ESCAPE;
        } else if (input_state == ESCAPE && (ch == '[' || ch == 'O')) {
            input_state = ARROW;
        } else if (input_state == ARROW || input_state == SKIP_SEQUENCE) {
            if (ch >= 0x20 && ch <= 0x3f) {
                input_state = SKIP_SEQUENCE;
                continue;
            }
            bool plain = input_state == ARROW;
            input_state = NORMAL;
            if (!plain) {
                continue;
            }
            if (ch == 'D' && cursor > 0) {
                --cursor;
                putchar('\b');
            } else if (ch == 'C' && cursor < length) {
                putchar(line[cursor++]);
            } else if ((ch == 'A' && history_age < history.count) ||
                       (ch == 'B' && history_age > 0)) {
                if (history_age == 0) {
                    strcpy(draft, line);
                    draft_cursor = cursor;
                }
                history_age = ch == 'A' ? history_age + 1 : history_age - 1;
                size_t index = (history.next + HISTORY_SIZE - history_age) % HISTORY_SIZE;
                strcpy(line, history_age ? history.entries[index] : draft);
                length = strlen(line);
                cursor = history_age ? length : draft_cursor;
                shell_print("\r> ");
                fwrite(line, 1, cursor, stdout);
                redraw_tail(line + cursor);
            }
        } else if (ch == '\b' || ch == 127) {
            input_state = NORMAL;
            if (cursor > 0) {
                memmove(line + cursor - 1, line + cursor, length - cursor + 1);
                --cursor;
                --length;
                putchar('\b');
                redraw_tail(line + cursor);
            }
        } else if (ch >= ' ' || ch == '\t') {
            input_state = NORMAL;
            if (length == sizeof(line) - 1) {
                overflow = true;
                putchar('\a');
            } else {
                memmove(line + cursor + 1, line + cursor, length - cursor + 1);
                line[cursor++] = ch == '\t' ? ' ' : (char)ch;
                ++length;
                putchar(line[cursor - 1]);
                if (cursor < length) {
                    redraw_tail(line + cursor);
                }
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
