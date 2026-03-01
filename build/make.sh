#!/bin/bash

# Compile script for Policy Executor

set -e

echo "Compiling Policy Executor..."

# Check for g++
if ! command -v g++ &> /dev/null; then
    echo "Error: g++ could not be found."
    exit 1
fi

SOURCE_DIR="../loadstar/Policy Executor"
OUTPUT_DIR="../loadstar/Policy Executor"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory $SOURCE_DIR not found."
    exit 1
fi

# Compile
g++ -std=c++17 -O2 -o "$OUTPUT_DIR/policy_executor" "$SOURCE_DIR/main.cpp"

if [ -f "$OUTPUT_DIR/policy_executor" ]; then
    echo "Compilation successful! Executable created at $OUTPUT_DIR/policy_executor"
else
    echo "Compilation failed."
    exit 1
fi
