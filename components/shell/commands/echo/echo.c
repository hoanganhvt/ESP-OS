#include "commands/command.h"

static void run_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            shell_print(" ");
        }
        shell_print(argv[i]);
    }
    shell_print("\n");
}

const shell_command_t echo_command = {
    .name = "echo",
    .description = "Print the supplied arguments",
    .run = run_echo,
};
