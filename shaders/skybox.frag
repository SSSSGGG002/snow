#version 330 core
in vec3 texCoord;

out vec4 FragColor;

uniform sampler2D environmentMap;
uniform float nightTint;

const float PI = 3.14159265358979323846;

vec2 directionToEquirectUV(vec3 direction) {
    vec3 d = normalize(direction);
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
    float v = asin(clamp(d.y, -1.0, 1.0)) / PI + 0.5;
    return vec2(u, v);
}

void main() {
    vec3 color = texture(environmentMap, directionToEquirectUV(texCoord)).rgb;
    vec3 nightColor = color * vec3(0.38, 0.46, 0.62);
    FragColor = vec4(mix(color, nightColor, nightTint), 1.0);
}
