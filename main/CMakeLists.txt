file(GLOB_RECURSE UI_SOURCES "ui/*.c")

idf_component_register(
    SRCS
        "main.c"
        "drivers/ina226/ina226.c"
        ${UI_SOURCES}
    INCLUDE_DIRS
        "."
        "drivers/ina226"
        "ui"
)