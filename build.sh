#!/bin/zsh
set -e

mkdir -p build

GLFW_PREFIX="$(brew --prefix glfw 2>/dev/null || true)"
if [[ -z "${GLFW_PREFIX}" ]]; then
    if [[ -d /opt/homebrew/opt/glfw ]]; then
        GLFW_PREFIX="/opt/homebrew/opt/glfw"
    elif [[ -d /usr/local/opt/glfw ]]; then
        GLFW_PREFIX="/usr/local/opt/glfw"
    else
        echo "Cannot find GLFW. Install it first with: brew install glfw"
        exit 1
    fi
fi

INCLUDE_DIR="${GLFW_PREFIX}/include"
LIB_DIR="${GLFW_PREFIX}/lib"

clang++ -std=c++17 -Wall -Wextra -I"${INCLUDE_DIR}" \
    01.cpp -L"${LIB_DIR}" -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -o build/exp01

clang++ -std=c++17 -Wall -Wextra -Wno-unused-function -I"${INCLUDE_DIR}" -I. \
    "14230107刘根森实验二/02.cpp" Shader.cpp -L"${LIB_DIR}" -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -o build/exp02

clang++ -std=c++17 -Wall -Wextra -I"${INCLUDE_DIR}" \
    03.cpp Shader.cpp -L"${LIB_DIR}" -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -o build/exp03

clang++ -std=c++17 -Wall -Wextra -I"${INCLUDE_DIR}" -I. \
    "14230107-刘根森-实验4/04.cpp" Shader.cpp -L"${LIB_DIR}" -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -o build/exp04

clang++ -std=c++17 -Wall -Wextra -I"${INCLUDE_DIR}" \
    05.cpp Shader.cpp -L"${LIB_DIR}" -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -o build/exp05

clang++ -std=c++17 -Wall -Wextra -I"${INCLUDE_DIR}" \
    final.cpp Shader.cpp -L"${LIB_DIR}" -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
    -framework ImageIO -framework CoreGraphics -framework CoreFoundation \
    -o build/final
