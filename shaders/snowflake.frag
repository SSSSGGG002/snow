#version 330 core
in vec3 vertexColor;
out vec4 FragColor;

uniform vec3 overrideColor;
uniform bool useOverrideColor;
uniform float alpha;

void main() {
    if (useOverrideColor) {
        FragColor = vec4(overrideColor, alpha);
    } else {
        FragColor = vec4(vertexColor, alpha);
    }
}
