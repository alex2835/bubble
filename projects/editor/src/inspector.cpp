#include "bubble/editor/inspector.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/material_preview.hpp"
#include "bubble/editor/scene_panel.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/types/algorithm.hpp"
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace bubble
{
namespace
{
// What a value holds, for a field that takes any: a script's props before
// its file declares them.
ValueKind KindOf( const Value& value )
{
    if ( value.Is<bool>() )
        return ValueKind::Bool;
    if ( value.Is<f64>() )
        return ValueKind::Number;
    if ( value.Is<Text>() )
        return ValueKind::Text;
    if ( value.Is<vec3>() )
        return ValueKind::Vec3;
    if ( value.Is<Color>() )
        return ValueKind::Color;
    if ( value.Is<Name>() )
        return ValueKind::Name;
    if ( value.Is<AssetId>() )
        return ValueKind::Asset;
    if ( value.Is<EntityPath>() )
        return ValueKind::Entity;
    if ( value.Is<Value::List>() )
        return ValueKind::List;
    return ValueKind::Record;
}

// Case aside, for ASCII: what a filter matches.
bool Contains( string_view text, string_view part )
{
    const auto lower = []( char c ) { return c >= 'A' and c <= 'Z' ? static_cast<char>( c - 'A' + 'a' ) : c; };
    return ranges::search( text, part, {}, lower, lower ).begin() != text.end() or part.empty();
}

Value::Record Args( vector<ValueField> fields )
{
    return Value::Record( std::move( fields ) );
}

Value TextValue( string_view text )
{
    return Value( *Text::TryFrom( text ) );
}

// A set prop stands out from one at its file's default.
constexpr ImVec4 cSet{ 1.0f, 0.85f, 0.45f, 1.0f };
constexpr ImVec4 cError{ 1.0f, 0.45f, 0.4f, 1.0f };

using MaterialParamValue = pair<string, MaterialValue>;

// What a material's bases give a parameter: the nearest that sets it.
opt<MaterialValue> TryInherited( span<const MaterialAsset> bases, string_view name )
{
    for ( const MaterialAsset& base : bases )
        if ( const auto found = ranges::find( base.mParams, name, &MaterialParamValue::first );
             found != base.mParams.end() )
            return found->second;
    return nullopt;
}

bool InheritedFlag( span<const MaterialAsset> bases, opt<bool> MaterialAsset::* flag )
{
    for ( const MaterialAsset& base : bases )
        if ( base.*flag )
            return *( base.*flag );
    return false;
}
}

string FieldLabel( string_view name )
{
    string label( name );
    ranges::replace( label, '_', ' ' );
    if ( not label.empty() and label[0] >= 'a' and label[0] <= 'z' )
        label[0] = static_cast<char>( label[0] - 'a' + 'A' );
    return label;
}

Value DefaultOf( const ValueType& type )
{
    switch ( type.mKind )
    {
        case ValueKind::Bool: return Value( false );
        case ValueKind::Number:
        case ValueKind::Integer:
        case ValueKind::Any: return Value( 0.0 );
        case ValueKind::Text: return Value( Text() );
        case ValueKind::Vec3: return Value( vec3( 0.0f ) );
        case ValueKind::Color: return Value( Color{ .mR = 1, .mG = 1, .mB = 1, .mA = 1 } );
        case ValueKind::Name: return Value( Name() );
        case ValueKind::Asset: return Value( AssetId() );
        case ValueKind::Entity: return Value( EntityPath{} );
        case ValueKind::List: return Value( Value::List{} );
        case ValueKind::Record:
        {
            Value::Record record;
            for ( const ValueTypeField& field : type.mFields )
                record.push_back( ValueField{ .mName = field.mName, .mValue = DefaultOf( field.mType ) } );
            return Value( std::move( record ) );
        }
    }
    return Value();
}

Inspector::Inspector( EditorProject& project ) : mProject( project )
{
}

void Inspector::Run( string_view op, Value::Record args )
{
    mPending.emplace_back( string( op ), std::move( args ) );
}

void Inspector::Set( Name component, const string& path, Value value )
{
    Run( "property.set", Args( { ValueField{ .mName = "component"_n, .mValue = Value( component ) },
                                 ValueField{ .mName = "path"_n, .mValue = TextValue( path ) },
                                 ValueField{ .mName = "value"_n, .mValue = std::move( value ) } } ) );
}

void Inspector::DataSet( const string& path, Value value )
{
    Run( "data.set", Args( { ValueField{ .mName = "path"_n, .mValue = TextValue( path ) },
                             ValueField{ .mName = "value"_n, .mValue = std::move( value ) } } ) );
}

void Inspector::DataRemove( const string& path )
{
    Run( "data.remove", Args( { ValueField{ .mName = "path"_n, .mValue = TextValue( path ) } } ) );
}

void Inspector::RunPending()
{
    const vector<pair<string, Value::Record>> pending = std::move( mPending );
    mPending.clear();
    const string step = std::move( mPendingStep );
    mPendingStep.clear();
    const auto dataMaybe = mProject.GetWorkspace().TryActiveData();
    if ( dataMaybe and not step.empty() )
        dataMaybe->BeginGroup( step );
    for ( const auto& [op, args] : pending )
        if ( not mProject.Run( op, args ) )
            break;
    if ( dataMaybe and not step.empty() )
        dataMaybe->EndGroup();
}

void Inspector::Settle()
{
    if ( mHeld == 0 or ImGui::GetActiveID() == mHeld )
        return;
    mHeld = 0;
    Workspace& workspace = mProject.GetWorkspace();
    if ( const auto dataMaybe = workspace.TryActiveData() )
    {
        if ( dataMaybe->InGroup() )
            dataMaybe->EndGroup();
    }
    else if ( const auto documentMaybe = workspace.TryActive() )
    {
        if ( documentMaybe->GetHistory().InGroup() )
            documentMaybe->GetHistory().EndGroup();
    }
}

void Inspector::Hold( bool changed )
{
    if ( mHeld != 0 or ImGui::GetActiveID() == 0 or not( ImGui::IsItemActivated() or changed ) )
        return;
    // A drag, a text typed, a colour picked: one step however many frames
    // it takes - the history folds a field set again into the first set.
    Workspace& workspace = mProject.GetWorkspace();
    if ( const auto dataMaybe = workspace.TryActiveData() )
    {
        dataMaybe->BeginGroup( "Set Property" );
        mHeld = ImGui::GetActiveID();
    }
    else if ( const auto documentMaybe = workspace.TryActive() )
    {
        documentMaybe->GetHistory().BeginGroup( "Set Property" );
        mHeld = ImGui::GetActiveID();
    }
}

void Inspector::Show()
{
    mWhere.clear();
    if ( const auto dataMaybe = mProject.GetWorkspace().TryActiveData() )
    {
        if ( dataMaybe->GetPath().View().ends_with( ".mat" ) )
            Material( *dataMaybe );
        else
            ImGui::TextDisabled( "An animation graph: its editor comes with step 6c" );
        RunPending();
        return;
    }
    const auto documentMaybe = mProject.GetWorkspace().TryActive();
    if ( not documentMaybe )
    {
        ImGui::TextDisabled( "No document is open" );
        return;
    }
    Document& document = *documentMaybe;
    const Scene& scene = document.GetScene();
    const Selection& selection = document.GetSelection();
    const Entity entity = selection.Active();
    if ( entity == Entity::Null or not scene.Alive( entity ) )
    {
        ImGui::TextDisabled( "Nothing is selected" );
        return;
    }
    NameRow( document, entity );
    if ( const size_t count = selection.All().size(); count > 1 )
        ImGui::TextDisabled( "%zu selected: an edit goes to each that has the field", count );
    for ( const ComponentTypes::Type& type : document.GetComponentTypes().All() )
        if ( type.mGet( scene, entity ) )
            Component( document, entity, type );
    AddComponent( document, entity );
    RunPending();
}

void Inspector::Material( DataDocument& document )
{
    MaterialPreview& preview = PreviewOf( mProject, document );
    preview.Refresh( document );
    ImGui::TextUnformatted( document.GetPath().String().c_str() );
    if ( not preview.Error().empty() )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, cError );
        ImGui::TextWrapped( "%s", preview.Error().c_str() );
        ImGui::PopStyleColor();
    }
    const auto materialMaybe = preview.TryMaterial();
    if ( not materialMaybe )
        return;
    const MaterialAsset& material = *materialMaybe;
    const span<const MaterialAsset> bases = preview.Bases();
    const bool based = static_cast<bool>( material.mBase );
    const auto table = [] {
        if ( not ImGui::BeginTable( "fields", 2, ImGuiTableFlags_Resizable ) )
            return false;
        ImGui::TableSetupColumn( "field", ImGuiTableColumnFlags_WidthStretch, 0.4f );
        ImGui::TableSetupColumn( "value", ImGuiTableColumnFlags_WidthStretch, 0.6f );
        return true;
    };
    if ( table() )
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( "Base" );
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -1.0f );
        AssetId base = material.mBase;
        if ( AssetPicker( "base", base, "material" ) )
        {
            if ( not base )
                DataRemove( "base" );
            else
            {
                // A material with a base takes its base's shader.
                if ( material.mShader )
                {
                    mPendingStep = "Set Base";
                    DataRemove( "shader" );
                }
                DataSet( "base", Value( base ) );
            }
        }
        Shown( "base" );

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( "Shader" );
        ImGui::TableNextColumn();
        if ( based )
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled( "the base's" );
        }
        else
        {
            ImGui::SetNextItemWidth( -1.0f );
            AssetId shader = material.mShader;
            if ( AssetPicker( "shader", shader, "shader" ) )
            {
                if ( shader )
                    DataSet( "shader", Value( shader ) );
                else
                    DataRemove( "shader" );
            }
            if ( not material.mShader and ImGui::IsItemHovered() )
                ImGui::SetTooltip( "None: the engine's standard material" );
        }
        Shown( "shader" );
        MaterialFlag( "Double sided", "double_sided", material.mDoubleSided,
                      InheritedFlag( bases, &MaterialAsset::mDoubleSided ), based );
        MaterialFlag( "Transparent", "transparent", material.mTransparent,
                      InheritedFlag( bases, &MaterialAsset::mTransparent ), based );
        ImGui::EndTable();
    }

    ImGui::SeparatorText( "Parameters" );
    const auto layoutMaybe = preview.TryLayout();
    if ( not layoutMaybe )
    {
        if ( preview.Error().empty() )
            ImGui::TextDisabled( "Loading the shader..." );
        return;
    }
    if ( not table() )
        return;
    for ( const MaterialParam& param : layoutMaybe->mParams )
        MaterialParamRow( material, bases, param );
    // What the document sets that its shader does not declare: there to be
    // taken out.
    for ( const auto& [name, value] : material.mParams )
    {
        if ( layoutMaybe->TryFind( name ) )
            continue;
        const string path = "params." + name;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled( "%s", FieldLabel( name ).c_str() );
        ImGui::TableNextColumn();
        if ( ImGui::SmallButton( ( "Remove##" + path ).c_str() ) )
            DataRemove( path );
        ImGui::SameLine();
        ImGui::TextDisabled( "not in the shader" );
        Shown( path );
    }
    ImGui::EndTable();
}

void Inspector::DataLabel( const string& label, const string& path, bool set, bool based )
{
    if ( set )
        ImGui::PushStyleColor( ImGuiCol_Text, cSet );
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( label.c_str() );
    if ( set )
        ImGui::PopStyleColor();
    if ( set and ImGui::BeginPopupContextItem( path.c_str() ) )
    {
        if ( ImGui::MenuItem( based ? "Back to the Base's" : "Back to Default" ) )
            DataRemove( path );
        ImGui::EndPopup();
    }
}

void Inspector::MaterialFlag( const char* label, const string& path, opt<bool> own, bool inherited, bool based )
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DataLabel( label, path, own.has_value(), based );
    ImGui::TableNextColumn();
    bool on = own.value_or( inherited );
    const bool changed = ImGui::Checkbox( ( "##" + path ).c_str(), &on );
    if ( changed )
        DataSet( path, Value( on ) );
    Shown( path );
    Hold( changed );
}

void Inspector::MaterialParamRow( const MaterialAsset& material,
                                  span<const MaterialAsset> bases,
                                  const MaterialParam& param )
{
    const string path = "params." + param.mName;
    const auto own = ranges::find( material.mParams, param.mName, &MaterialParamValue::first );
    const bool set = own != material.mParams.end();
    const auto inheritedMaybe = TryInherited( bases, param.mName );
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DataLabel( FieldLabel( param.mName ), path, set, inheritedMaybe.has_value() );
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
    const string id = "##" + path;
    bool changed = false;
    if ( param.mKind == MaterialParamKind::Texture )
    {
        AssetId texture = set ? own->second.mTexture : inheritedMaybe ? inheritedMaybe->mTexture : AssetId();
        // None takes the document's own back: the base's, or the shader's
        // default.
        if ( ( changed = AssetPicker( path, texture, "texture" ) ) )
        {
            if ( texture )
                DataSet( path, Value( texture ) );
            else if ( set )
                DataRemove( path );
        }
    }
    else
    {
        vector<f32> numbers = set ? own->second.mNumbers : inheritedMaybe ? inheritedMaybe->mNumbers : param.mDefault;
        numbers.resize( param.mComponents, 0.0f );
        switch ( param.mComponents )
        {
            case 1: changed = ImGui::DragFloat( id.c_str(), numbers.data(), 0.01f, 0, 0, "%.3f" ); break;
            case 2: changed = ImGui::DragFloat2( id.c_str(), numbers.data(), 0.01f, 0, 0, "%.3f" ); break;
            case 3:
                changed = param.mColor ? ImGui::ColorEdit3( id.c_str(), numbers.data() )
                                       : ImGui::DragFloat3( id.c_str(), numbers.data(), 0.01f, 0, 0, "%.3f" );
                break;
            default:
                changed = param.mColor ? ImGui::ColorEdit4( id.c_str(), numbers.data(), ImGuiColorEditFlags_AlphaBar )
                                       : ImGui::DragFloat4( id.c_str(), numbers.data(), 0.01f, 0, 0, "%.3f" );
                break;
        }
        if ( changed )
        {
            if ( numbers.size() == 1 )
                DataSet( path, Value( static_cast<f64>( numbers[0] ) ) );
            else
            {
                Value::List list;
                for ( const f32 number : numbers )
                    list.push_back( Value( static_cast<f64>( number ) ) );
                DataSet( path, Value( std::move( list ) ) );
            }
        }
    }
    Shown( path );
    Hold( changed );
}

void Inspector::NameRow( Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    // Taken from the entity unless it is being typed: a rename is one step,
    // when the field is left.
    const ImGuiID field = ImGui::GetID( "##name" );
    if ( ImGui::GetActiveID() != field or mNamed != entity )
    {
        mName = NameOf( scene, entity ).String();
        mNamed = entity;
    }
    ImGui::SetNextItemWidth( -1.0f );
    ImGui::InputText( "##name", &mName );
    Shown( "name" );
    if ( ImGui::IsItemDeactivatedAfterEdit() and mName != NameOf( scene, entity ).View() )
        if ( const auto textMaybe = Text::TryFrom( mName ) )
            Run( "entity.rename", Args( { ValueField{ .mName = "entity"_n, .mValue = Value( PathOf( scene, entity ) ) },
                                          ValueField{ .mName = "name"_n, .mValue = Value( *textMaybe ) } } ) );
    ImGui::TextDisabled( "%s", PathOf( scene, entity ).mPath.String().c_str() );
}

void Inspector::Component( Document& document, Entity entity, const ComponentTypes::Type& type )
{
    const Scene& scene = document.GetScene();
    const string name = type.mName.ToString();
    ImGui::PushID( name.c_str() );
    const bool open = ImGui::CollapsingHeader( FieldLabel( name ).c_str(), ImGuiTreeNodeFlags_DefaultOpen );
    if ( ImGui::BeginPopupContextItem( "component" ) )
    {
        const Value::Record component{ ValueField{ .mName = "component"_n, .mValue = Value( type.mName ) } };
        const Value::Record prefab{ ValueField{ .mName = "entity"_n, .mValue = Value( PathOf( scene, entity ) ) },
                                    ValueField{ .mName = "path"_n, .mValue = TextValue( name ) } };
        const auto item = [&]( const char* label, string_view op, const Value::Record& args ) {
            const auto pollMaybe = mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op, args );
            if ( ImGui::MenuItem( label, nullptr, false, pollMaybe.has_value() ) )
                Run( op, args );
        };
        item( "Remove Component", "component.remove", component );
        item( "Revert to Prefab", "prefab.revert", prefab );
        item( "Apply to Prefab", "prefab.apply", prefab );
        ImGui::EndPopup();
    }
    if ( open and ImGui::BeginTable( "fields", 2, ImGuiTableFlags_Resizable ) )
    {
        ImGui::TableSetupColumn( "field", ImGuiTableColumnFlags_WidthStretch, 0.4f );
        ImGui::TableSetupColumn( "value", ImGuiTableColumnFlags_WidthStretch, 0.6f );
        ReflectedFields( scene, type.mName, type.mGet( scene, entity ), "" );
        if ( const auto scriptMaybe = scene.TryGet<Script>( entity ); scriptMaybe and type.mName == "script"_n )
            ScriptProps( scene, scriptMaybe->mProps, scriptMaybe->mAsset );
        ImGui::EndTable();
    }
    ImGui::PopID();
}

void Inspector::ReflectedFields( const Scene& scene, Name component, const MetaAny& owner, const string& prefix )
{
    const Reflection& reflection = mProject.GetApp().GetReflection();
    reflection.ForEachField( owner.type(), [&]( Name name, const MetaField& field ) {
        const FieldInfo& info = Reflection::InfoOf( field );
        if ( info.Has( FieldInfo::Hidden ) or ( info.mVisible != nullptr and not info.mVisible( owner ) ) )
            return;
        // A script's props are shown as its file declares them.
        if ( component == "script"_n and prefix.empty() and name == "props"_n )
            return;
        const string path = prefix.empty() ? name.ToString() : format( "{}.{}", prefix, name.ToString() );
        const auto valueMaybe = reflection.TryGet( owner, name );
        const auto typeMaybe = reflection.TryValueTypeOf( field.type() );
        if ( not valueMaybe or not typeMaybe )
            return;
        // A struct of its own: its fields under it, each with its own range
        // and flags.
        if ( typeMaybe->mKind == ValueKind::Record )
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool open = ImGui::TreeNodeEx( ( FieldLabel( name.ToString() ) + "##" + path ).c_str(),
                                                 ImGuiTreeNodeFlags_SpanFullWidth );
            ImGui::TableNextColumn();
            if ( open )
            {
                ReflectedFields( scene, component, *valueMaybe, path );
                ImGui::TreePop();
            }
            return;
        }
        auto valueOfMaybe = reflection.TryToValue( *valueMaybe );
        if ( not valueOfMaybe )
            return;
        ImGui::BeginDisabled( info.Has( FieldInfo::ReadOnly ) );
        if ( ValueRows( scene,
                        Row{ .mLabel = FieldLabel( name.ToString() ),
                             .mPath = path,
                             .mComponent = component,
                             .mSet = false,
                             .mProp = false },
                        *valueOfMaybe, *typeMaybe, info ) )
            Set( component, path, std::move( *valueOfMaybe ) );
        ImGui::EndDisabled();
    } );
}

void Inspector::ScriptProps( const Scene& scene, const Value& overrides, const AssetId& scriptId )
{
    if ( not scriptId )
        return;
    AssetRef<ScriptAsset>& scriptRef = mScripts[scriptId];
    if ( not scriptRef )
        scriptRef = mProject.GetAssets().Load<ScriptAsset>( scriptId );
    const auto scriptMaybe = scriptRef.TryGet();
    if ( not scriptMaybe )
        return;
    const Value::Record set = overrides.TryAs<Value::Record>() ? *overrides.TryAs<Value::Record>() : Value::Record();
    for ( const ScriptProp& prop : scriptMaybe->mProps )
    {
        const auto found = ranges::find( set, prop.mName, &ValueField::mName );
        Value value = found != set.end() ? found->mValue : prop.mDefault;
        FieldInfo info{ .mMin = prop.mMin, .mMax = prop.mMax };
        info.mTooltip = prop.mTooltip.empty() ? nullptr : prop.mTooltip.c_str();
        const Row row{ .mLabel = FieldLabel( prop.mName.ToString() ),
                       .mPath = format( "props.{}", prop.mName.ToString() ),
                       .mComponent = "script"_n,
                       .mSet = found != set.end(),
                       .mProp = true };
        if ( ValueRows( scene, row, value, prop.mType, info ) )
        {
            // The props the scene sets, this one among them now.
            Value::Record next = set;
            const auto at = ranges::find( next, prop.mName, &ValueField::mName );
            if ( at != next.end() )
                at->mValue = std::move( value );
            else
                next.push_back( ValueField{ .mName = prop.mName, .mValue = std::move( value ) } );
            Set( "script"_n, "props", Value( std::move( next ) ) );
        }
        if ( mReset == prop.mName )
        {
            mReset = Name();
            Value::Record next = set;
            erase_if( next, [&]( const ValueField& field ) { return field.mName == prop.mName; } );
            Set( "script"_n, "props", Value( std::move( next ) ) );
        }
    }
}

void Inspector::Label( const Scene& scene, const Row& row, const FieldInfo& info )
{
    if ( row.mSet )
        ImGui::PushStyleColor( ImGuiCol_Text, cSet );
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( row.mLabel.c_str() );
    if ( row.mSet )
        ImGui::PopStyleColor();
    if ( info.mTooltip != nullptr and ImGui::IsItemHovered() )
        ImGui::SetTooltip( "%s", info.mTooltip );
    if ( ImGui::BeginPopupContextItem( row.mPath.c_str() ) )
    {
        if ( row.mProp and ImGui::MenuItem( "Reset to Default", nullptr, false, row.mSet ) )
            mReset = *Name::TryFrom( row.mPath.substr( row.mPath.find( '.' ) + 1 ) );
        const auto activeMaybe = mProject.GetWorkspace().TryActive();
        const Entity entity = activeMaybe ? activeMaybe->GetSelection().Active() : Entity::Null;
        if ( entity != Entity::Null )
        {
            const Value::Record prefab{ ValueField{ .mName = "entity"_n, .mValue = Value( PathOf( scene, entity ) ) },
                                        ValueField{ .mName = "path"_n,
                                                    .mValue = TextValue(
                                                        format( "{}.{}", row.mComponent.ToString(), row.mPath ) ) } };
            for ( const auto& [label, op] : { pair<const char*, string_view>{ "Revert to Prefab", "prefab.revert" },
                                              pair<const char*, string_view>{ "Apply to Prefab", "prefab.apply" } } )
            {
                const auto pollMaybe = mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op, prefab );
                if ( ImGui::MenuItem( label, nullptr, false, pollMaybe.has_value() ) )
                    Run( op, prefab );
            }
        }
        ImGui::EndPopup();
    }
}

bool Inspector::ValueRows( const Scene& scene,
                           const Row& row,
                           Value& value,
                           const ValueType& type,
                           const FieldInfo& info )
{
    const ValueKind kind = type.mKind == ValueKind::Any ? KindOf( value ) : type.mKind;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if ( kind != ValueKind::List and kind != ValueKind::Record )
    {
        Label( scene, row, info );
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -1.0f );
        return Leaf( scene, row, value, kind, info );
    }

    // A list or a record: its rows under it.
    if ( row.mSet )
        ImGui::PushStyleColor( ImGuiCol_Text, cSet );
    const bool open = ImGui::TreeNodeEx( ( row.mLabel + "##" + row.mPath ).c_str(), ImGuiTreeNodeFlags_SpanFullWidth );
    if ( row.mSet )
        ImGui::PopStyleColor();
    ImGui::TableNextColumn();
    bool changed = false;
    if ( kind == ValueKind::List )
    {
        Value::List& list = *value.TryAs<Value::List>();
        const ValueType element = type.mElement ? *type.mElement : ValueType::Of( ValueKind::Any );
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled( "%zu", list.size() );
        ImGui::SameLine();
        if ( ImGui::SmallButton( ( "+##" + row.mPath ).c_str() ) )
        {
            list.push_back( DefaultOf( element ) );
            changed = true;
        }
        ImGui::SameLine();
        if ( ImGui::SmallButton( ( "-##" + row.mPath ).c_str() ) and not list.empty() )
        {
            list.pop_back();
            changed = true;
        }
        if ( open )
            for ( size_t at = 0; at < list.size(); ++at )
                changed |= ValueRows( scene,
                                      Row{ .mLabel = format( "{}", at + 1 ),
                                           .mPath = format( "{}[{}]", row.mPath, at + 1 ),
                                           .mComponent = row.mComponent,
                                           .mSet = false,
                                           .mProp = false },
                                      list[at], element, info );
    }
    else if ( open )
    {
        Value::Record& record = *value.TryAs<Value::Record>();
        for ( ValueField& field : record )
        {
            const auto declared = ranges::find( type.mFields, field.mName, &ValueTypeField::mName );
            const ValueType fieldType =
                declared != type.mFields.end() ? declared->mType : ValueType::Of( ValueKind::Any );
            changed |= ValueRows( scene,
                                  Row{ .mLabel = FieldLabel( field.mName.ToString() ),
                                       .mPath = format( "{}.{}", row.mPath, field.mName.ToString() ),
                                       .mComponent = row.mComponent,
                                       .mSet = false,
                                       .mProp = false },
                                  field.mValue, fieldType, FieldInfo{} );
        }
    }
    if ( open )
        ImGui::TreePop();
    return changed;
}

void Inspector::Shown( const string& field )
{
    const ImVec2 low = ImGui::GetItemRectMin();
    const ImVec2 high = ImGui::GetItemRectMax();
    mWhere[field] = vec2( ( low.x + high.x ) * 0.5f, ( low.y + high.y ) * 0.5f );
}

opt<vec2> Inspector::TryWhere( string_view field ) const
{
    const auto found = mWhere.find( string( field ) );
    return found != mWhere.end() ? opt<vec2>( found->second ) : nullopt;
}

bool Inspector::Leaf( const Scene& scene, const Row& row, Value& value, ValueKind kind, const FieldInfo& info )
{
    const string& path = row.mPath;
    const string id = "##" + path;
    const bool ranged = info.mMax > info.mMin;
    bool changed = false;
    switch ( kind )
    {
        case ValueKind::Bool:
        {
            bool on = *value.TryAs<bool>();
            if ( ( changed = ImGui::Checkbox( id.c_str(), &on ) ) )
                value = Value( on );
            break;
        }
        case ValueKind::Number:
        {
            const bool angle = info.Has( FieldInfo::Angle );
            f32 number = static_cast<f32>( *value.TryAs<f64>() );
            if ( angle )
                number = glm::degrees( number );
            changed = info.Has( FieldInfo::Slider ) and ranged
                          ? ImGui::SliderFloat( id.c_str(), &number, info.mMin, info.mMax )
                          : ImGui::DragFloat( id.c_str(), &number, angle ? 0.5f : 0.01f, info.mMin, info.mMax,
                                              angle ? "%.1f" : "%.3f" );
            if ( changed )
                value = Value( static_cast<f64>( angle ? glm::radians( number ) : number ) );
            break;
        }
        case ValueKind::Integer:
        {
            int number = static_cast<int>( *value.TryAs<f64>() );
            changed = info.Has( FieldInfo::Slider ) and ranged
                          ? ImGui::SliderInt( id.c_str(), &number, static_cast<int>( info.mMin ),
                                              static_cast<int>( info.mMax ) )
                          : ImGui::DragInt( id.c_str(), &number, 0.2f, static_cast<int>( info.mMin ),
                                            static_cast<int>( info.mMax ) );
            if ( changed )
                value = Value( number );
            break;
        }
        case ValueKind::Text:
        {
            string text = value.TryAs<Text>()->String();
            if ( ImGui::InputText( id.c_str(), &text ) )
                if ( auto textMaybe = Text::TryFrom( text ) )
                {
                    value = Value( std::move( *textMaybe ) );
                    changed = true;
                }
            break;
        }
        case ValueKind::Vec3:
        {
            vec3 vector = *value.TryAs<vec3>();
            if ( info.Has( FieldInfo::Color ) )
                changed = ImGui::ColorEdit3( id.c_str(), &vector.x );
            else if ( info.Has( FieldInfo::Angle ) )
            {
                vec3 degrees = glm::degrees( vector );
                if ( ( changed = ImGui::DragFloat3( id.c_str(), &degrees.x, 0.5f, 0, 0, "%.1f" ) ) )
                    vector = glm::radians( degrees );
            }
            else
                changed = ImGui::DragFloat3( id.c_str(), &vector.x, 0.01f, info.mMin, info.mMax, "%.3f" );
            if ( changed )
                value = Value( vector );
            break;
        }
        case ValueKind::Color:
        {
            const Color color = *value.TryAs<Color>();
            array<f32, 4> channels{ color.mR, color.mG, color.mB, color.mA };
            if ( ( changed = ImGui::ColorEdit4( id.c_str(), channels.data(), ImGuiColorEditFlags_AlphaBar ) ) )
                value = Value( Color{ .mR = channels[0], .mG = channels[1], .mB = channels[2], .mA = channels[3] } );
            break;
        }
        case ValueKind::Name:
        {
            string text = value.TryAs<Name>()->ToString();
            if ( ImGui::InputText( id.c_str(), &text ) )
                if ( const auto nameMaybe = Name::TryFrom( text ) )
                {
                    value = Value( *nameMaybe );
                    changed = true;
                }
            break;
        }
        case ValueKind::Asset:
        {
            AssetId asset = *value.TryAs<AssetId>();
            if ( ( changed = AssetPicker( path, asset, info.mAssetKind ) ) )
                value = Value( asset );
            break;
        }
        case ValueKind::Entity:
        {
            EntityPath entity = *value.TryAs<EntityPath>();
            if ( ( changed = EntityField( scene, path, entity ) ) )
                value = Value( std::move( entity ) );
            break;
        }
        default: ImGui::TextDisabled( "%s", value.Describe().c_str() ); break;
    }
    Shown( format( "{}.{}", row.mComponent.ToString(), path ) );
    Hold( changed );
    return changed;
}

bool Inspector::AssetPicker( const string& path, AssetId& asset, const char* kind )
{
    const AssetCatalog& catalog = mProject.GetCatalog();
    const auto pathMaybe = catalog.TryPathOf( asset );
    const string shown = not asset ? "None" : pathMaybe ? pathMaybe->String() : "missing " + asset.ToString();
    bool changed = false;
    const bool open = ImGui::BeginCombo( ( "##" + path ).c_str(), shown.c_str(), ImGuiComboFlags_HeightLarge );
    // A file dragged from the assets panel, if it is of the field's kind.
    if ( not open and ImGui::BeginDragDropTarget() )
    {
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( cAssetPayload ) )
        {
            const auto droppedMaybe = AssetPath::TryFrom(
                string_view( static_cast<const char*>( payload->Data ), static_cast<size_t>( payload->DataSize ) ) );
            if ( droppedMaybe and ( kind == nullptr or mProject.Makes( *droppedMaybe, kind ) ) )
                if ( const auto droppedIdMaybe = catalog.TryIdOf( *droppedMaybe );
                     droppedIdMaybe and *droppedIdMaybe != asset )
                {
                    asset = *droppedIdMaybe;
                    changed = true;
                }
        }
        ImGui::EndDragDropTarget();
    }
    if ( open )
    {
        if ( ImGui::IsWindowAppearing() )
        {
            mFilter.clear();
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth( -1.0f );
        ImGui::InputTextWithHint( "##filter", "filter", &mFilter );
        if ( ImGui::Selectable( "None", not asset ) and asset )
        {
            asset = AssetId();
            changed = true;
        }
        vector<AssetPath> paths = catalog.Paths();
        ranges::sort( paths, {}, &AssetPath::View );
        for ( const AssetPath& option : paths )
        {
            if ( ( kind != nullptr and not mProject.Makes( option, kind ) ) or not Contains( option.View(), mFilter ) )
                continue;
            const AssetId optionId = *catalog.TryIdOf( option );
            if ( ImGui::Selectable( option.String().c_str(), optionId == asset ) and optionId != asset )
            {
                asset = optionId;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool Inspector::EntityField( const Scene& scene, const string& path, EntityPath& entity )
{
    string text = entity.mPath.String();
    bool changed = false;
    if ( ImGui::InputTextWithHint( ( "##" + path ).c_str(), "drop a row of the tree here", &text ) )
        if ( auto textMaybe = Text::TryFrom( text ) )
        {
            entity = EntityPath{ std::move( *textMaybe ) };
            changed = true;
        }
    if ( ImGui::BeginDragDropTarget() )
    {
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( cEntitiesPayload );
             payload != nullptr and payload->DataSize >= static_cast<int>( sizeof( Entity ) ) )
        {
            Entity dropped = Entity::Null;
            memcpy( &dropped, payload->Data, sizeof( Entity ) );
            if ( scene.Alive( dropped ) )
            {
                entity = PathOf( scene, dropped );
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}

void Inspector::AddComponent( Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    ImGui::Spacing();
    if ( ImGui::Button( "Add Component", ImVec2( -1.0f, 0.0f ) ) )
    {
        mFilter.clear();
        ImGui::OpenPopup( "add component" );
    }
    if ( not ImGui::BeginPopup( "add component" ) )
        return;
    if ( ImGui::IsWindowAppearing() )
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint( "##filter", "filter", &mFilter );
    for ( const ComponentTypes::Type& type : document.GetComponentTypes().All() )
    {
        const string name = type.mName.ToString();
        if ( type.mGet( scene, entity ) or not Contains( name, mFilter ) )
            continue;
        if ( ImGui::Selectable( FieldLabel( name ).c_str() ) )
            Run( "component.add", Args( { ValueField{ .mName = "component"_n, .mValue = Value( type.mName ) } } ) );
    }
    ImGui::EndPopup();
}
}
