#version 330 core
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 emission;
    sampler2D diffuseTexture;
    float shininess;
    float alpha;
    bool useTexture;
};

struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

in vec3 fragPosition;
in vec3 normal;
in vec2 texCoord;

out vec4 FragColor;

uniform Material material;
uniform DirectionalLight sunLight;
uniform PointLight pointLights[2];
uniform vec3 viewPosition;
uniform vec3 fogColor;
uniform float fogDensity;

vec3 calculateDirectionalLight(DirectionalLight light, vec3 unitNormal, vec3 viewDirection) {
    vec4 texSample = material.useTexture ? texture(material.diffuseTexture, texCoord) : vec4(1.0);
    vec3 materialDiffuse = material.diffuse * texSample.rgb;
    vec3 materialAmbient = material.ambient * texSample.rgb;
    vec3 lightDirection = normalize(-light.direction);
    float diffuseStrength = max(dot(unitNormal, lightDirection), 0.0);
    vec3 reflectionDirection = reflect(-lightDirection, unitNormal);
    float specularStrength = pow(max(dot(viewDirection, reflectionDirection), 0.0), material.shininess);
    return light.ambient * materialAmbient
        + light.diffuse * diffuseStrength * materialDiffuse
        + light.specular * specularStrength * material.specular;
}

vec3 calculatePointLight(PointLight light, vec3 unitNormal, vec3 viewDirection) {
    vec4 texSample = material.useTexture ? texture(material.diffuseTexture, texCoord) : vec4(1.0);
    vec3 materialDiffuse = material.diffuse * texSample.rgb;
    vec3 materialAmbient = material.ambient * texSample.rgb;
    vec3 lightDirection = normalize(light.position - fragPosition);
    float diffuseStrength = max(dot(unitNormal, lightDirection), 0.0);
    vec3 reflectionDirection = reflect(-lightDirection, unitNormal);
    float specularStrength = pow(max(dot(viewDirection, reflectionDirection), 0.0), material.shininess);
    float distanceToLight = length(light.position - fragPosition);
    float attenuation = 1.0 / (
        light.constant
        + light.linear * distanceToLight
        + light.quadratic * distanceToLight * distanceToLight);
    return (
        light.ambient * materialAmbient
        + light.diffuse * diffuseStrength * materialDiffuse
        + light.specular * specularStrength * material.specular) * attenuation;
}

void main() {
    vec3 unitNormal = normalize(normal);
    if (!gl_FrontFacing) {
        unitNormal = -unitNormal;
    }
    vec3 viewDirection = normalize(viewPosition - fragPosition);
    vec3 color = calculateDirectionalLight(sunLight, unitNormal, viewDirection);
    for (int i = 0; i < 2; ++i) {
        color += calculatePointLight(pointLights[i], unitNormal, viewDirection);
    }
    float distanceToCamera = length(viewPosition - fragPosition);
    float fogAmount = 1.0 - exp(-fogDensity * distanceToCamera * distanceToCamera);
    fogAmount = clamp(fogAmount, 0.0, 0.68);
    color = mix(color, fogColor, fogAmount) + material.emission;
    float texAlpha = material.useTexture ? texture(material.diffuseTexture, texCoord).a : 1.0;
    FragColor = vec4(color, material.alpha * texAlpha);
}
