#!/usr/bin/env bash
set -e

glslangValidator -V shaders/shader.vert -o vert.spv
glslangValidator -V shaders/shader.frag -o frag.spv
