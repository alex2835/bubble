#version 330 core
layout(location = 0) out uint objectId;

uniform uint uObjectId;

void main()
{
    objectId = uObjectId;
}
