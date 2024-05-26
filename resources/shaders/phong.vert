#version 330 core
layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec2  aTexCoords;
layout(location = 3) in vec3  aTangent;
layout(location = 4) in vec3  aBitangent;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoords;
out mat3 vTBN;

layout(std140) uniform VertexUniformBuffer
{
    mat4 uProjection;
    mat4 uView;
};
uniform mat4 uModel;
uniform bool uNormalMapping;

void main()
{
    mat3 ITModel = mat3(transpose(inverse(uModel)));

    vFragPos = vec3(uModel * vec4(aPosition, 1.0));
    vNormal  = normalize(ITModel * aNormal);
    vTexCoords = aTexCoords;

    if (uNormalMapping)
    {
        vec3 B = normalize(vec3(uModel * vec4(aBitangent, 0.0)));
        vec3 N = normalize(vec3(uModel * vec4(aNormal,    0.0)));
        vec3 T = normalize(vec3(uModel * vec4(aTangent,   0.0)));
        vTBN  = mat3(T, B, N);
    }

    gl_Position = uProjection * uView * vec4(vFragPos, 1.0);
}