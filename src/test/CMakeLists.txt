add_executable(test)

target_sources(test PRIVATE
    main.cpp
    sht3x.cpp
)

set(TESTS OFF) # Disable cpputest self-tests
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/cpputest
    ${CMAKE_CURRENT_BINARY_DIR}/cpputest
)

target_link_libraries(test PRIVATE
    CppUTest
    CppUTestExt
    driver
)
