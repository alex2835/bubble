#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
struct Model;

struct ModelComponent
{
    static int ID() { return static_cast<int>( ComponentID::Model ); }
	static string_view Name() { return "Model"sv; }

	static void OnComponentDraw( const Project& project, const Entity& entity, ModelComponent& component );
	static void ToJson( json& json, const Project& project, const ModelComponent& component );
	static void FromJson( const json& json, Project& project, ModelComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	ModelComponent() = default;
	ModelComponent( const Ref<Model>& model );
    ~ModelComponent();
	Ref<Model> mModel;
};

}
