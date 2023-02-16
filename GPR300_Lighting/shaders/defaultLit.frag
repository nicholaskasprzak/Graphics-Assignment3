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
* Struct to manage light data
*/
struct Light
{
    vec3 position;
    vec3 color;
    float intensity;
};

uniform Light _Light;
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
vec3 calcAmbient(float ambientCoefficient, vec3 light)
{
    vec3 ambientRet;

    ambientRet = ambientCoefficient * light;

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
vec3 calcDiffuse(float diffuseCoefficient, vec3 lightPosition, vec3 vertexPosition, vec3 vertexNormal, vec3 light)
{
    vec3 diffuseRet;

    vec3 dirToVert = lightPosition - vertexPosition;
    float cosAngle = dot(normalize(dirToVert), normalize(vertexNormal));
    clamp(cosAngle, 0, cosAngle);

    diffuseRet = diffuseCoefficient * cosAngle * light;

    return diffuseRet;
};

/*
* Calculates the specular of a light.
*
* 
*/
vec3 calcSpecular(float specularCoefficient, vec3 lightPosition, vec3 vertexPosition, float shininess, vec3 light, vec3 cameraPosition)
{
    vec3 specularRet;

    // light bounces off at the same angle so direction would be same relative to source?
    vec3 reflectDir = reflect(lightPosition, vertexPosition);
    vec3 cameraDir = cameraPosition - vertexPosition;
    float cosAngle = dot(normalize(reflectDir), normalize(cameraDir));
    clamp(cosAngle, 0, cosAngle);

    specularRet = specularCoefficient * pow(cosAngle, shininess) * light;

    return specularRet;
};

vec3 calcPhong(Vertex vertex, Material material, Light light, vec3 cameraPosition)
{
    vec3 phongRet;

    vec3 lightColor = light.intensity * light.color;

    vec3 ambient = calcAmbient(material.ambientK, lightColor);
    vec3 diffuse = calcDiffuse(material.diffuseK, light.position, vertex.worldPosition, vertex.worldNormal, lightColor);
    vec3 specular = calcSpecular(material.specularK, light.position, vertex.worldPosition, material.shininess, lightColor, cameraPosition);

    phongRet = ambient + diffuse + specular;

    return phongRet;
}

void main(){    

    vec3 lightCol = calcPhong(vertexOutput, _Material, _Light, _CameraPosition);

    vec3 normal = normalize(vertexOutput.worldNormal);
    FragColor = vec4(abs(normal),1.0f) * vec4(lightCol, 1.0f);
}