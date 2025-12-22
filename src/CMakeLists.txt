add_library(driver INTERFACE)

target_sources(driver INTERFACE
    sht3x.c
)

target_include_directories(driver INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(test)
