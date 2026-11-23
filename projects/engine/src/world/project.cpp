#include "bubble/world/project.hpp"
#include "bubble/data/json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr i32 cProjectVersion = 1;

unexpected<string> Fail( const Json& at, string_view what )
{
    return unexpected( format( "{}: {}", at.Where(), what ) );
}

expected<string, string> TryText( const Json& value, string_view what )
{
    if ( const auto textMaybe = value.TryAs<string>() )
        return *textMaybe;
    return Fail( value, format( "{} is text, not {}", what, value.Describe() ) );
}

// One binding: its input by the one key it has, and its scale.
expected<ActionBinding, string> TryBinding( const Json& json, const string& where )
{
    const auto objectMaybe = json.TryAs<Json::Object>();
    if ( not objectMaybe )
        return Fail( json, format( "{} is an object - {{ \"key\": \"space\" }} -, not {}", where, json.Describe() ) );
    opt<ActionInput> inputMaybe;
    f32 scale = 1.0f;
    for ( const Json::Member& member : *objectMaybe )
    {
        const string field = format( "{}.{}", where, member.mKey );
        if ( member.mKey == "scale" )
        {
            const auto numberMaybe = member.mValue.TryAs<f64>();
            if ( not numberMaybe )
                return Fail( member.mValue, format( "{} is a number, not {}", field, member.mValue.Describe() ) );
            scale = static_cast<f32>( *numberMaybe );
            continue;
        }
        constexpr array<string_view, 6> cInputs = { "key",          "mouse_button", "mouse_axis", "gamepad_button",
                                                    "gamepad_axis", "touch" };
        if ( ranges::find( cInputs, member.mKey ) == cInputs.end() )
            return Fail( member.mValue, format( "{}: not an input (inputs: key, mouse_button, mouse_axis, "
                                                "gamepad_button, gamepad_axis, touch; and scale)",
                                                field ) );
        if ( inputMaybe )
            return Fail( member.mValue, format( "{}: one input a binding", where ) );
        if ( member.mKey == "touch" )
        {
            const auto listMaybe = member.mValue.TryAs<Json::Array>();
            if ( not listMaybe or listMaybe->size() != 4 or
                 not all_of( listMaybe->begin(), listMaybe->end(), []( const Json& v ) { return v.Is<f64>(); } ) )
                return Fail( member.mValue, format( "{} is [ left, top, right, bottom ] as shares", field ) );
            const auto share = [&]( size_t at ) { return static_cast<f32>( *( *listMaybe )[at].TryAs<f64>() ); };
            inputMaybe = TouchArea{ .mMin = vec2( share( 0 ), share( 1 ) ), .mMax = vec2( share( 2 ), share( 3 ) ) };
            continue;
        }
        const auto nameMaybe = TryText( member.mValue, field );
        if ( not nameMaybe )
            return unexpected( nameMaybe.error() );
        const auto unknown = [&]( string_view kind ) {
            return Fail( member.mValue, format( "{}: no {} {}", field, kind, *nameMaybe ) );
        };
        if ( member.mKey == "key" )
        {
            const auto keyMaybe = TryKeyFromName( *nameMaybe );
            if ( not keyMaybe )
                return unknown( "key" );
            inputMaybe = *keyMaybe;
        }
        else if ( member.mKey == "mouse_button" )
        {
            const auto buttonMaybe = TryMouseButtonFromName( *nameMaybe );
            if ( not buttonMaybe )
                return Fail( member.mValue, format( "{}: no mouse button {} (buttons: left, middle, right, x1, x2)",
                                                    field, *nameMaybe ) );
            inputMaybe = *buttonMaybe;
        }
        else if ( member.mKey == "mouse_axis" )
        {
            const auto axisMaybe = TryMouseAxisFromName( *nameMaybe );
            if ( not axisMaybe )
                return Fail( member.mValue, format( "{}: no mouse axis {} (axes: x, y, wheel)", field, *nameMaybe ) );
            inputMaybe = *axisMaybe;
        }
        else if ( member.mKey == "gamepad_button" )
        {
            const auto buttonMaybe = TryGamepadButtonFromName( *nameMaybe );
            if ( not buttonMaybe )
                return unknown( "gamepad button" );
            inputMaybe = *buttonMaybe;
        }
        else
        {
            const auto axisMaybe = TryGamepadAxisFromName( *nameMaybe );
            if ( not axisMaybe )
                return Fail( member.mValue, format( "{}: no gamepad axis {} (axes: left_x, left_y, right_x, right_y, "
                                                    "left_trigger, right_trigger)",
                                                    field, *nameMaybe ) );
            inputMaybe = *axisMaybe;
        }
    }
    if ( not inputMaybe )
        return Fail( json, format( "{} has no input", where ) );
    return ActionBinding{ .mInput = *inputMaybe, .mScale = scale };
}

bool SnakeCase( string_view name )
{
    return not name.empty() and name.front() >= 'a' and name.front() <= 'z' and
           all_of( name.begin(), name.end(),
                   []( char c ) { return ( c >= 'a' and c <= 'z' ) or ( c >= '0' and c <= '9' ) or c == '_'; } );
}
}

vector<string> ActionNames( const ActionMap& actions )
{
    vector<string> names;
    for ( const Action& action : actions.List() )
        names.push_back( action.mName.ToString() );
    return names;
}

opt<AssetId> ProjectSettings::TryStartLevel( const AssetCatalog& catalog ) const
{
    if ( mStartLevelId and catalog.TryPathOf( mStartLevelId ) )
        return mStartLevelId;
    if ( mStartLevelPathMaybe )
        return catalog.TryIdOf( *mStartLevelPathMaybe );
    return nullopt;
}

expected<ProjectSettings, string> TryReadProjectSettings( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( std::move( jsonMaybe.error() ) );
    const Json& json = *jsonMaybe;
    const auto objectMaybe = json.TryAs<Json::Object>();
    if ( not objectMaybe )
        return Fail( json, format( "a project's settings are a JSON object, not {}", json.Describe() ) );
    ProjectSettings settings;
    bool versioned = false;
    for ( const Json::Member& member : *objectMaybe )
    {
        if ( member.mKey == "version" )
        {
            const auto versionMaybe = member.mValue.TryAs<f64>();
            if ( not versionMaybe )
                return Fail( member.mValue, format( "version is a number, not {}", member.mValue.Describe() ) );
            if ( *versionMaybe != cProjectVersion )
                return Fail( member.mValue, format( "version: this engine reads version {}, not {}", cProjectVersion,
                                                    *versionMaybe ) );
            versioned = true;
        }
        else if ( member.mKey == "name" )
        {
            auto nameMaybe = TryText( member.mValue, "name" );
            if ( not nameMaybe )
                return unexpected( nameMaybe.error() );
            settings.mName = std::move( *nameMaybe );
        }
        else if ( member.mKey == "start_level" )
        {
            const string what = R"(start_level is an asset - { "asset": "<id>", "path": "<path>" } -)";
            const auto linkMaybe = member.mValue.TryAs<Json::Object>();
            if ( not linkMaybe )
                return Fail( member.mValue, format( "{}, not {}", what, member.mValue.Describe() ) );
            for ( const Json::Member& part : *linkMaybe )
            {
                const auto textMaybe = part.mValue.TryAs<string>();
                if ( part.mKey == "asset" and textMaybe and AssetId::TryParse( *textMaybe ) )
                    settings.mStartLevelId = *AssetId::TryParse( *textMaybe );
                else if ( part.mKey == "path" and textMaybe and AssetPath::TryFrom( *textMaybe ) )
                    settings.mStartLevelPathMaybe = *AssetPath::TryFrom( *textMaybe );
                else
                    return Fail( part.mValue, what );
            }
        }
        else if ( member.mKey == "actions" )
        {
            const auto actionsMaybe = member.mValue.TryAs<Json::Object>();
            if ( not actionsMaybe )
                return Fail( member.mValue, "actions is an object of name: [ binding, ... ]" );
            for ( const Json::Member& action : *actionsMaybe )
            {
                if ( not SnakeCase( action.mKey ) )
                    return Fail( action.mValue, format( "actions.{}: an action's name is snake_case", action.mKey ) );
                const auto bindingsMaybe = action.mValue.TryAs<Json::Array>();
                if ( not bindingsMaybe )
                    return Fail( action.mValue, format( "actions.{} is a list of bindings", action.mKey ) );
                for ( size_t at = 0; at < bindingsMaybe->size(); ++at )
                {
                    auto bindingMaybe =
                        TryBinding( ( *bindingsMaybe )[at], format( "actions.{}[{}]", action.mKey, at ) );
                    if ( not bindingMaybe )
                        return unexpected( std::move( bindingMaybe.error() ) );
                    settings.mActions.Bind( action.mKey, bindingMaybe->mInput, bindingMaybe->mScale );
                }
            }
        }
        else
            return Fail( member.mValue,
                         format( "{}: not a field (fields: version, name, start_level, actions)", member.mKey ) );
    }
    if ( not versioned )
        return Fail( json, "a project's settings say their version: \"version\": 1" );
    return settings;
}
}
