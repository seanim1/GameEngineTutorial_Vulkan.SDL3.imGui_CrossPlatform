#!/usr/bin/env bash
set -e

mkdir -p spv
glslangValidator -V shaders/shader.vert -o spv/vert.spv
glslangValidator -V shaders/shader.frag -o spv/frag.spv

echo "Shaders compiled to spv/"