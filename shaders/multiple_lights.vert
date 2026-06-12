#version 330 core
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;

out vec3 fragPosition;
out vec3 normal;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    fragPosition = vec3(model * vec4(vPosition, 1.0));
    normal = mat3(transpose(inverse(model))) * vNormal;
    texCoord = vTexCoord;
    gl_Position = projection * view * vec4(fragPosition, 1.0);
}
