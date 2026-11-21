#include <stdio.h>

#include "os.h"
#include "shell.h"

void os_init(void)
{
    printf("Initializing OS...\n");
}

void os_start(void)
{
    esp_err_t err = shell_start();
    if (err != ESP_OK) {
        printf("Failed to start shell: %s\n", esp_err_to_name(err));
    }
}
