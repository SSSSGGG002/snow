#version 330 core
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vColor;

out vec3 vertexColor;
uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(vPosition, 1.0);
    vertexColor = vColor;
}
