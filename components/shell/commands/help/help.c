#include "commands/command.h"

static void run_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    for (size_t i = 0; i < shell_command_count; ++i) {
        shell_print(shell_commands[i]->name);
        shell_print(" - ");
        shell_print(shell_commands[i]->description);
        shell_print("\n");
    }
}

const shell_command_t help_command = {
    .name = "help",
    .description = "List available commands",
    .run = run_help,
};
