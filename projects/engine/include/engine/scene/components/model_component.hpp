#pragma once
#include "engine/scene/components/component_base.hpp"
#include <sol/sol.hpp>

namespace bubble
{
struct Model;

struct ModelComponent
{
    static int ID() { return static_cast<int>( ComponentID::Model ); }
	static string_view Name() { return "model"sv; }

    // Fields for engine/reflection: the model, a resource saved by its path.
    static void Reflect();
    // Lua: the fields come from Reflect(); these are what is added to them.
    using LuaConstructors = sol::constructors<ModelComponent(), ModelComponent( const Ref<Model>& )>;
    static void BindLuaMethods( sol::state& lua, sol::usertype<ModelComponent>& type );

public:
	ModelComponent() = default;
	ModelComponent( const Ref<Model>& model );
    ~ModelComponent();
	Ref<Model> mModel;
};

}
