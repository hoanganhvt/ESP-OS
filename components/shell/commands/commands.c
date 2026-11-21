#include "command.h"

extern const shell_command_t help_command;
extern const shell_command_t echo_command;

const shell_command_t *const shell_commands[] = {
    &help_command,
    &echo_command,
};

const size_t shell_command_count = sizeof(shell_commands) / sizeof(shell_commands[0]);
