#version 330 core
layout(location = 0) in vec3 aPosition;
layout(std140) uniform VertexUniformBuffer
{
    mat4 uProjection;
    mat4 uView;
};
uniform mat4 uModel;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}