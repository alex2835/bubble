#version 330 core
layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec2  aTexCoord;

// Uniform buffers
layout(std140) uniform VertexUniformBuffer
{
    mat4 uProjection;
    mat4 uView;
};

uniform mat4 uModel;
uniform vec3 uBillboardPos;
uniform vec2 uBillboardSize;

out vec2 vTexCoord;

void main()
{
    // Billboard always faces camera
    // Extract camera right and up vectors from view matrix
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp = vec3(uView[0][1], uView[1][1], uView[2][1]);

    // Calculate billboard vertex position in world space
    vec3 vertexPosition = uBillboardPos
                        + cameraRight * aPosition.x * uBillboardSize.x
                        + cameraUp * aPosition.y * uBillboardSize.y;

    gl_Position = uProjection * uView * vec4(vertexPosition, 1.0);

    // Flip Y coordinate to match OpenGL texture convention
    vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}
