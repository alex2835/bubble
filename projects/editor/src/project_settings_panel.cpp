#include "bubble/editor/project_settings_panel.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/name.hpp"
#include "bubble/input/controls.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

namespace bubble
{
namespace
{
// The inputs a binding takes, as project.bubble names them, and as a person
// reads them.
constexpr array<string_view, 6> cKinds = { "key",          "mouse_button", "mouse_axis", "gamepad_button",
                                           "gamepad_axis", "touch" };
constexpr array<const char*, 6> cKindLabels = { "Key",          "Mouse button", "Mouse axis", "Gamepad button",
                                                "Gamepad axis", "Touch" };

Value TextValue( string_view text )
{
    return Value( *Text::TryFrom( text ) );
}

// The JSON as a Value, plainly: what data.set writes back as it was.
Value Plain( const Json& json )
{
    if ( const auto onMaybe = json.TryAs<bool>() )
        return Value( *onMaybe );
    if ( const auto numberMaybe = json.TryAs<f64>() )
        return Value( *numberMaybe );
    if ( const auto textMaybe = json.TryAs<string>() )
        return TextValue( *textMaybe );
    if ( const auto arrayMaybe = json.TryAs<Json::Array>() )
    {
        Value::List list;
        for ( const Json& element : *arrayMaybe )
            list.push_back( Plain( element ) );
        return Value( std::move( list ) );
    }
    Value::Record record;
    if ( const auto objectMaybe = json.TryAs<Json::Object>() )
        for ( const Json::Member& member : *objectMaybe )
            record.push_back( ValueField{ .mName = *Name::TryFrom( member.mKey ), .mValue = Plain( member.mValue ) } );
    return Value( std::move( record ) );
}

// What a binding is bound to: the one member besides its scale.
string KindOf( const Json& binding )
{
    if ( const auto objectMaybe = binding.TryAs<Json::Object>() )
        for ( const Json::Member& member : *objectMaybe )
            if ( member.mKey != "scale" )
                return member.mKey;
    return string( cKinds[0] );
}

f64 ScaleOf( const Json& binding )
{
    const auto scaleMaybe = binding.TryFind( "scale" );
    return scaleMaybe and scaleMaybe->TryAs<f64>() ? *scaleMaybe->TryAs<f64>() : 1.0;
}

// A binding as project.bubble keeps it: its input, and its scale when it is
// not 1.
Value BindingValue( string_view kind, Value input, f64 scale )
{
    Value::Record record{ ValueField{ .mName = *Name::TryFrom( kind ), .mValue = std::move( input ) } };
    if ( scale != 1.0 )
        record.push_back( ValueField{ .mName = "scale"_n, .mValue = Value( scale ) } );
    return Value( std::move( record ) );
}

// What an input of `kind` starts as: its first name, the whole screen for a
// touch.
Value FirstInput( string_view kind )
{
    if ( kind == "touch" )
        return Value( Value::List{ Value( 0.0 ), Value( 0.0 ), Value( 1.0 ), Value( 1.0 ) } );
    return TextValue( InputNames( kind ).front() );
}

// Case aside, for ASCII: what a filter matches.
bool Contains( string_view text, string_view part )
{
    const auto lower = []( char c ) { return c >= 'A' and c <= 'Z' ? static_cast<char>( c - 'A' + 'a' ) : c; };
    return part.empty() or ranges::search( text, part, {}, lower, lower ).begin() != text.end();
}

void RowLabel( const char* label )
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( label );
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
}
}

bool IsProjectSettings( const DataDocument& document )
{
    return document.GetPath().View() == cProjectFile;
}

vector<string_view> InputNames( string_view kind )
{
    vector<string_view> names;
    if ( kind == "key" )
    {
        for ( u32 code = 0; code < cKeySlots; ++code )
            if ( const auto keyMaybe = TryKeyFromCode( code ) )
                names.push_back( KeyName( *keyMaybe ) );
    }
    else if ( kind == "mouse_button" )
        for ( u32 at = 0; at < cMouseButtons; ++at )
            names.push_back( MouseButtonName( static_cast<MouseButton>( at ) ) );
    else if ( kind == "mouse_axis" )
        for ( u32 at = 0; at < cMouseAxes; ++at )
            names.push_back( MouseAxisName( static_cast<MouseAxis>( at ) ) );
    else if ( kind == "gamepad_button" )
        for ( u32 at = 0; at < cGamepadButtons; ++at )
            names.push_back( GamepadButtonName( static_cast<GamepadButton>( at ) ) );
    else if ( kind == "gamepad_axis" )
        for ( u32 at = 0; at < cGamepadAxes; ++at )
            names.push_back( GamepadAxisName( static_cast<GamepadAxis>( at ) ) );
    return names;
}

void ProjectSettingsPanel::Show( const DataDocument& document )
{
    const Json& data = document.GetData();
    mWhere.clear();
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 10.0f, 10.0f ) );
    const bool shown =
        ImGui::BeginChild( "project settings", ImVec2( 0.0f, 0.0f ), ImGuiChildFlags_AlwaysUseWindowPadding );
    ImGui::PopStyleVar();
    if ( shown )
    {
        if ( ImGui::BeginTable( "project", 2, ImGuiTableFlags_SizingStretchProp ) )
        {
            ImGui::TableSetupColumn( "label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.0f );
            ImGui::TableSetupColumn( "value", ImGuiTableColumnFlags_WidthStretch );
            RowLabel( "Name" );
            const auto nameMaybe = data.TryFind( "name" );
            string name = nameMaybe and nameMaybe->TryAs<string>() ? *nameMaybe->TryAs<string>() : string();
            ImGui::InputText( "##name", &name );
            Shown( "name" );
            if ( ImGui::IsItemDeactivatedAfterEdit() )
                if ( Text::TryFrom( name ) )
                    Set( "name", TextValue( name ) );
            RowLabel( "Start level" );
            StartLevel( data );
            ImGui::EndTable();
        }
        ImGui::SeparatorText( "Actions" );
        Actions( data );
    }
    ImGui::EndChild();
    for ( auto& [op, args] : exchange( mPending, {} ) )
        if ( not mProject.Run( op, args ) )
            break;
}

void ProjectSettingsPanel::StartLevel( const Json& data )
{
    const AssetCatalog& catalog = mProject.GetCatalog();
    vector<AssetPath> levels;
    for ( const AssetPath& path : catalog.Paths() )
        if ( path.View().ends_with( ".scene" ) )
            levels.push_back( path );
    ranges::sort( levels, {}, &AssetPath::View );
    // As the file links it: by its id, else by its path.
    string current;
    if ( const auto linkMaybe = data.TryFind( "start_level" ) )
    {
        const auto idMaybe = linkMaybe->TryFind( "asset" );
        const auto pathMaybe = linkMaybe->TryFind( "path" );
        opt<AssetId> levelIdMaybe;
        if ( idMaybe and idMaybe->TryAs<string>() )
            levelIdMaybe = AssetId::TryParse( *idMaybe->TryAs<string>() );
        if ( levelIdMaybe and catalog.TryPathOf( *levelIdMaybe ) )
            current = catalog.TryPathOf( *levelIdMaybe )->String();
        else if ( pathMaybe and pathMaybe->TryAs<string>() )
            current = *pathMaybe->TryAs<string>();
    }
    const bool open = ImGui::BeginCombo( "##start level", current.empty() ? "none" : current.c_str() );
    Shown( "start_level" );
    if ( not open )
        return;
    for ( const AssetPath& path : levels )
        if ( ImGui::Selectable( path.String().c_str(), path.View() == current ) )
            Set( "start_level", Value( *catalog.TryIdOf( path ) ) );
    ImGui::EndCombo();
}

void ProjectSettingsPanel::Actions( const Json& data )
{
    const auto actionsMaybe = data.TryFind( "actions" );
    const auto objectMaybe = actionsMaybe ? actionsMaybe->TryAs<Json::Object>() : nullopt;
    if ( objectMaybe )
        for ( const Json::Member& member : *objectMaybe )
        {
            ImGui::PushID( member.mKey.c_str() );
            Action( *actionsMaybe, member.mKey, member.mValue );
            ImGui::PopID();
        }
    const bool add = ImGui::Button( "Add Action" );
    Shown( "add_action" );
    if ( not add )
        return;
    string name = "new_action";
    for ( int suffix = 2; objectMaybe and actionsMaybe->TryFind( name ); ++suffix )
        name = format( "new_action_{}", suffix );
    if ( objectMaybe )
        Set( "actions." + name, Value( Value::List{} ) );
    else
        Set( "actions", Value( Value::Record{
                            ValueField{ .mName = *Name::TryFrom( name ), .mValue = Value( Value::List{} ) } } ) );
}

void ProjectSettingsPanel::Action( const Json& actions, const string& name, const Json& bindings )
{
    const bool open = ImGui::TreeNodeEx( "##action",
                                         ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                             ImGuiTreeNodeFlags_AllowOverlap,
                                         "%s", name.c_str() );
    if ( not open )
        return;
    // Renamed when the field is left, in its place among the others.
    const ImGuiID field = ImGui::GetID( "##action name" );
    if ( ImGui::GetActiveID() != field or mRenamed != name )
    {
        mRenaming = name;
        mRenamed = name;
    }
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 4.0f );
    ImGui::InputText( "##action name", &mRenaming );
    Shown( name + ".name" );
    if ( ImGui::IsItemDeactivatedAfterEdit() and mRenaming != name )
    {
        if ( not detail::NameValid( mRenaming ) )
            LogError( "actions.{}: an action's name is snake_case, not \"{}\"", name, mRenaming );
        else if ( actions.TryFind( mRenaming ) )
            LogError( "actions.{}: there is an action {} already", name, mRenaming );
        else
        {
            Value::Record renamed;
            for ( const Json::Member& member : *actions.TryAs<Json::Object>() )
                renamed.push_back( ValueField{ .mName = *Name::TryFrom( member.mKey == name ? mRenaming : member.mKey ),
                                               .mValue = Plain( member.mValue ) } );
            Set( "actions", Value( std::move( renamed ) ) );
        }
    }
    ImGui::SameLine();
    const bool remove = ImGui::Button( "Remove" );
    Shown( name + ".remove" );
    if ( remove )
        Remove( "actions." + name );

    const auto listMaybe = bindings.TryAs<Json::Array>();
    const size_t count = listMaybe ? listMaybe->size() : 0;
    if ( count > 0 and ImGui::BeginTable( "bindings", 4, ImGuiTableFlags_SizingStretchProp ) )
    {
        ImGui::TableSetupColumn( "kind", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.5f );
        ImGui::TableSetupColumn( "input", ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( "scale", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f );
        ImGui::TableSetupColumn( "remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight() );
        for ( size_t at = 0; at < count; ++at )
        {
            ImGui::PushID( static_cast<int>( at ) );
            Binding( name, at, ( *listMaybe )[at] );
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    const bool add = ImGui::SmallButton( "Add Binding" );
    Shown( name + ".add_binding" );
    if ( add )
        Set( format( "actions.{}[{}]", name, count + 1 ), BindingValue( cKinds[0], FirstInput( cKinds[0] ), 1.0 ) );
    ImGui::TreePop();
}

void ProjectSettingsPanel::Binding( const string& action, size_t at, const Json& binding )
{
    // Positions in a data path count from 1.
    const string path = format( "actions.{}[{}]", action, at + 1 );
    const string kind = KindOf( binding );
    const f64 scale = ScaleOf( binding );
    const Json input = binding.TryFind( kind ) ? *binding.TryFind( kind ) : Json();
    const string where = format( "{}[{}]", action, at + 1 );

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
    const auto kindAt = ranges::find( cKinds, kind );
    const char* kindLabel = kindAt != cKinds.end() ? cKindLabels[kindAt - cKinds.begin()] : kind.c_str();
    const bool kinds = ImGui::BeginCombo( "##kind", kindLabel );
    Shown( where + ".kind" );
    if ( kinds )
    {
        for ( size_t choice = 0; choice < cKinds.size(); ++choice )
            if ( ImGui::Selectable( cKindLabels[choice], cKinds[choice] == kind ) and cKinds[choice] != kind )
                Set( path, BindingValue( cKinds[choice], FirstInput( cKinds[choice] ), scale ) );
        ImGui::EndCombo();
    }

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
    if ( kind == "touch" )
    {
        // An area as shares of the screen: "left, top, right, bottom".
        string text;
        if ( const auto shareMaybe = input.TryAs<Json::Array>() )
            for ( const Json& share : *shareMaybe )
                if ( const auto numberMaybe = share.TryAs<f64>() )
                    text += format( "{}{}", text.empty() ? "" : ", ", *numberMaybe );
        ImGui::InputTextWithHint( "##area", "left, top, right, bottom", &text );
        Shown( where + ".input" );
        if ( ImGui::IsItemDeactivatedAfterEdit() )
        {
            Value::List shares;
            string_view rest = text;
            while ( not rest.empty() )
            {
                const size_t comma = rest.find( ',' );
                string part( rest.substr( 0, comma ) );
                rest = comma == string_view::npos ? string_view() : rest.substr( comma + 1 );
                erase_if( part, []( char c ) { return c == ' '; } );
                char* end = nullptr;
                const f64 share = strtod( part.c_str(), &end );
                if ( part.empty() or end != part.c_str() + part.size() )
                    break;
                shares.push_back( Value( share ) );
            }
            if ( shares.size() == 4 )
                Set( path, BindingValue( kind, Value( std::move( shares ) ), scale ) );
            else
                LogError( "{}: a touch area is four shares of the screen: left, top, right, bottom", path );
        }
    }
    else
    {
        const auto nameMaybe = input.TryAs<string>();
        const string current = nameMaybe ? *nameMaybe : string();
        const bool names = ImGui::BeginCombo( "##input", current.c_str(), ImGuiComboFlags_HeightLarge );
        Shown( where + ".input" );
        if ( names )
        {
            // A key among a hundred and more: typed for.
            if ( kind == "key" )
            {
                if ( ImGui::IsWindowAppearing() )
                {
                    mFilter.clear();
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::SetNextItemWidth( -1.0f );
                ImGui::InputTextWithHint( "##filter", "filter", &mFilter );
            }
            for ( const string_view name : InputNames( kind ) )
                if ( Contains( name, kind == "key" ? string_view( mFilter ) : string_view() ) and
                     ImGui::Selectable( string( name ).c_str(), name == current ) )
                    Set( path, BindingValue( kind, TextValue( name ), scale ) );
            ImGui::EndCombo();
        }
    }

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth( -1.0f );
    f64 typed = scale;
    ImGui::InputDouble( "##scale", &typed, 0.0, 0.0, "%g" );
    Shown( where + ".scale" );
    ImGui::SetItemTooltip( "Scale: what the input is multiplied by - -1 for the other way along an axis" );
    if ( ImGui::IsItemDeactivatedAfterEdit() and typed != scale )
        Set( path, BindingValue( kind, Plain( input ), typed ) );

    ImGui::TableNextColumn();
    const bool remove = ImGui::Button( "x", ImVec2( ImGui::GetFrameHeight(), 0.0f ) );
    Shown( where + ".remove" );
    if ( remove )
        Remove( path );
}

opt<vec2> ProjectSettingsPanel::TryWhere( string_view what ) const
{
    const auto found = mWhere.find( string( what ) );
    return found != mWhere.end() ? opt<vec2>( found->second ) : nullopt;
}

void ProjectSettingsPanel::Shown( string what )
{
    const ImVec2 low = ImGui::GetItemRectMin();
    const ImVec2 high = ImGui::GetItemRectMax();
    mWhere.insert_or_assign( std::move( what ), vec2( low.x + high.x, low.y + high.y ) * 0.5f );
}

void ProjectSettingsPanel::Set( const string& path, Value value )
{
    mPending.emplace_back( "data.set",
                           Value::Record{ ValueField{ .mName = "path"_n, .mValue = TextValue( path ) },
                                          ValueField{ .mName = "value"_n, .mValue = std::move( value ) } } );
}

void ProjectSettingsPanel::Remove( const string& path )
{
    mPending.emplace_back( "data.remove",
                           Value::Record{ ValueField{ .mName = "path"_n, .mValue = TextValue( path ) } } );
}
}
