
#include "engine/scene/components.hpp"

namespace bubble
{
// TagComponent
void TagComponent::OnComponentDraw( void* raw )
{
    auto& tag = *(TagComponent*)raw;
    char buffer[64] = { 0 };
    tag.copy( buffer, sizeof( buffer ) );
    ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "TagComponent" );
    ImGui::InputText( "##Tag", buffer, sizeof( buffer ) );
    tag.assign( buffer );
}

void TagComponent::ToJson( json& j, const void* raw )
{

}

void TagComponent::FromJson( const json& j, void* raw )
{

}

// TransformComponent
void TransformComponent::OnComponentDraw( void* raw )
{
    auto& component = *(TransformComponent*)raw;
    ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "TransformComponent" );
    ImGui::DragFloat3( "Scale", (float*)&component.mScale, 0.01f, 0.01f );
    ImGui::DragFloat3( "Rotation", (float*)&component.mRotation, 0.01f );
    ImGui::DragFloat3( "Position", (float*)&component.mPosition, 0.1f );
}

mat4 TransformComponent::Transform()
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, glm::radians( mRotation.x ), vec3( 1, 0, 0 ) );
    transform = glm::rotate( transform, glm::radians( mRotation.y ), vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, glm::radians( mRotation.z ), vec3( 0, 0, 1 ) );
    transform = glm::scale( transform, mScale );
    return transform;
}

void TransformComponent::ToJson( json& j, const void* raw )
{

}

void TransformComponent::FromJson( const json& j, void* raw )
{

}

// LightComponent
void LightComponent::OnComponentDraw( void* raw )
{
    auto& component = *(LightComponent*)raw;
    ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "LightComponent" );
}

void LightComponent::ToJson( json& j, const void* raw )
{

}

void LightComponent::FromJson( const json& j, void* raw )
{

}

// ModelComponent
void ModelComponent::OnComponentDraw( void* raw )
{
    auto& component = *(ModelComponent*)raw;
    ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "ModelComponent" );
    ImGui::Text( component->mName.c_str() );
}

void ModelComponent::ToJson( json& j, const void* raw )
{

}

void ModelComponent::FromJson( const json& j, void* raw )
{

}

}