#version 330 core
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
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

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float innerCutoff;
    float outerCutoff;
};

in vec3 fragPosition;
in vec3 normal;
in vec2 texCoord;

out vec4 FragColor;

uniform Material material;
uniform DirectionalLight directionalLight;
uniform PointLight pointLights[4];
uniform SpotLight spotLight;
uniform vec3 viewPosition;

vec3 calculateDirectionalLight(DirectionalLight light, vec3 unitNormal, vec3 viewDirection) {
    vec3 lightDirection = normalize(-light.direction);
    float diffuseStrength = max(dot(unitNormal, lightDirection), 0.0);
    vec3 reflectionDirection = reflect(-lightDirection, unitNormal);
    float specularStrength = pow(max(dot(viewDirection, reflectionDirection), 0.0), material.shininess);

    vec3 diffuseMap = vec3(texture(material.diffuse, texCoord));
    vec3 specularMap = vec3(texture(material.specular, texCoord));
    return light.ambient * diffuseMap
        + light.diffuse * diffuseStrength * diffuseMap
        + light.specular * specularStrength * specularMap;
}

vec3 calculatePointLight(PointLight light, vec3 unitNormal, vec3 viewDirection) {
    vec3 lightDirection = normalize(light.position - fragPosition);
    float diffuseStrength = max(dot(unitNormal, lightDirection), 0.0);
    vec3 reflectionDirection = reflect(-lightDirection, unitNormal);
    float specularStrength = pow(max(dot(viewDirection, reflectionDirection), 0.0), material.shininess);

    float distanceToLight = length(light.position - fragPosition);
    float attenuation = 1.0 / (
        light.constant
        + light.linear * distanceToLight
        + light.quadratic * distanceToLight * distanceToLight);

    vec3 diffuseMap = vec3(texture(material.diffuse, texCoord));
    vec3 specularMap = vec3(texture(material.specular, texCoord));
    return (
        light.ambient * diffuseMap
        + light.diffuse * diffuseStrength * diffuseMap
        + light.specular * specularStrength * specularMap) * attenuation;
}

vec3 calculateSpotLight(SpotLight light, vec3 unitNormal, vec3 viewDirection) {
    vec3 lightDirection = normalize(light.position - fragPosition);
    float diffuseStrength = max(dot(unitNormal, lightDirection), 0.0);
    vec3 reflectionDirection = reflect(-lightDirection, unitNormal);
    float specularStrength = pow(max(dot(viewDirection, reflectionDirection), 0.0), material.shininess);

    float theta = dot(lightDirection, normalize(-light.direction));
    float edgeWidth = light.innerCutoff - light.outerCutoff;
    float intensity = clamp((theta - light.outerCutoff) / edgeWidth, 0.0, 1.0);
    float distanceToLight = length(light.position - fragPosition);
    float attenuation = 1.0 / (
        light.constant
        + light.linear * distanceToLight
        + light.quadratic * distanceToLight * distanceToLight);

    vec3 diffuseMap = vec3(texture(material.diffuse, texCoord));
    vec3 specularMap = vec3(texture(material.specular, texCoord));
    return (
        light.ambient * diffuseMap
        + light.diffuse * diffuseStrength * diffuseMap
        + light.specular * specularStrength * specularMap) * attenuation * intensity;
}

void main() {
    vec3 unitNormal = normalize(normal);
    vec3 viewDirection = normalize(viewPosition - fragPosition);
    vec3 color = calculateDirectionalLight(directionalLight, unitNormal, viewDirection);
    for (int i = 0; i < 4; ++i) {
        color += calculatePointLight(pointLights[i], unitNormal, viewDirection);
    }
    color += calculateSpotLight(spotLight, unitNormal, viewDirection);
    FragColor = vec4(color, 1.0);
}
