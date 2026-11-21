#include "bubble/editor/inspector.hpp"
#include "bubble/animation/animation_graph.hpp"
#include "bubble/core/log.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/graph_view.hpp"
#include "bubble/editor/material_preview.hpp"
#include "bubble/editor/play_session.hpp"
#include "bubble/editor/scene_panel.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab.hpp"
#include "bubble/scene/prefab_link.hpp"
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

void Inspector::PrefabOverrides( Document& document, Entity entity )
{
    mOverridden.clear();
    mAddedComponents.clear();
    const Scene& scene = document.GetScene();
    const auto linkMaybe = scene.TryGet<PrefabLink>( entity );
    if ( not linkMaybe or not scene.Alive( linkMaybe->mInstance ) or
         not scene.Has<PrefabInstance>( linkMaybe->mInstance ) )
        return;
    Workspace& workspace = mProject.GetWorkspace();
    const auto prefabMaybe = workspace.TryPrefab( scene.Get<PrefabInstance>( linkMaybe->mInstance ).mPrefab );
    if ( not prefabMaybe )
        return;
    // As a level file would keep the instance: by node, what is set and
    // what is added.
    const auto overridesMaybe =
        TryInstanceOverrides( scene, linkMaybe->mInstance, **prefabMaybe, workspace.GetFileContext() );
    if ( not overridesMaybe )
        return;
    const string node = format( "{}", linkMaybe->mNode );
    for ( const Json::Member& member : *overridesMaybe )
    {
        const auto nodesMaybe = member.mValue.TryAs<Json::Object>();
        if ( not nodesMaybe )
            continue;
        const auto found = ranges::find( *nodesMaybe, node, &Json::Member::mKey );
        if ( found == nodesMaybe->end() or not found->mValue.TryAs<Json::Object>() )
            continue;
        hset<string>& into = member.mKey == "set" ? mOverridden : mAddedComponents;
        if ( member.mKey != "set" and member.mKey != "add_components" )
            continue;
        for ( const Json::Member& field : *found->mValue.TryAs<Json::Object>() )
            into.insert( field.mKey );
    }
}

bool Inspector::Overridden( Name component, const string& path ) const
{
    const string name = component.ToString();
    if ( mAddedComponents.contains( name ) )
        return true;
    const string field = name + "." + path;
    // Set itself, or a part of it - a struct's field, a list's element.
    return ranges::any_of( mOverridden, [&]( const string& set ) {
        return set == field or
               ( set.starts_with( field ) and ( set[field.size()] == '.' or set[field.size()] == '[' ) );
    } );
}

tuple<const void*, Entity, size_t> Inspector::DocumentKey()
{
    Workspace& workspace = mProject.GetWorkspace();
    if ( const auto dataMaybe = workspace.TryActiveData() )
        return { &*dataMaybe, Entity::Null, 0 };
    if ( const auto documentMaybe = workspace.TryActive() )
        return { &*documentMaybe, documentMaybe->GetSelection().Active(), documentMaybe->GetSelection().All().size() };
    return { nullptr, Entity::Null, 0 };
}

void Inspector::Inspect( string path )
{
    mInspected = std::move( path );
    mInspectedOver = DocumentKey();
}

void Inspector::Show()
{
    mWhere.clear();
    mMarked.clear();
    if ( not mInspected.empty() and DocumentKey() == mInspectedOver )
    {
        ImportSettings( mInspected );
        return;
    }
    mInspected.clear();
    if ( const auto dataMaybe = mProject.GetWorkspace().TryActiveData() )
    {
        if ( dataMaybe->GetPath().View().ends_with( ".mat" ) )
            Material( *dataMaybe );
        else if ( dataMaybe->GetPath().View().ends_with( ".anim" ) )
            Graph( *dataMaybe );
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
    PrefabOverrides( document, entity );
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

namespace
{
OptRef<const Json> TryMember( const Json& json, string_view key )
{
    return json.TryAs<Json::Object>() ? json.TryFind( key ) : nullopt;
}

opt<string> TryTextAt( const Json& json, string_view key )
{
    const auto memberMaybe = TryMember( json, key );
    return memberMaybe and memberMaybe->TryAs<string>() ? opt<string>( *memberMaybe->TryAs<string>() ) : nullopt;
}

vector<string> MemberNames( const Json& json, string_view key )
{
    vector<string> names;
    if ( const auto memberMaybe = TryMember( json, key ); memberMaybe and memberMaybe->TryAs<Json::Object>() )
        for ( const Json::Member& member : *memberMaybe->TryAs<Json::Object>() )
            names.push_back( member.mKey );
    return names;
}

// The parameters that hold numbers: what a blend or a speed reads.
vector<string> NumberParameters( const Json& data )
{
    vector<string> names;
    if ( const auto parametersMaybe = TryMember( data, "parameters" );
         parametersMaybe and parametersMaybe->TryAs<Json::Object>() )
        for ( const Json::Member& member : *parametersMaybe->TryAs<Json::Object>() )
            if ( member.mValue.TryAs<f64>() )
                names.push_back( member.mKey );
    return names;
}

// A transition's conditions, one or a list in the file.
vector<string> ConditionsOf( const Json& transition )
{
    vector<string> conditions;
    if ( const auto whenMaybe = TryMember( transition, "when" ) )
    {
        if ( const auto oneMaybe = whenMaybe->TryAs<string>() )
            conditions.push_back( *oneMaybe );
        else if ( const auto listMaybe = whenMaybe->TryAs<Json::Array>() )
            for ( const Json& condition : *listMaybe )
                if ( condition.TryAs<string>() )
                    conditions.push_back( *condition.TryAs<string>() );
    }
    return conditions;
}

Value ConditionsValue( const vector<string>& conditions )
{
    if ( conditions.size() == 1 )
        return TextValue( conditions.front() );
    Value::List list;
    for ( const string& condition : conditions )
        list.push_back( TextValue( condition ) );
    return Value( std::move( list ) );
}

bool TableOfFields( const char* id )
{
    if ( not ImGui::BeginTable( id, 2, ImGuiTableFlags_Resizable ) )
        return false;
    ImGui::TableSetupColumn( "field", ImGuiTableColumnFlags_WidthStretch, 0.4f );
    ImGui::TableSetupColumn( "value", ImGuiTableColumnFlags_WidthStretch, 0.6f );
    return true;
}
}

void Inspector::ImportSettings( const string& path )
{
    const auto assetMaybe = AssetPath::TryFrom( path );
    if ( not assetMaybe )
        return;
    ImGui::TextUnformatted( path.c_str() );
    if ( const string_view kind = mProject.KindOf( *assetMaybe ); not kind.empty() )
        ImGui::TextDisabled( "%.*s", static_cast<int>( kind.size() ), kind.data() );
    if ( const auto errorMaybe = mProject.TryErrorOf( *assetMaybe ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.45f, 0.4f, 1.0f ) );
        ImGui::TextWrapped( "%s", errorMaybe->c_str() );
        ImGui::PopStyleColor();
    }
    const auto importerMaybe = mProject.TryImporterOf( *assetMaybe );
    if ( not importerMaybe or importerMaybe->mSettings.empty() )
    {
        ImGui::TextDisabled( importerMaybe ? "Nothing to set on how it imports" : "Not an asset" );
        return;
    }
    ImGui::SeparatorText( "Import" );
    const auto textMaybe = mProject.GetFiles().TryRead( ImportFileOf( *assetMaybe ) );
    const auto fileMaybe = textMaybe ? TryParseImportFile( *textMaybe ) : unexpected( "no .import yet"s );
    if ( not fileMaybe )
    {
        ImGui::TextDisabled( "%s", fileMaybe.error().c_str() );
        return;
    }
    if ( not TableOfFields( "import" ) )
        return;
    for ( const struct ImportSetting& setting : importerMaybe->mSettings )
        ImportSetting( path, setting, fileMaybe->mSettings.TryFind( setting.mName ) );
    ImGui::EndTable();
    RunPending();
}

void Inspector::ImportSetting( const string& path, const struct ImportSetting& setting, OptRef<const Json> ownMaybe )
{
    const string id = "##import." + setting.mName;
    const auto set = [&]( Value value ) {
        Run( "asset.set_import", Args( { ValueField{ .mName = "path"_n, .mValue = TextValue( path ) },
                                         ValueField{ .mName = "setting"_n, .mValue = TextValue( setting.mName ) },
                                         ValueField{ .mName = "value"_n, .mValue = std::move( value ) } } ) );
    };
    const auto reset = [&] {
        Run( "asset.set_import", Args( { ValueField{ .mName = "path"_n, .mValue = TextValue( path ) },
                                         ValueField{ .mName = "setting"_n, .mValue = TextValue( setting.mName ) } } ) );
    };
    // The .import's own value, read as the setting's type; one that does not
    // read shows as the default, the importer saying why it is wrong.
    opt<Value> ownValueMaybe;
    if ( ownMaybe )
        if ( auto readMaybe = TryValueFromJson( *ownMaybe, setting.mType, setting.mName, mProject.GetCatalog() ) )
            ownValueMaybe = std::move( *readMaybe );
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if ( ownMaybe )
        ImGui::PushStyleColor( ImGuiCol_Text, cSet );
    ImGui::TextUnformatted( FieldLabel( setting.mName ).c_str() );
    if ( ownMaybe )
        ImGui::PopStyleColor();
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "%s", setting.mTooltip.c_str() );
    if ( ownMaybe and ImGui::BeginPopupContextItem( ( "menu" + id ).c_str() ) )
    {
        if ( ImGui::MenuItem( "Back to the importer's default" ) )
            reset();
        ImGui::EndPopup();
    }
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
    const opt<Value> shownMaybe = ownValueMaybe ? ownValueMaybe : setting.mDefault;
    if ( not shownMaybe )
    {
        // No default: the source decides; a choice sets it either way.
        int choice = 0;
        if ( ImGui::Combo( id.c_str(), &choice, "decided by the file\0on\0off\0" ) and choice != 0 )
            set( Value( choice == 1 ) );
    }
    else
        switch ( setting.mType.mKind )
        {
            case ValueKind::Bool:
            {
                bool on = *shownMaybe->TryAs<bool>();
                if ( not setting.mDefault )
                {
                    int choice = on ? 1 : 2;
                    if ( ImGui::Combo( id.c_str(), &choice, "decided by the file\0on\0off\0" ) )
                    {
                        if ( choice == 0 )
                            reset();
                        else
                            set( Value( choice == 1 ) );
                    }
                }
                else if ( ImGui::Checkbox( id.c_str(), &on ) )
                    set( Value( on ) );
                break;
            }
            case ValueKind::Number:
            {
                f64 number = *shownMaybe->TryAs<f64>();
                ImGui::InputDouble( id.c_str(), &number, 0.0, 0.0, "%g" );
                if ( ImGui::IsItemDeactivatedAfterEdit() )
                    set( Value( number ) );
                break;
            }
            case ValueKind::Text:
            {
                string text = shownMaybe->TryAs<Text>()->String();
                ImGui::InputText( id.c_str(), &text );
                if ( ImGui::IsItemDeactivatedAfterEdit() )
                    if ( auto textMaybe = Text::TryFrom( text ) )
                        set( Value( std::move( *textMaybe ) ) );
                break;
            }
            case ValueKind::List:
            {
                // Numbers, as a person writes them: "0.5, 0.25".
                string text;
                for ( const Value& element : *shownMaybe->TryAs<Value::List>() )
                    if ( const auto numberMaybe = element.TryAs<f64>() )
                        text += format( "{}{}", text.empty() ? "" : ", ", *numberMaybe );
                ImGui::InputText( id.c_str(), &text );
                if ( ImGui::IsItemDeactivatedAfterEdit() )
                {
                    Value::List numbers;
                    string_view rest = text;
                    while ( not rest.empty() )
                    {
                        const size_t comma = rest.find( ',' );
                        string part( rest.substr( 0, comma ) );
                        rest = comma == string_view::npos ? string_view() : rest.substr( comma + 1 );
                        erase_if( part, []( char c ) { return c == ' '; } );
                        if ( part.empty() )
                            continue;
                        char* end = nullptr;
                        const f64 number = strtod( part.c_str(), &end );
                        if ( end != part.c_str() + part.size() )
                        {
                            LogError( "{}: {} is not a number", setting.mName, part );
                            return;
                        }
                        numbers.push_back( Value( number ) );
                    }
                    set( Value( std::move( numbers ) ) );
                }
                break;
            }
            default: ImGui::TextDisabled( "%s", shownMaybe->Describe().c_str() ); break;
        }
    Shown( "import." + setting.mName );
}

void Inspector::ClipEvents( const Json& data, const string& clip )
{
    // As the graph keeps them: [ [time, name], ... ] under the clip's name.
    vector<pair<f64, string>> events;
    if ( const auto allMaybe = TryMember( data, "events" ) )
        if ( const auto listMaybe = TryMember( *allMaybe, clip ); listMaybe and listMaybe->TryAs<Json::Array>() )
            for ( const Json& event : *listMaybe->TryAs<Json::Array>() )
                if ( const auto pairMaybe = event.TryAs<Json::Array>(); pairMaybe and pairMaybe->size() == 2 )
                    events.emplace_back( ( *pairMaybe )[0].TryAs<f64>() ? *( *pairMaybe )[0].TryAs<f64>() : 0.0,
                                         ( *pairMaybe )[1].TryAs<string>() ? *( *pairMaybe )[1].TryAs<string>()
                                                                           : string() );
    const auto set = [&]( const vector<pair<f64, string>>& to ) {
        Value::List list;
        for ( const auto& [time, name] : to )
            list.push_back( Value( Value::List{ Value( time ), TextValue( name ) } ) );
        Run( "graph.set_events", Args( { ValueField{ .mName = "clip"_n, .mValue = TextValue( clip ) },
                                         ValueField{ .mName = "events"_n, .mValue = Value( std::move( list ) ) } } ) );
    };
    for ( size_t at = 0; at < events.size(); ++at )
    {
        const string where = format( "events.{}[{}]", clip, at + 1 );
        ImGui::PushID( where.c_str() );
        RowLabel( at == 0 ? "Events" : "" );
        f32 time = static_cast<f32>( events[at].first );
        ImGui::SetNextItemWidth( ImGui::GetFontSize() * 4.0f );
        bool changed = ImGui::DragFloat( "##time", &time, 0.005f, 0.0f, 1.0f, "%.3f" );
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Where in the clip, a share of it from 0 to 1" );
        Shown( where + ".time" );
        ImGui::SameLine();
        string name = events[at].second;
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 2.0f );
        ImGui::InputTextWithHint( "##name", "on_animation_event's name", &name );
        const bool named = ImGui::IsItemDeactivatedAfterEdit() and not name.empty();
        ImGui::SameLine();
        const bool removed = ImGui::SmallButton( "x" );
        if ( changed or named )
        {
            vector<pair<f64, string>> edited = events;
            edited[at] = { clamp( static_cast<f64>( time ), 0.0, 1.0 ), name };
            set( edited );
        }
        else if ( removed )
        {
            vector<pair<f64, string>> edited = events;
            edited.erase( edited.begin() + static_cast<ptrdiff_t>( at ) );
            set( edited );
        }
        Hold( changed );
        ImGui::PopID();
    }
    RowLabel( events.empty() ? "Events" : "" );
    if ( ImGui::SmallButton( "Add Event" ) )
    {
        vector<pair<f64, string>> edited = events;
        edited.emplace_back( 0.5, "event" );
        set( edited );
    }
    Shown( "events." + clip + ".add" );
}

bool Inspector::ClipField( const char* id, string& clip )
{
    if ( not ImGui::BeginCombo( id, clip.empty() ? "choose a clip" : clip.c_str() ) )
        return false;
    bool chosen = false;
    // The project's models' clips; one typed, for a model not imported yet.
    const vector<string> names = mProject.ClipNames();
    if ( names.empty() )
        ImGui::TextDisabled( "No model of the project has clips" );
    for ( const string& name : names )
        if ( ImGui::Selectable( name.c_str(), name == clip ) and name != clip )
        {
            clip = name;
            chosen = true;
        }
    ImGui::SetNextItemWidth( ImGui::GetFontSize() * 10.0f );
    if ( ImGui::InputTextWithHint( "##typed", "or type one", &mTypedClip, ImGuiInputTextFlags_EnterReturnsTrue ) and
         not mTypedClip.empty() )
    {
        clip = exchange( mTypedClip, string() );
        chosen = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndCombo();
    return chosen;
}

void Inspector::RowLabel( const char* label )
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( label );
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
}

void Inspector::Graph( DataDocument& document )
{
    GraphView& view = GraphViewOf( mProject, document );
    const Json& data = document.GetData();
    ImGui::TextUnformatted( document.GetPath().String().c_str() );
    if ( const auto graphMaybe = TryParseAnimationGraph( data.Write() ); not graphMaybe )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, cError );
        ImGui::TextWrapped( "%s: %s", document.GetPath().String().c_str(), graphMaybe.error().c_str() );
        ImGui::PopStyleColor();
    }
    const vector<string> states = MemberNames( data, "states" );
    if ( TableOfFields( "graph" ) )
    {
        RowLabel( "Entry" );
        const string entry = TryTextAt( data, "entry" ).value_or( states.empty() ? string() : states.front() );
        if ( ImGui::BeginCombo( "##entry", entry.c_str() ) )
        {
            for ( const string& state : states )
                if ( ImGui::Selectable( state.c_str(), state == entry ) and state != entry )
                    DataSet( "entry", TextValue( state ) );
            ImGui::EndCombo();
        }
        Shown( "entry" );
        ImGui::EndTable();
    }
    ImGui::SeparatorText( "Parameters" );
    GraphParameters( data );
    if ( const string& state = view.SelectedState(); not state.empty() and ranges::contains( states, state ) )
    {
        ImGui::SeparatorText( "State" );
        GraphState( data, state, view );
    }
    else if ( const auto transitionMaybe = view.SelectedTransition() )
    {
        ImGui::SeparatorText( "Transition" );
        GraphTransition( data, *transitionMaybe );
    }
    else
        ImGui::TextDisabled( "Select a state or a transition on the graph" );
}

void Inspector::GraphParameters( const Json& data )
{
    const auto parametersMaybe = TryMember( data, "parameters" );
    const auto objectMaybe = parametersMaybe ? parametersMaybe->TryAs<Json::Object>() : nullopt;
    if ( objectMaybe and not objectMaybe->empty() and TableOfFields( "parameters" ) )
    {
        for ( const Json::Member& member : *objectMaybe )
        {
            const string path = "parameters." + member.mKey;
            ImGui::PushID( path.c_str() );
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( member.mKey.c_str() );
            // Renamed from its menu, wherever the graph reads it.
            if ( ImGui::BeginPopupContextItem( "rename" ) )
            {
                if ( ImGui::IsWindowAppearing() )
                {
                    mParameterName = member.mKey;
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::SetNextItemWidth( ImGui::GetFontSize() * 10.0f );
                if ( ImGui::InputText( "##name", &mParameterName, ImGuiInputTextFlags_EnterReturnsTrue ) )
                {
                    Run( "graph.rename_parameter",
                         Args( { ValueField{ .mName = "parameter"_n, .mValue = TextValue( member.mKey ) },
                                 ValueField{ .mName = "name"_n, .mValue = TextValue( mParameterName ) } } ) );
                    ImGui::CloseCurrentPopup();
                }
                ImGui::TextDisabled( "Enter renames it, in every condition too" );
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth( -1.0f );
            constexpr array<const char*, 3> cKinds{ "number", "bool", "trigger" };
            const int kind = member.mValue.TryAs<f64>() ? 0 : member.mValue.TryAs<bool>() ? 1 : 2;
            ImGui::SetNextItemWidth( ImGui::GetFontSize() * 5.0f );
            if ( ImGui::BeginCombo( "##kind", cKinds[static_cast<size_t>( kind )] ) )
            {
                const array<Value, 3> defaults{ Value( 0.0 ), Value( false ), TextValue( "trigger" ) };
                for ( size_t at = 0; at < cKinds.size(); ++at )
                    if ( ImGui::Selectable( cKinds[at], static_cast<int>( at ) == kind ) and
                         static_cast<int>( at ) != kind )
                        DataSet( path, defaults[at] );
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            bool changed = false;
            if ( const auto numberMaybe = member.mValue.TryAs<f64>() )
            {
                f32 number = static_cast<f32>( *numberMaybe );
                ImGui::SetNextItemWidth( ImGui::GetFontSize() * 5.0f );
                if ( ( changed = ImGui::DragFloat( "##value", &number, 0.01f, 0, 0, "%.3f" ) ) )
                    DataSet( path, Value( static_cast<f64>( number ) ) );
            }
            else if ( const auto onMaybe = member.mValue.TryAs<bool>() )
            {
                bool on = *onMaybe;
                if ( ( changed = ImGui::Checkbox( "##value", &on ) ) )
                    DataSet( path, Value( on ) );
            }
            else
                ImGui::TextDisabled( "set by a script" );
            Shown( path );
            Hold( changed );
            ImGui::SameLine();
            if ( ImGui::SmallButton( "x" ) )
                DataRemove( path );
            if ( ImGui::IsItemHovered() )
                ImGui::SetTooltip( "Remove the parameter" );
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::SetNextItemWidth( ImGui::GetFontSize() * 10.0f );
    ImGui::InputTextWithHint( "##new parameter", "new_parameter", &mNewParameter );
    ImGui::SameLine();
    const bool free =
        detail::NameValid( mNewParameter ) and not ranges::contains( MemberNames( data, "parameters" ), mNewParameter );
    ImGui::BeginDisabled( not free );
    if ( ImGui::Button( "Add Parameter" ) )
    {
        DataSet( "parameters." + mNewParameter, Value( 0.0 ) );
        mNewParameter.clear();
    }
    ImGui::EndDisabled();
}

void Inspector::GraphState( const Json& data, const string& name, GraphView& view )
{
    const Json& state = *TryMember( data, "states" )->TryFind( name );
    const string base = "states." + name;
    if ( not TableOfFields( "state" ) )
        return;
    // Renamed when the field is left: a rename rewrites what names it.
    RowLabel( "Name" );
    const ImGuiID field = ImGui::GetID( "##state name" );
    if ( ImGui::GetActiveID() != field or mStateNamed != name )
    {
        mStateName = name;
        mStateNamed = name;
    }
    ImGui::InputText( "##state name", &mStateName );
    Shown( base );
    if ( ImGui::IsItemDeactivatedAfterEdit() and mStateName != name and detail::NameValid( mStateName ) )
    {
        Run( "graph.rename_state", Args( { ValueField{ .mName = "state"_n, .mValue = TextValue( name ) },
                                           ValueField{ .mName = "name"_n, .mValue = TextValue( mStateName ) } } ) );
        view.Select( mStateName );
    }

    const auto clipMaybe = TryTextAt( state, "clip" );
    const auto blendMaybe = TryMember( state, "blend" );
    const vector<string> numbers = NumberParameters( data );
    RowLabel( "Plays" );
    constexpr array<const char*, 3> cPlays{ "rest pose", "a clip", "a blend" };
    const size_t plays = blendMaybe ? 2 : clipMaybe ? 1 : 0;
    if ( ImGui::BeginCombo( "##plays", cPlays[plays] ) )
    {
        for ( size_t at = 0; at < cPlays.size(); ++at )
        {
            // A blend goes along a number.
            ImGui::BeginDisabled( at == 2 and numbers.empty() );
            if ( ImGui::Selectable( cPlays[at], at == plays ) and at != plays )
            {
                mPendingStep = "Set Plays";
                if ( clipMaybe )
                    DataRemove( base + ".clip" );
                if ( blendMaybe )
                    DataRemove( base + ".blend" );
                if ( at == 1 )
                    DataSet( base + ".clip", TextValue( "" ) );
                if ( at == 2 )
                    DataSet(
                        base + ".blend",
                        Value( Value::Record{ ValueField{ .mName = "param"_n, .mValue = TextValue( numbers.front() ) },
                                              ValueField{ .mName = "points"_n,
                                                          .mValue = Value( Value::List{ Value( Value::List{
                                                              TextValue( "" ), Value( 0.0 ) } ) } ) } } ) );
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    Shown( base + ".plays" );
    if ( clipMaybe )
    {
        RowLabel( "Clip" );
        string clip = *clipMaybe;
        const bool changed = ClipField( "##clip", clip );
        if ( changed )
            DataSet( base + ".clip", TextValue( clip ) );
        Shown( base + ".clip" );
        Hold( changed );
        if ( not clipMaybe->empty() )
            ClipEvents( data, *clipMaybe );
    }
    if ( blendMaybe )
    {
        RowLabel( "Along" );
        const string param = TryTextAt( *blendMaybe, "param" ).value_or( "" );
        if ( ImGui::BeginCombo( "##along", param.c_str() ) )
        {
            for ( const string& number : numbers )
                if ( ImGui::Selectable( number.c_str(), number == param ) and number != param )
                    DataSet( base + ".blend.param", TextValue( number ) );
            ImGui::EndCombo();
        }
        const auto pointsMaybe = TryMember( *blendMaybe, "points" );
        const auto listMaybe = pointsMaybe ? pointsMaybe->TryAs<Json::Array>() : nullopt;
        const size_t count = listMaybe ? listMaybe->size() : 0;
        for ( size_t at = 0; at < count; ++at )
        {
            const auto pointMaybe = ( *listMaybe )[at].TryAs<Json::Array>();
            if ( not pointMaybe or pointMaybe->size() != 2 )
                continue;
            const string point = format( "{}.blend.points[{}]", base, at + 1 );
            ImGui::PushID( point.c_str() );
            RowLabel( format( "Point {}", at + 1 ).c_str() );
            string clip = ( *pointMaybe )[0].TryAs<string>() ? *( *pointMaybe )[0].TryAs<string>() : string();
            f32 value = ( *pointMaybe )[1].TryAs<f64>() ? static_cast<f32>( *( *pointMaybe )[1].TryAs<f64>() ) : 0.0f;
            ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.55f );
            bool changed = ClipField( "##clip", clip );
            if ( changed )
                DataSet( point + "[1]", TextValue( clip ) );
            Shown( point + "[1]" );
            Hold( changed );
            ImGui::SameLine();
            ImGui::SetNextItemWidth( ImGui::GetFontSize() * 4.0f );
            changed = ImGui::DragFloat( "##at", &value, 0.01f, 0, 0, "%.2f" );
            if ( changed )
                DataSet( point + "[2]", Value( static_cast<f64>( value ) ) );
            Shown( point + "[2]" );
            Hold( changed );
            ImGui::SameLine();
            ImGui::BeginDisabled( count == 1 );
            if ( ImGui::SmallButton( "x" ) )
                DataRemove( point );
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        RowLabel( "" );
        if ( ImGui::SmallButton( "Add Point" ) )
        {
            const f64 last = count > 0 and ( *listMaybe )[count - 1].TryAs<Json::Array>() and
                                     ( *listMaybe )[count - 1].TryAs<Json::Array>()->size() == 2 and
                                     ( *( *listMaybe )[count - 1].TryAs<Json::Array>() )[1].TryAs<f64>()
                                 ? *( *( *listMaybe )[count - 1].TryAs<Json::Array>() )[1].TryAs<f64>()
                                 : 0.0;
            DataSet( format( "{}.blend.points[{}]", base, count + 1 ),
                     Value( Value::List{ TextValue( "" ), Value( last + 1.0 ) } ) );
        }
    }

    RowLabel( "Loop" );
    const auto loopMaybe = TryMember( state, "loop" );
    bool loop = loopMaybe and loopMaybe->TryAs<bool>() ? *loopMaybe->TryAs<bool>() : true;
    if ( ImGui::Checkbox( "##loop", &loop ) )
        DataSet( base + ".loop", Value( loop ) );
    Shown( base + ".loop" );

    // A number, or a parameter read instead.
    RowLabel( "Speed" );
    const auto speedMaybe = TryMember( state, "speed" );
    const auto speedParameterMaybe = TryTextAt( state, "speed" );
    if ( not speedParameterMaybe )
    {
        f32 speed = speedMaybe and speedMaybe->TryAs<f64>() ? static_cast<f32>( *speedMaybe->TryAs<f64>() ) : 1.0f;
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.5f );
        const bool changed = ImGui::DragFloat( "##speed", &speed, 0.01f, 0, 0, "%.2f" );
        if ( changed )
            DataSet( base + ".speed", Value( static_cast<f64>( speed ) ) );
        Shown( base + ".speed" );
        Hold( changed );
        ImGui::SameLine();
    }
    ImGui::SetNextItemWidth( -1.0f );
    if ( ImGui::BeginCombo( "##speed by", speedParameterMaybe ? speedParameterMaybe->c_str() : "a number" ) )
    {
        if ( ImGui::Selectable( "a number", not speedParameterMaybe ) and speedParameterMaybe )
            DataSet( base + ".speed", Value( 1.0 ) );
        for ( const string& number : numbers )
            if ( ImGui::Selectable( number.c_str(), speedParameterMaybe == number ) and speedParameterMaybe != number )
                DataSet( base + ".speed", TextValue( number ) );
        ImGui::EndCombo();
    }

    RowLabel( "Root motion" );
    const auto rootMaybe = TryMember( state, "root_motion" );
    bool root = rootMaybe and rootMaybe->TryAs<bool>() and *rootMaybe->TryAs<bool>();
    if ( ImGui::Checkbox( "##root motion", &root ) )
        DataSet( base + ".root_motion", Value( root ) );
    Shown( base + ".root_motion" );
    ImGui::EndTable();
}

void Inspector::GraphTransition( const Json& data, size_t index )
{
    const auto transitionsMaybe = TryMember( data, "transitions" );
    const auto listMaybe = transitionsMaybe ? transitionsMaybe->TryAs<Json::Array>() : nullopt;
    if ( not listMaybe or index < 1 or index > listMaybe->size() )
        return;
    const Json& transition = ( *listMaybe )[index - 1];
    const string base = format( "transitions[{}]", index );
    const string from = TryTextAt( transition, "from" ).value_or( "?" );
    const string to = TryTextAt( transition, "to" ).value_or( "?" );
    ImGui::Text( "%s  ->  %s", from == "*" ? "any state" : from.c_str(), to.c_str() );
    if ( not TableOfFields( "transition" ) )
        return;
    vector<string> conditions = ConditionsOf( transition );
    for ( size_t at = 0; at < conditions.size(); ++at )
    {
        ImGui::PushID( static_cast<int>( at ) );
        RowLabel( at == 0 ? "When" : "and" );
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 2.0f );
        string condition = conditions[at];
        const bool changed = ImGui::InputTextWithHint( "##when", "name, !name, name < 2", &condition );
        if ( changed )
        {
            vector<string> next = conditions;
            next[at] = condition;
            DataSet( base + ".when", ConditionsValue( next ) );
        }
        Shown( format( "{}.when[{}]", base, at + 1 ) );
        Hold( changed );
        ImGui::SameLine();
        if ( ImGui::SmallButton( "x" ) )
        {
            vector<string> next = conditions;
            next.erase( next.begin() + static_cast<i64>( at ) );
            if ( next.empty() )
                DataRemove( base + ".when" );
            else
                DataSet( base + ".when", ConditionsValue( next ) );
        }
        ImGui::PopID();
    }
    RowLabel( conditions.empty() ? "When" : "" );
    if ( ImGui::SmallButton( "Add Condition" ) )
    {
        const vector<string> parameters = MemberNames( data, "parameters" );
        conditions.push_back( parameters.empty() ? string() : parameters.front() );
        DataSet( base + ".when", ConditionsValue( conditions ) );
    }

    RowLabel( "Exit time" );
    const auto exitMaybe = TryMember( transition, "exit_time" );
    bool exits = exitMaybe.HasValue();
    if ( ImGui::Checkbox( "##exits", &exits ) )
    {
        if ( exits )
            DataSet( base + ".exit_time", Value( 1.0 ) );
        else
            DataRemove( base + ".exit_time" );
    }
    Shown( base + ".exits" );
    if ( exitMaybe and exitMaybe->TryAs<f64>() )
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth( -1.0f );
        f32 share = static_cast<f32>( *exitMaybe->TryAs<f64>() );
        const bool changed = ImGui::SliderFloat( "##exit time", &share, 0.0f, 1.0f, "%.2f" );
        if ( changed )
            DataSet( base + ".exit_time", Value( static_cast<f64>( share ) ) );
        Shown( base + ".exit_time" );
        Hold( changed );
    }

    RowLabel( "Duration" );
    const auto durationMaybe = TryMember( transition, "duration" );
    f32 duration =
        durationMaybe and durationMaybe->TryAs<f64>() ? static_cast<f32>( *durationMaybe->TryAs<f64>() ) : 0.2f;
    const bool changed = ImGui::DragFloat( "##duration", &duration, 0.005f, 0.0f, 5.0f, "%.3f s" );
    if ( changed )
        DataSet( base + ".duration", Value( static_cast<f64>( duration ) ) );
    Shown( base + ".duration" );
    Hold( changed );

    RowLabel( "Interrupt" );
    const auto interruptMaybe = TryMember( transition, "interrupt" );
    bool interrupt = interruptMaybe and interruptMaybe->TryAs<bool>() and *interruptMaybe->TryAs<bool>();
    if ( ImGui::Checkbox( "##interrupt", &interrupt ) )
        DataSet( base + ".interrupt", Value( interrupt ) );
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "Taken while the pose still eases into the state" );
    Shown( base + ".interrupt" );
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
    // A prefab's root is named by its file.
    if ( entity == scene.Root() )
    {
        ImGui::TextUnformatted( RootLabel( document ).c_str() );
        ImGui::TextDisabled( "the prefab's root" );
        return;
    }
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

void Inspector::LiveSelf( const Scene& scene, Entity entity )
{
    const auto playMaybe = mProject.TryPlaying();
    const auto instanceMaybe = scene.TryGet<ScriptInstance>( entity );
    if ( not playMaybe or not instanceMaybe )
        return;
    const ScriptRuntime& runtime = playMaybe->GetWorld().Get<ScriptRuntime>();
    if ( not runtime.Alive( instanceMaybe->mInstanceHandle ) )
        return;
    // As the game has it now: read, never written - the next step would
    // write over it.
    vector<pair<string, string>> fields;
    for ( const auto& [key, value] : runtime.Self( instanceMaybe->mInstanceHandle ).Pairs() )
    {
        const auto nameMaybe = key.TryAs<string>();
        if ( not nameMaybe or *nameMaybe == "entity" )
            continue;
        string shown;
        if ( const auto numberMaybe = value.TryAs<f64>() )
            shown = format( "{:.4g}", *numberMaybe );
        else if ( const auto vectorMaybe = value.TryAs<vec3>() )
            shown = format( "{:.3g}, {:.3g}, {:.3g}", vectorMaybe->x, vectorMaybe->y, vectorMaybe->z );
        else if ( const auto tableMaybe = value.TryAs<LuaTable>() )
            shown = format( "a table of {}", tableMaybe->Pairs().size() );
        else
            shown = value.Describe();
        fields.emplace_back( *nameMaybe, std::move( shown ) );
    }
    ranges::sort( fields );
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled( "self, playing" );
    for ( const auto& [name, shown] : fields )
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted( FieldLabel( name ).c_str() );
        ImGui::TableNextColumn();
        ImGui::TextDisabled( "%s", shown.c_str() );
        Shown( "script.self." + name );
    }
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
        {
            ScriptProps( scene, scriptMaybe->mProps, scriptMaybe->mAsset );
            if ( document.IsPlayed() )
                LiveSelf( scene, entity );
        }
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
                             .mSet = Overridden( component, path ),
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
            // Among the names it takes, when it says; any name else.
            if ( info.mChoices != nullptr )
            {
                if ( ImGui::BeginCombo( id.c_str(), text.c_str() ) )
                {
                    for ( string_view rest = info.mChoices; not rest.empty(); )
                    {
                        const size_t comma = rest.find( ',' );
                        const string choice( rest.substr( 0, comma ) );
                        rest = comma == string_view::npos ? string_view() : rest.substr( comma + 1 );
                        if ( ImGui::Selectable( choice.c_str(), choice == text ) and choice != text )
                            if ( const auto nameMaybe = Name::TryFrom( choice ) )
                            {
                                value = Value( *nameMaybe );
                                changed = true;
                            }
                    }
                    ImGui::EndCombo();
                }
            }
            else if ( ImGui::InputText( id.c_str(), &text ) )
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
    if ( row.mSet )
        mMarked.insert( format( "{}.{}", row.mComponent.ToString(), path ) );
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
