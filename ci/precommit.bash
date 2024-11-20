#!/bin/bash

set -e

cd "$(dirname "$0")"/..

COMPILERS=("gcc" "clang")
OPTIMIZATION_LEVELS=("-O0" "-O3")
SANITIZERS=("" "address,leak")

for compiler in "${COMPILERS[@]}"; do
    for optimization_level in "${OPTIMIZATION_LEVELS[@]}"; do
        for sanitizer in "${SANITIZERS[@]}"; do
            echo "======= Running '$compiler', '$optimization_level', '$sanitizer' ======="
            make \
                COMPILER="$compiler" \
                OPTIMIZATION_LEVEL="$optimization_level" \
                SANITIZERS="-fsanitize=$sanitizer"
        done
    done
done
