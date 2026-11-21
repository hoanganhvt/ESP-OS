#pragma once

#include <stddef.h>

typedef struct {
    const char *name;
    const char *description;
    void (*run)(int argc, char **argv);
} shell_command_t;

extern const shell_command_t *const shell_commands[];
extern const size_t shell_command_count;

void shell_print(const char *text);
