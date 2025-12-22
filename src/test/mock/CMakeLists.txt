target_sources(test PRIVATE
    mock_cfg_functions.cpp
)

target_include_directories(test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
