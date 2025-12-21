#!/usr/bin/env bash
set -e

cmake -GNinja -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --
./build/src/test
