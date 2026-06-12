#version 330 core
in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D cubeTexture;

void main() {
    FragColor = texture(cubeTexture, texCoord);
}
