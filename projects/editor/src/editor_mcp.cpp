#include "bubble/editor/editor_mcp.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/png.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/editing/editor_lua.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/play_session.hpp"
#include "bubble/editor/viewport.hpp"
#include "bubble/reflection/field_path.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
Json Object( initializer_list<pair<string_view, Json>> members )
{
    Json::Object object;
    for ( const auto& [key, value] : members )
        object.push_back( Json::Member{ .mKey = string( key ), .mValue = value } );
    return Json( std::move( object ) );
}

// An argument a tool takes, as its JSON schema says it: a type left empty
// takes any JSON.
struct Param
{
    string_view mName;
    string_view mType;
    string_view mDescription;
    bool mRequired = false;
};

Json Schema( initializer_list<Param> params )
{
    Json::Object properties;
    Json::Array required;
    for ( const Param& param : params )
    {
        Json::Object property;
        if ( not param.mType.empty() )
            property.push_back( Json::Member{ .mKey = "type", .mValue = Json( string( param.mType ) ) } );
        property.push_back( Json::Member{ .mKey = "description", .mValue = Json( string( param.mDescription ) ) } );
        properties.push_back( Json::Member{ .mKey = string( param.mName ), .mValue = Json( std::move( property ) ) } );
        if ( param.mRequired )
            required.push_back( Json( string( param.mName ) ) );
    }
    return Object( { { "type", Json( "object"s ) },
                     { "properties", Json( std::move( properties ) ) },
                     { "required", Json( std::move( required ) ) } } );
}

Json::Array Said( string text )
{
    return Json::Array{ McpText( std::move( text ) ) };
}

opt<string> TextArg( const Json& args, string_view name )
{
    const auto foundMaybe = args.TryFind( name );
    const auto textMaybe = foundMaybe ? foundMaybe->TryAs<string>() : nullopt;
    return textMaybe ? opt<string>( *textMaybe ) : nullopt;
}

opt<f64> NumberArg( const Json& args, string_view name )
{
    const auto foundMaybe = args.TryFind( name );
    const auto numberMaybe = foundMaybe ? foundMaybe->TryAs<f64>() : nullopt;
    return numberMaybe ? opt<f64>( *numberMaybe ) : nullopt;
}

bool FlagArg( const Json& args, string_view name )
{
    const auto foundMaybe = args.TryFind( name );
    const auto flagMaybe = foundMaybe ? foundMaybe->TryAs<bool>() : nullopt;
    return flagMaybe and *flagMaybe;
}

unexpected<string> NoScene()
{
    return unexpected( "no level or prefab is open: run_operator document.open { \"path\": \"levels/main.scene\" }"s );
}

// The entity at the path `args` has under `name`; the scene's root for "/".
expected<Entity, string> TryEntityArg( const Scene& scene, const Json& args, string_view name, string_view fallback )
{
    const string text = TextArg( args, name ).value_or( string( fallback ) );
    if ( text.empty() )
        return unexpected( format( "{}: an entity's path expected, as /props/door", name ) );
    if ( text == "/" )
        return scene.Root();
    const auto pathMaybe = Text::TryFrom( text );
    if ( not pathMaybe )
        return unexpected( format( "{}: {}", name, pathMaybe.error() ) );
    auto entityMaybe = TryResolve( scene, EntityPath{ *pathMaybe } );
    if ( not entityMaybe )
        return unexpected( format( "{}: {} (scene_tree lists the entities)", name, entityMaybe.error() ) );
    return *entityMaybe;
}

string ComponentsOf( const Document& document, Entity entity )
{
    string names;
    for ( const ComponentTypes::Type& type : document.GetComponentTypes().All() )
        if ( type.mGet( document.GetScene(), entity ) )
            names += ( names.empty() ? "" : ", " ) + type.mName.ToString();
    return names;
}

void Tree( const Document& document, const AssetCatalog& assets, Entity entity, i32 depth, i32 deepest, string& out )
{
    const Scene& scene = document.GetScene();
    for ( const Entity child : ChildrenOf( scene, entity ) )
    {
        out += string( static_cast<size_t>( depth ) * 2, ' ' ) + PathOf( scene, child ).mPath.String();
        if ( const string components = ComponentsOf( document, child ); not components.empty() )
            out += "  [" + components + "]";
        if ( const auto instanceMaybe = scene.TryGet<PrefabInstance>( child ) )
        {
            const auto pathMaybe = assets.TryPathOf( instanceMaybe->mPrefab );
            out += "  <- " + ( pathMaybe ? pathMaybe->String() : instanceMaybe->mPrefab.ToString() );
        }
        out += '\n';
        const size_t under = ChildrenOf( scene, child ).size();
        if ( under == 0 )
            continue;
        if ( depth + 1 < deepest )
            Tree( document, assets, child, depth + 1, deepest, out );
        else
            out += string( static_cast<size_t>( depth + 1 ) * 2, ' ' ) + format( "... {} under it\n", under );
    }
}

string_view LevelOf( LogLevel level )
{
    switch ( level )
    {
        case LogLevel::Info: return "info";
        case LogLevel::Warning: return "warning";
        case LogLevel::Error: return "error";
        case LogLevel::Script: return "script";
    }
    return "info";
}

McpTool StateTool( EditorProject& project )
{
    return McpTool{
        .mName = "editor_state",
        .mDescription = "The project open, its documents - levels, prefabs, data files: which is shown, which is "
                        "active, which are not saved -, the selection in the shown scene and whether the game plays.",
        .mInputSchema = Schema( {} ),
        .mTryCall = [&project]( const Json& ) -> expected<Json::Array, string> {
            Workspace& workspace = project.GetWorkspace();
            const auto activeMaybe = workspace.TryActive();
            const auto shownMaybe = workspace.TryShownScene();
            Json::Array documents;
            for ( const Scope<Document>& document : workspace.All() )
            {
                const auto pathMaybe = document->TryPath();
                documents.push_back( Object( { { "path", pathMaybe ? Json( pathMaybe->String() ) : Json() },
                                               { "kind", Json( document->TryPrefabNodes() ? "prefab"s : "level"s ) },
                                               { "shown", Json( shownMaybe and &*shownMaybe == document.get() ) },
                                               { "active", Json( activeMaybe and &*activeMaybe == document.get() ) },
                                               { "not_saved", Json( document->IsDirty() ) },
                                               { "played", Json( document->IsPlayed() ) } } ) );
            }
            const auto activeDataMaybe = workspace.TryActiveData();
            Json::Array data;
            for ( const Scope<DataDocument>& document : workspace.AllData() )
                data.push_back( Object( { { "path", Json( document->GetPath().String() ) },
                                          { "active", Json( activeDataMaybe and &*activeDataMaybe == document.get() ) },
                                          { "not_saved", Json( document->IsDirty() ) } } ) );
            Json::Array selection;
            Json activeEntity;
            if ( shownMaybe )
            {
                const Scene& scene = shownMaybe->GetScene();
                for ( const Entity entity : shownMaybe->GetSelection().All() )
                    selection.push_back( Json( PathOf( scene, entity ).mPath.String() ) );
                if ( const Entity active = shownMaybe->GetSelection().Active(); active != Entity::Null )
                    activeEntity = Json( PathOf( scene, active ).mPath.String() );
            }
            const auto playMaybe = project.TryPlaying();
            const auto rootMaybe = project.TryRoot();
            const Json state = Object( { { "project", Json( project.GetSettings().mName ) },
                                         { "folder", rootMaybe ? Json( PathToUtf8( *rootMaybe ) ) : Json() },
                                         { "documents", Json( std::move( documents ) ) },
                                         { "data_documents", Json( std::move( data ) ) },
                                         { "selection", Json( std::move( selection ) ) },
                                         { "active_entity", activeEntity },
                                         { "game", Json( not playMaybe                    ? "stopped"s
                                                         : playMaybe->GetWorld().Paused() ? "paused"s
                                                                                          : "playing"s ) } } );
            return Said( state.Write() );
        }
    };
}

McpTool TreeTool( EditorProject& project )
{
    return McpTool{ .mName = "scene_tree",
                    .mDescription =
                        "The shown scene's entities, a line each: its path - what the other tools name it by -, "
                        "its components, and the prefab an instance is of, indented by depth.",
                    .mInputSchema =
                        Schema( { { "root", "string",
                                    "Where to start: an entity's path; the scene's root, \"/\", "
                                    "when left out" },
                                  { "depth", "integer", "How many levels down; all of them when left out" } } ),
                    .mTryCall = [&project]( const Json& args ) -> expected<Json::Array, string> {
                        const auto documentMaybe = project.GetWorkspace().TryShownScene();
                        if ( not documentMaybe )
                            return NoScene();
                        auto rootMaybe = TryEntityArg( documentMaybe->GetScene(), args, "root", "/" );
                        if ( not rootMaybe )
                            return unexpected( std::move( rootMaybe.error() ) );
                        const i32 deepest = static_cast<i32>( NumberArg( args, "depth" ).value_or( 1000 ) );
                        string out;
                        Tree( *documentMaybe, project.GetCatalog(), *rootMaybe, 0, max( deepest, 1 ), out );
                        return Said( out.empty() ? "nothing under it"s : out );
                    } };
}

McpTool EntityTool( EditorProject& project )
{
    return McpTool{ .mName = "get_entity",
                    .mDescription =
                        "An entity of the shown scene as the level file holds it - its name, every component's "
                        "fields, what is under it -, as JSON: the values set_field takes.",
                    .mInputSchema = Schema( { { "entity", "string", "The entity's path, as /props/door", true } } ),
                    .mTryCall = [&project]( const Json& args ) -> expected<Json::Array, string> {
                        Workspace& workspace = project.GetWorkspace();
                        const auto documentMaybe = workspace.TryShownScene();
                        if ( not documentMaybe )
                            return NoScene();
                        const Scene& scene = documentMaybe->GetScene();
                        auto entityMaybe = TryEntityArg( scene, args, "entity", "" );
                        if ( not entityMaybe )
                            return unexpected( std::move( entityMaybe.error() ) );
                        auto jsonMaybe = TryEntityToJson( scene, *entityMaybe, workspace.GetFileContext() );
                        if ( not jsonMaybe )
                            return unexpected( std::move( jsonMaybe.error() ) );
                        return Said( jsonMaybe->Write() );
                    } };
}

McpTool SetFieldTool( EditorProject& project )
{
    return McpTool{
        .mName = "set_field",
        .mDescription = "Sets a field of an entity's component in the shown scene, the value as the level file "
                        "writes it: a number, a text, [x, y, z] for a vec3 - a rotation in radians -, [r, g, b, a] "
                        "for a color, { \"asset\": id, \"path\": path } for an asset, a path for an entity. One "
                        "step of the history.",
        .mInputSchema = Schema( { { "entity", "string", "The entity's path, as /props/door", true },
                                  { "component", "string", "The component, as transform or point_light", true },
                                  { "path", "string", "The field, as position, shadow.bias or steps[2]", true },
                                  { "value", "", "The value, as get_entity shows it", true } } ),
        .mTryCall = [&project]( const Json& args ) -> expected<Json::Array, string> {
            Workspace& workspace = project.GetWorkspace();
            const auto documentMaybe = workspace.TryShownScene();
            if ( not documentMaybe )
                return NoScene();
            const Scene& scene = documentMaybe->GetScene();
            auto entityMaybe = TryEntityArg( scene, args, "entity", "" );
            if ( not entityMaybe )
                return unexpected( std::move( entityMaybe.error() ) );
            const string component = TextArg( args, "component" ).value_or( "" );
            const string path = TextArg( args, "path" ).value_or( "" );
            const auto valueMaybe = args.TryFind( "value" );
            if ( not valueMaybe )
                return unexpected( "value: the field's value expected"s );
            // What the field is decides how its JSON is read, as a file's is.
            const ComponentTypes& componentTypes = documentMaybe->GetComponentTypes();
            const auto nameMaybe = Name::TryFrom( component );
            const auto typeMaybe = nameMaybe ? componentTypes.TryFind( *nameMaybe ) : nullopt;
            if ( not typeMaybe )
                return unexpected(
                    format( "component: {} is not one (components: {})", component, componentTypes.Names() ) );
            const MetaAny owner = typeMaybe->mGet( scene, *entityMaybe );
            if ( not owner )
                return unexpected( format( "{} has no {}", PathOf( scene, *entityMaybe ).mPath.String(), component ) );
            const auto fieldPathMaybe = FieldPath::TryParse( path );
            if ( not fieldPathMaybe )
                return unexpected( format( "path: {}", fieldPathMaybe.error() ) );
            Reflection& reflection = componentTypes.GetReflection();
            const auto currentMaybe = reflection.TryGetField( owner, *fieldPathMaybe );
            if ( not currentMaybe )
                return unexpected( currentMaybe.error() );
            const auto valueTypeMaybe = reflection.TryValueTypeOf( currentMaybe->type() );
            if ( not valueTypeMaybe )
                return unexpected( valueTypeMaybe.error() );
            auto readMaybe = TryValueFromJson( *valueMaybe, *valueTypeMaybe, format( "{}.{}", component, path ),
                                               workspace.GetAssets() );
            if ( not readMaybe )
                return unexpected( std::move( readMaybe.error() ) );
            // Set on the shown scene, as a click in the viewport would make it
            // the active one.
            workspace.Activate( *documentMaybe );
            const Value::Record record{ ValueField{
                                            .mName = "entities"_n,
                                            .mValue = Value( Value::List{ Value( PathOf( scene, *entityMaybe ) ) } ) },
                                        ValueField{ .mName = "component"_n, .mValue = Value( *nameMaybe ) },
                                        ValueField{ .mName = "path"_n, .mValue = Value( *Text::TryFrom( path ) ) },
                                        ValueField{ .mName = "value"_n, .mValue = std::move( *readMaybe ) } };
            if ( auto setMaybe = project.GetOperators().TryRun( workspace, "property.set", record ); not setMaybe )
                return unexpected( std::move( setMaybe.error() ) );
            return Said( "set" );
        }
    };
}

McpTool OperatorsTool( EditorProject& project )
{
    return McpTool{ .mName = "list_operators",
                    .mDescription =
                        "Every operator - what the editor's menus, keys and panels run - with its arguments: what "
                        "run_operator and run_lua's editor.ops take. An argument with ? may be left out; then it "
                        "means the selection, or what the operator says.",
                    .mInputSchema = Schema( {} ),
                    .mTryCall = [&project]( const Json& ) -> expected<Json::Array, string> {
                        string out;
                        for ( const Operator& op : project.GetOperators().All() )
                        {
                            out += op.mId + " - " + op.mLabel + ( op.mEdits ? "" : " (no step of the history)" ) + ":";
                            if ( op.mArgs.empty() )
                                out += " no arguments";
                            for ( size_t at = 0; at < op.mArgs.size(); ++at )
                            {
                                const OperatorArg& arg = op.mArgs[at];
                                out += format( "{} {}{}: {}", at == 0 ? "" : ",", arg.mName.ToString(),
                                               arg.mRequired ? "" : "?", arg.mType.Describe() );
                            }
                            out += '\n';
                        }
                        return Said( out );
                    } };
}

McpTool RunOperatorTool( EditorProject& project )
{
    return McpTool{
        .mName = "run_operator",
        .mDescription = "Runs an operator, as list_operators lists them, on the active document: one step of its "
                        "history - edit.undo takes it back. Its arguments are JSON by what it declares: entities a "
                        "list of paths; a value of no declared kind - property.set's - with a tag where JSON cannot "
                        "tell: { \"$vec3\": [1, 2, 3] }, { \"$entity\": \"/door\" }.",
        .mInputSchema = Schema( { { "id", "string", "The operator, as entity.create or document.save", true },
                                  { "args", "object",
                                    "Its arguments by name, as { \"entities\": [\"/lamp\"] }; none when left "
                                    "out" } } ),
        .mTryCall = [&project]( const Json& args ) -> expected<Json::Array, string> {
            const auto idMaybe = TextArg( args, "id" );
            if ( not idMaybe )
                return unexpected( "id: an operator's id expected (list_operators lists them)"s );
            const auto operatorArgsMaybe = args.TryFind( "args" );
            const Json operatorArgs = operatorArgsMaybe ? *operatorArgsMaybe : Json();
            if ( auto ranMaybe = project.GetOperators().TryRun( project.GetWorkspace(), *idMaybe, operatorArgs );
                 not ranMaybe )
                return unexpected( std::move( ranMaybe.error() ) );
            return Said( "done" );
        }
    };
}

McpTool LuaTool( EditorProject& project )
{
    // One VM for the server's life; what a run prints, gathered for it.
    const auto printed = CreateRef<string>();
    const auto lua =
        CreateRef<EditorLua>( project.GetWorkspace(), project.GetOperators(), [printed]( string_view text ) {
            *printed += text;
            *printed += '\n';
        } );
    return McpTool{ .mName = "run_lua",
                    .mDescription =
                        "Runs Luau in the editor's own VM - never the game's: editor.ops.<group>.<name>( args ) runs "
                        "an operator, editor.get( path, \"component.field\" ) reads, editor.children( path ), "
                        "editor.components( path ), editor.exists( path ), editor.selection(); "
                        "editor.group( label, fn ) makes what fn does one step of the history. What it prints comes "
                        "back.",
                    .mInputSchema = Schema( { { "source", "string", "The Luau to run", true } } ),
                    .mTryCall = [lua, printed]( const Json& args ) -> expected<Json::Array, string> {
                        const auto sourceMaybe = TextArg( args, "source" );
                        if ( not sourceMaybe )
                            return unexpected( "source: the Luau to run expected"s );
                        printed->clear();
                        if ( auto ranMaybe = lua->TryRun( *sourceMaybe, "mcp.luau" ); not ranMaybe )
                            return unexpected( *printed + ranMaybe.error() );
                        return Said( printed->empty() ? "done"s : *printed );
                    } };
}

McpTool AssetsTool( EditorProject& project )
{
    return McpTool{
        .mName = "list_assets",
        .mDescription = "The project's files, a line each: its path, what it imports as - level, prefab, model, "
                        "material, script... -, its asset id, and why it did not import when it did not.",
        .mInputSchema = Schema( { { "folder", "string", "Only what is under this folder, as materials" } } ),
        .mTryCall = [&project]( const Json& args ) -> expected<Json::Array, string> {
            auto filesMaybe = project.GetFiles().TryList();
            if ( not filesMaybe )
                return unexpected( std::move( filesMaybe.error() ) );
            string folder = TextArg( args, "folder" ).value_or( "" );
            if ( not folder.empty() and not folder.ends_with( '/' ) )
                folder += '/';
            vector<string> lines;
            for ( const ListedFile& file : *filesMaybe )
            {
                const string& path = file.mPath.String();
                if ( path.starts_with( ".bubble/" ) or path.ends_with( ".import" ) or not path.starts_with( folder ) )
                    continue;
                string line = path;
                if ( const string_view kind = project.KindOf( file.mPath ); not kind.empty() )
                    line += format( "  {}", kind );
                if ( const auto idMaybe = project.GetCatalog().TryIdOf( file.mPath ) )
                    line += "  " + idMaybe->ToString();
                if ( const auto errorMaybe = project.TryErrorOf( file.mPath ) )
                    line += "  error: " + *errorMaybe;
                lines.push_back( std::move( line ) );
            }
            ranges::sort( lines );
            string out;
            for ( const string& line : lines )
                out += line + '\n';
            return Said( out.empty() ? "no files there"s : out );
        }
    };
}

McpTool LogTool()
{
    return McpTool{
        .mName = "read_log",
        .mDescription = "What the editor logged - imports, scripts' errors and prints, operators' failures -, the "
                        "newest last, each with its number; the last line says what to ask from next time.",
        .mInputSchema = Schema( { { "since", "integer", "From this number on; the last ones when left out" },
                                  { "limit", "integer", "At most this many, the newest; 50 when left out" },
                                  { "errors_only", "boolean", "Errors and warnings alone" } } ),
        .mTryCall = []( const Json& args ) -> expected<Json::Array, string> {
            vector<LogEntry> entries;
            const u64 next =
                LogReadSince( static_cast<u64>( max( NumberArg( args, "since" ).value_or( 0 ), 0.0 ) ), entries );
            if ( FlagArg( args, "errors_only" ) )
                erase_if( entries, []( const LogEntry& entry ) {
                    return entry.mLevel != LogLevel::Error and entry.mLevel != LogLevel::Warning;
                } );
            const size_t limit = static_cast<size_t>( max( NumberArg( args, "limit" ).value_or( 50 ), 1.0 ) );
            const size_t from = entries.size() > limit ? entries.size() - limit : 0;
            string out;
            for ( size_t at = from; at < entries.size(); ++at )
                out += format( "[{}] {}: {}\n", entries[at].mIndex, LevelOf( entries[at].mLevel ), entries[at].mText );
            out += format( "next: {}", next );
            return Said( out );
        }
    };
}
}

string EncodeBase64( string_view bytes )
{
    static constexpr string_view cDigits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string out;
    out.reserve( ( bytes.size() + 2 ) / 3 * 4 );
    for ( size_t at = 0; at < bytes.size(); at += 3 )
    {
        const size_t left = bytes.size() - at;
        const auto byte = [&]( size_t offset ) { return u32( static_cast<u8>( bytes[at + offset] ) ); };
        const u32 group = ( byte( 0 ) << 16 ) | ( left > 1 ? byte( 1 ) << 8 : 0u ) | ( left > 2 ? byte( 2 ) : 0u );
        out += cDigits[( group >> 18 ) & 63];
        out += cDigits[( group >> 12 ) & 63];
        out += left > 1 ? cDigits[( group >> 6 ) & 63] : '=';
        out += left > 2 ? cDigits[group & 63] : '=';
    }
    return out;
}

Rgba8Picture ShrinkRgba8( span<const u8> pixels, ivec2 size, i32 longest )
{
    const i32 side = max( size.x, size.y );
    if ( side <= longest )
        return Rgba8Picture{ .mPixels = vector<u8>( pixels.begin(), pixels.end() ), .mSize = size };
    const f64 scale = static_cast<f64>( longest ) / side;
    const ivec2 shrunk( max( 1, static_cast<i32>( size.x * scale + 0.5 ) ),
                        max( 1, static_cast<i32>( size.y * scale + 0.5 ) ) );
    vector<u8> out( static_cast<size_t>( shrunk.x ) * static_cast<size_t>( shrunk.y ) * 4 );
    for ( i32 y = 0; y < shrunk.y; ++y )
    {
        // The source's rows and columns this pixel covers: at least one.
        const i32 top = y * size.y / shrunk.y;
        const i32 bottom = max( top + 1, ( y + 1 ) * size.y / shrunk.y );
        for ( i32 x = 0; x < shrunk.x; ++x )
        {
            const i32 left = x * size.x / shrunk.x;
            const i32 right = max( left + 1, ( x + 1 ) * size.x / shrunk.x );
            array<u32, 4> sum{};
            for ( i32 row = top; row < bottom; ++row )
                for ( i32 column = left; column < right; ++column )
                    for ( size_t channel = 0; channel < 4; ++channel )
                        sum[channel] += pixels[( static_cast<size_t>( row ) * static_cast<size_t>( size.x ) +
                                                 static_cast<size_t>( column ) ) *
                                                   4 +
                                               channel];
            const u32 count = static_cast<u32>( ( bottom - top ) * ( right - left ) );
            for ( size_t channel = 0; channel < 4; ++channel )
                out[( static_cast<size_t>( y ) * static_cast<size_t>( shrunk.x ) + static_cast<size_t>( x ) ) * 4 +
                    channel] = static_cast<u8>( ( sum[channel] + count / 2 ) / count );
        }
    }
    return Rgba8Picture{ .mPixels = std::move( out ), .mSize = shrunk };
}

vector<McpTool> EditorMcpTools( EditorProject& project )
{
    return vector<McpTool>{ StateTool( project ),    TreeTool( project ),      EntityTool( project ),
                            SetFieldTool( project ), OperatorsTool( project ), RunOperatorTool( project ),
                            LuaTool( project ),      AssetsTool( project ),    LogTool() };
}

McpTool ViewportScreenshotTool( Gpu& gpu, const Viewport& viewport )
{
    return McpTool{ .mName = "screenshot",
                    .mDescription = "A picture of the viewport as the editor last drew it: the shown scene through the "
                                    "editor's camera, the selection outlined - or the game, while it plays.",
                    .mInputSchema = Schema( { { "max_size", "integer",
                                                "Its longer side at most this many pixels, 64 to 2048; "
                                                "640 when left out" } } ),
                    .mTryCall = [&gpu, &viewport]( const Json& args ) -> expected<Json::Array, string> {
                        const GpuTexture& picture = viewport.Picture();
                        if ( not picture )
                            return unexpected( "the viewport has drawn nothing yet: is it shown?"s );
                        auto pixelsMaybe = gpu.TryReadRgba8( picture );
                        if ( not pixelsMaybe )
                            return unexpected( std::move( pixelsMaybe.error() ) );
                        const i32 longest =
                            static_cast<i32>( clamp( NumberArg( args, "max_size" ).value_or( 640 ), 64.0, 2048.0 ) );
                        const Rgba8Picture shrunk = ShrinkRgba8( *pixelsMaybe, picture.Size(), longest );
                        const string png = EncodePng( shrunk.mPixels, shrunk.mSize );
                        return Json::Array{ Object( { { "type", Json( "image"s ) },
                                                      { "data", Json( EncodeBase64( png ) ) },
                                                      { "mimeType", Json( "image/png"s ) } } ) };
                    } };
}

string EditorMcpInstructions()
{
    return "Bubble's editor, running: what it has open is read and changed here as its own menus do it. Entities "
           "are named by their paths from the scene's root: /lamp, /props/door.\n"
           "Reading: editor_state (documents, selection, the game), scene_tree (the shown scene's entities and "
           "components), get_entity (an entity as the level file holds it), list_assets, read_log, screenshot.\n"
           "Changing: set_field sets a component's field from JSON as the files write it; run_operator runs any "
           "operator list_operators lists - entity.create, component.add, prefab.instantiate, document.save...; "
           "run_lua runs the editor's Luau for many changes at once, editor.group making them one step.\n"
           "Every change is a step of the document's history, undone by run_operator edit.undo, and stays in "
           "memory until document.save writes the file. document.open { \"path\": ... } opens a level or prefab; "
           "play.start and play.stop play the shown scene on a copy.";
}
}
