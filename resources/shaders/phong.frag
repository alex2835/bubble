#version 330 core

#include <phong>

out vec4 FragColor;

void main()
{
    FragColor = PhongFragment(vec4(1.0));
}
