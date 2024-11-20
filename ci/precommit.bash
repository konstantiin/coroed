#!/bin/bash

set -e

cd "$(dirname "$0")"/..

COMPILERS=("gcc" "clang")
OPTIMIZATION_LEVELS=("-O0" "-O3")
SANITIZERS=("" "address,leak")

for compiler in "${COMPILERS[@]}"; do
    for optimization_level in "${OPTIMIZATION_LEVELS[@]}"; do
        for sanitizer in "${SANITIZERS[@]}"; do
            if [[ -z "$sanitizers" ]]; then
                SAN=""
            else
                SAN="-fsanitize=$sanitizers"
            fi

            echo "======= Building '$compiler', '$optimization_level', '$sanitizer' ======="
            make clean
            bear -- make compile \
                COMPILER="$compiler" \
                OPTIMIZATION_LEVEL="$optimization_level" \
                SANITIZERS="$SAN"
            for i in {1..8}; do
                echo "|- Running #$i..."
                ./build/bin/app 1>/dev/null
            done
        done
    done
done
