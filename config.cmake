set(PROJECT_NAME "p101-report")
set(PROJECT_VERSION "1.0.0")
set(PROJECT_DESCRIPTION "Correlates p101 resource and call logs into one report")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Common compiler flags
set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)

set(DARWIN_STANDARD_FLAGS
        -D_DARWIN_C_SOURCE
)

set(LINUX_STANDARD_FLAGS
)

set(BSD_STANDARD_FLAGS
)

# Define targets
set(EXECUTABLE_TARGETS main)
set(LIBRARY_TARGETS "")
set(main_OUTPUT_NAME p101-report)

set(main_SOURCES
        src/bundle.c
        src/cli.c
        src/finding.c
        src/io.c
        src/json.c
        src/lifetime.c
        src/lifetime_common.c
        src/lifetime_mermaid.c
        src/main.c
        src/memory.c
        src/model.c
        src/model_generic.c
        src/model_support.c
        src/output.c
        src/parse.c
        src/reader.c
        src/runner.c
)

set(main_HEADERS
        include/arguments.h
        include/bundle.h
        include/cli.h
        include/constants.h
        include/errors.h
        include/finding.h
        include/io.h
        include/json.h
        include/lifetime.h
        include/lifetime_common.h
        include/lifetime_mermaid.h
        include/memory.h
        include/model.h
        include/model_generic.h
        include/model_support.h
        include/output.h
        include/parse.h
        include/reader.h
        include/runner.h
        include/types.h
)

set(main_LINK_LIBRARIES
        p101_error
        p101_tool_event
        p101_env
        p101_c
        p101_posix
        m
)
