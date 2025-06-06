#version 330 core

#include <material>

// Fragment input
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoords;
in mat3 vTBN;

// Fragment output
out vec4 FragColor;

void main()
{
    FragColor = uMaterial.hasDiffuseMap ?
                    texture(uMaterial.diffuseMap, vTexCoords) :
                    uMaterial.diffuseColor ;
}