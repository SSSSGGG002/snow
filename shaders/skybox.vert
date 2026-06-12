#version 330 core
layout (location = 0) in vec3 vPosition;

out vec3 texCoord;

uniform mat4 view;
uniform mat4 projection;

void main() {
    vec3 direction = vPosition * 2.0;
    texCoord = direction;
    vec4 position = projection * view * vec4(direction, 1.0);
    gl_Position = position.xyww;
}
