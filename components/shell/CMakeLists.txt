idf_component_register(
    SRCS "shell.c"
         "commands/commands.c"
         "commands/help/help.c"
         "commands/echo/echo.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_stdio esp_driver_uart freertos
)
