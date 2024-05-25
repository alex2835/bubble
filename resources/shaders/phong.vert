#version 330 core
layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec2  aTexCoords;
layout(location = 3) in vec3  aTangent;
layout(location = 4) in vec3  aBitangent;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoords;
out mat3 v_TBN;

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

    v_FragPos = vec3(uModel * vec4(aPosition, 1.0));
    v_Normal  = normalize(ITModel * aNormal);
    v_TexCoords = aTexCoords;

    if (uNormalMapping)
    {
        vec3 B = normalize(vec3(uModel * vec4(aBitangent, 0.0)));
        vec3 N = normalize(vec3(uModel * vec4(aNormal,    0.0)));
        vec3 T = normalize(vec3(uModel * vec4(aTangent,   0.0)));
        v_TBN  = mat3(T, B, N);
    }

    gl_Position = uProjection * uView * vec4(v_FragPos, 1.0);
}