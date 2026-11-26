file(GLOB_RECURSE UI_SOURCES "ui/*.c")

idf_component_register(
    SRCS
        "main.c"
        "drivers/ina226/ina226.c"
        "drivers/mcp4725/mcp4725.c"
        ${UI_SOURCES}
    INCLUDE_DIRS
        "."
        "drivers/ina226"
        "drivers/mcp4725"
        "ui"
)