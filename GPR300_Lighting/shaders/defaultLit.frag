#version 450                          
out vec4 FragColor;

/*
* Takes in the vertex struct provided
* by the vertex shader.
*/
in struct Vertex
{
    vec3 worldNormal;
    vec3 worldPosition;
}vertexOutput;

/*
* Struct to manage a model's material
* values, utilized by the different
* components of phong shading
*/
struct Material
{
    vec3 color;
    float ambientK, diffuseK, specularK; // (0-1 range)
    float shininess; // (0-512 range)
};

/*
* Structs to manage light data
*/
struct Light
{
    vec3 color;
    float intensity;
};

struct DirectionalLight
{
    vec3 direction;
    Light light;
};

struct PointLight
{
    vec3 position;
    Light light;

    float constK, linearK, quadraticK;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    Light light;

    float range;
    float innerAngle;
    float outerAngle;
    float angleFalloff;
};

const int MAX_LIGHTS = 8;
uniform int lightCount;

uniform DirectionalLight _DirectionalLight;
uniform PointLight[MAX_LIGHTS] _PointLights;
uniform SpotLight _SpotLight;
uniform Material _Material;
uniform vec3 _CameraPosition;

/*
* Calculates the ambient of a light.
*
* Combines the light's color and intensity
* to find the intensity of the light's color,
* then multiplies that value by the ambient
* coefficient to get the surface's reflection
* of the light.
*/
float calcAmbient(float ambientCoefficient)
{
    float ambientRet;

    ambientRet = ambientCoefficient;

    return ambientRet;
}

/*
* Calculates the diffuse of a light
* using Lambert's cosine law. 
*
* Uses the dot product of the direction
* to the vertex from the light and the
* vertex's normal to calculate the angle
* between the two, fulfilling the requirement
* of needing the cosine angle for the cosine
* law.
*/
float calcDiffuse(float diffuseCoefficient, vec3 lightDirection, vec3 vertexNormal)
{
    float diffuseRet;

    float cosAngle = dot(normalize(lightDirection), normalize(vertexNormal));
    cosAngle = clamp(cosAngle, 0, cosAngle);

    diffuseRet = diffuseCoefficient * cosAngle;

    return diffuseRet;
};

/*
* Calculates the specular of a light.
*
* 
*/
float calcSpecular(float specularCoefficient, vec3 lightDirection, vec3 vertexPosition, vec3 vertexNormal, float shininess, vec3 cameraPosition)
{
    float specularRet;

    vec3 reflectDir = reflect(-lightDirection, vertexNormal);
    vec3 cameraDir = cameraPosition - vertexPosition;
    float cosAngle = dot(normalize(reflectDir), normalize(cameraDir));
    cosAngle = clamp(cosAngle, 0, cosAngle);

    specularRet = specularCoefficient * pow(cosAngle, shininess);

    return specularRet;
};

vec3 calcPhong(Vertex vertex, Material material, Light light, vec3 lightDirection, vec3 cameraPosition)
{
    vec3 phongRet;

    vec3 lightColor = light.intensity * light.color;

    float ambient = calcAmbient(material.ambientK);
    float diffuse = calcDiffuse(material.diffuseK, lightDirection, vertex.worldNormal);
    float specular = calcSpecular(material.specularK, lightDirection, vertex.worldPosition, vertex.worldNormal, material.shininess, cameraPosition);

    phongRet = (ambient + diffuse + specular) * lightColor;

    return phongRet;
}

float calcGLAttenuation(PointLight light, vec3 vertPos)
{
    float attenuation;
    float dist = distance(light.position, vertPos);

    attenuation = 1 / (light.constK + light.linearK + (light.quadraticK * dist));
    //attenuation = clamp(attenuation, 0, attenuation);

    return attenuation;
}

float calcAngularAttenuation(SpotLight light, vec3 vertPos)
{
    float attenuation;

    vec3 dir = (vertPos - light.position) / length(vertPos - light.position);
    float cosAngle = dot(dir, normalize(-light.direction));

    float maxAngle = cos(radians(light.outerAngle));
    float minAngle = cos(radians(light.innerAngle));

    attenuation = (cosAngle - maxAngle) / (minAngle - maxAngle);
    attenuation = pow(attenuation, light.angleFalloff);
    attenuation = clamp(attenuation, 0, 1);

    return attenuation;
}

void main(){    

    vec3 lightCol;

    lightCol += calcPhong(vertexOutput, _Material, _DirectionalLight.light, _DirectionalLight.direction, _CameraPosition);

    // Point Lights
    for (int i = 0; i < lightCount; i++)
    {
        lightCol += calcPhong(vertexOutput, _Material, _PointLights[i].light, _PointLights[i].position - vertexOutput.worldPosition, _CameraPosition) * calcGLAttenuation(_PointLights[i], vertexOutput.worldPosition);
    }

    lightCol += calcPhong(vertexOutput, _Material, _SpotLight.light, _SpotLight.position - vertexOutput.worldPosition, _CameraPosition) * calcAngularAttenuation(_SpotLight, vertexOutput.worldPosition);

    vec3 normal = normalize(vertexOutput.worldNormal);
    FragColor = vec4(lightCol * _Material.color, 1.0f);
}