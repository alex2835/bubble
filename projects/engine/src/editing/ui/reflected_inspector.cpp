#include "engine/pch/pch.hpp"
#include "engine/editing/ui/reflected_inspector.hpp"
#include "engine/editing/ui/inspector_context.hpp"
#include "engine/editing/ui/interaction.hpp"
#include "engine/editing/commands/field_command.hpp"
#include "engine/editing/history.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <imgui.h>

namespace bubble
{
namespace
{
constexpr ImVec4 cTitleColor = ImVec4( 1, 1, 0, 1 );

// Where the edits of one component go.
struct Target
{
    InspectorContext& mCtx;
    Entity mEntity;
    ComponentTypeId mComponentId;
};

bool IsDescribed( const entt::meta_type& type )
{
    return type.data().begin() != type.data().end();
}

// Drawn by one widget, as opposed to opened up as a tree.
bool IsLeaf( const entt::meta_type& type )
{
    return type.is_enum() or not( type.is_sequence_container() or IsDescribed( type ) );
}

string EnumValueName( const entt::meta_any& value )
{
    for ( const auto [id, constant] : value.type().data() )
        if ( constant.get( {} ) == value )
            return constant.name();
    return "?";
}

/// Widgets

// A rotation edited as Euler degrees. Angles read back from a quaternion may
// be another spelling of the same rotation - past 90 degrees of Y, (0, 100,
// 0) reads (180, 80, 180) - which would make the field jump under the mouse,
// so the angles last shown by a widget are shown again for as long as the
// rotation is still the one they made.
bool RotationWidget( const char* label, quat& rotation )
{
    struct Shown
    {
        quat mRotation;
        vec3 mDegrees;
    };
    static hash_map<ImGuiID, Shown> shown;

    const ImGuiID id = ImGui::GetID( label );
    const auto it = shown.find( id );
    vec3 degrees = it != shown.end() and it->second.mRotation == rotation ? it->second.mDegrees
                                                                            : glm::degrees( Transform::ToEuler( rotation ) );
    if ( not ImGui::DragFloat3( label, glm::value_ptr( degrees ), 0.5f, 0.0f, 0.0f, "%.1f" ) )
        return false;
    rotation = Transform::FromEuler( glm::radians( degrees ) );
    shown[id] = { rotation, degrees };
    return true;
}

bool FloatWidget( const char* label, f32& value, const FieldInfo& info )
{
    if ( info.Has( FieldInfo::Slider ) and info.mMax > info.mMin )
        return ImGui::SliderFloat( label, &value, info.mMin, info.mMax, "%.3f",
                                   info.Has( FieldInfo::Logarithmic ) ? ImGuiSliderFlags_Logarithmic : 0 );
    return ImGui::DragFloat( label, &value, info.mSpeed > 0.0f ? info.mSpeed : 0.01f, info.mMin, info.mMax );
}

template <typename Vec>
bool VectorWidget( const char* label, Vec& value, const FieldInfo& info )
{
    const f32 speed = info.mSpeed > 0.0f ? info.mSpeed : 0.01f;
    if constexpr ( Vec::length() == 2 )
        return ImGui::DragFloat2( label, &value.x, speed, info.mMin, info.mMax );
    else if constexpr ( Vec::length() == 3 )
        return info.Has( FieldInfo::Color ) ? ImGui::ColorEdit3( label, &value.x )
                                             : ImGui::DragFloat3( label, &value.x, speed, info.mMin, info.mMax );
    else
        return info.Has( FieldInfo::Color ) ? ImGui::ColorEdit4( label, &value.x )
                                             : ImGui::DragFloat4( label, &value.x, speed, info.mMin, info.mMax );
}

bool EnumWidget( const char* label, entt::meta_any& value )
{
    bool changed = false;
    if ( ImGui::BeginCombo( label, FieldLabel( EnumValueName( value ) ).c_str() ) )
    {
        for ( const auto [id, constant] : value.type().data() )
        {
            entt::meta_any option = constant.get( {} );
            const bool selected = option == value;
            if ( ImGui::Selectable( FieldLabel( constant.name() ).c_str(), selected ) and not selected )
            {
                value = std::move( option );
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

// The widget for a value of a leaf type, editing `value` in place.
bool LeafWidget( const char* label, entt::meta_any& value, const FieldInfo& info )
{
    if ( auto* v = value.try_cast<f32>() )
        return FloatWidget( label, *v, info );
    if ( auto* v = value.try_cast<bool>() )
        return ImGui::Checkbox( label, v );
    if ( auto* v = value.try_cast<i32>() )
        return info.Has( FieldInfo::Slider ) and info.mMax > info.mMin
                   ? ImGui::SliderInt( label, v, (i32)info.mMin, (i32)info.mMax )
                   : ImGui::DragInt( label, v, 1.0f, (i32)info.mMin, (i32)info.mMax );
    if ( auto* v = value.try_cast<u32>() )
        return ImGui::DragScalar( label, ImGuiDataType_U32, v );
    if ( auto* v = value.try_cast<string>() )
        return ImGui::InputText( label, *v );
    if ( auto* v = value.try_cast<vec2>() )
        return VectorWidget( label, *v, info );
    if ( auto* v = value.try_cast<vec3>() )
        return VectorWidget( label, *v, info );
    if ( auto* v = value.try_cast<vec4>() )
        return VectorWidget( label, *v, info );
    if ( auto* v = value.try_cast<quat>() )
        return RotationWidget( label, *v );
    if ( value.type().is_enum() )
        return EnumWidget( label, value );
    ImGui::TextDisabled( "%s: %s", label, TypeName( value.type() ).c_str() );
    return false;
}

/// The walk

void DrawValue( Target& target, const string& label, entt::meta_any& value, const string& path, const FieldInfo& info );

void DrawFields( Target& target, entt::meta_any& owner, const string& prefix )
{
    for ( const auto [id, field] : owner.type().data() )
    {
        const FieldInfo& info = FieldInfoOf( field );
        if ( info.Has( FieldInfo::Hidden ) or ( info.mVisible and not info.mVisible( owner ) ) )
            continue;
        const string path = prefix.empty() ? string( field.name() ) : std::format( "{}.{}", prefix, field.name() );
        // By reference for a field, a copy for a property: either way only
        // read here, and written through SetField.
        entt::meta_any value = field.get( owner );
        DrawValue( target, FieldLabel( field.name() ), value, path, info );
        if ( info.mTooltip and ImGui::IsItemHovered() )
            ImGui::SetTooltip( "%s", info.mTooltip );
    }
}

void DrawLeaf( Target& target, const string& label, const entt::meta_any& current, const string& path, const FieldInfo& info )
{
    // Copies: the widget edits one, the step is recorded between the two.
    const entt::meta_any before = current;
    entt::meta_any edited = current;

    ImGui::BeginDisabled( info.Has( FieldInfo::ReadOnly ) );
    const bool changed = LeafWidget( std::format( "{}##{}", label, path ).c_str(), edited, info );
    ImGui::EndDisabled();

    if ( changed )
    {
        // Applied at once, so the view follows the mouse and derived state
        // (OnChanged) with it.
        entt::meta_any component = ComponentManager::Reflected( target.mCtx.mScene, target.mEntity, target.mComponentId );
        try
        {
            SetField( component, path, entt::meta_any( edited ) );
        }
        catch ( const std::exception& e )
        {
            LogError( "{}.{}: {}", ComponentManager::GetName( target.mComponentId ), path, e.what() );
        }
    }

    edit_detail::TrackEdit( changed, before, edited, [&]( const entt::meta_any& from, const entt::meta_any& to )
    {
        target.mCtx.mHistory.Record( CreateScope<SetFieldCommand>( target.mCtx.mScene, target.mEntity,
                                                                   target.mComponentId, path, from, to ) );
    } );
}

void DrawValue( Target& target, const string& label, entt::meta_any& value, const string& path, const FieldInfo& info )
{
    const entt::meta_type type = value.type();
    if ( IsLeaf( type ) )
    {
        DrawLeaf( target, label, value, path, info );
        return;
    }

    if ( type.is_sequence_container() )
    {
        auto sequence = value.as_sequence_container();
        if ( ImGui::TreeNode( std::format( "{} ({})##{}", label, sequence.size(), path ).c_str() ) )
        {
            // The field's FieldInfo goes to its elements: the range of a
            // list of numbers is the range of each.
            for ( size_t i = 0; i < sequence.size(); i++ )
            {
                entt::meta_any element = sequence[i];
                DrawValue( target, std::format( "[{}]", i ), element, std::format( "{}[{}]", path, i ), info );
            }
            ImGui::TreePop();
        }
        return;
    }

    if ( ImGui::TreeNode( std::format( "{}##{}", label, path ).c_str() ) )
    {
        DrawFields( target, value, path );
        ImGui::TreePop();
    }
}
}

string FieldLabel( string_view name )
{
    string label( name );
    for ( char& c : label )
        if ( c == '_' )
            c = ' ';
    if ( not label.empty() )
        label[0] = (char)std::toupper( (unsigned char)label[0] );
    return label;
}

void DrawComponentFields( InspectorContext& ctx, Entity entity, ComponentTypeId componentId )
{
    entt::meta_any component = ComponentManager::Reflected( ctx.mScene, entity, componentId );
    if ( not component )
        return;
    ImGui::TextColored( cTitleColor, "%s", FieldLabel( ComponentManager::GetName( componentId ) ).c_str() );
    ImGui::PushID( (int)(u32)entity );
    ImGui::PushID( componentId );
    Target target{ ctx, entity, componentId };
    DrawFields( target, component, "" );
    ImGui::PopID();
    ImGui::PopID();
}

}
