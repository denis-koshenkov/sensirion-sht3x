cmake_minimum_required(VERSION 4.0.3)

project(sensirion-sht3x)

# The policy allows us to change options of external projects using set()
# Needed for CppUTest
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_subdirectory(src)
