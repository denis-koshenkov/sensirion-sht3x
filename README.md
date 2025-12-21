# Sensirion SHT3X Driver
Driver for Sensirion SHT3X temperature & humidity sensor.

# How to build and execute tests
Execute the commands from the repository root directory.

Build test executable:
```
cmake -GNinja -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --
```

Run test executable:
```
./build/src/test
```
