#include "bubble/editor/editor_project.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/import/project_setup.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/scene/engine_file.hpp"
#include "bubble/scripts/script_declarations.hpp"
#include "bubble/types/algorithm.hpp"

namespace bubble
{
void EditorProject::Report( const ProjectRefresh& refresh )
{
    for ( const string& warning : refresh.mWarnings )
        LogWarning( "{}", warning );
    for ( const AssetPath& path : refresh.mImported )
        mErrors.erase( path.String() );
    // Each error says its file first: "scripts/player.luau:3: ...".
    for ( const string& error : refresh.mErrors )
    {
        LogError( "{}", error );
        const size_t colon = error.find( ':' );
        if ( colon != string::npos )
            mErrors.insert_or_assign( error.substr( 0, colon ), error.substr( colon + 1 ) );
    }
    for ( const auto& [path, assetId] : refresh.mOrphans )
        LogWarning( "{}: an .import whose source is gone", path.View() );
}

OptRef<const string> EditorProject::TryErrorOf( const AssetPath& path ) const
{
    const auto found = mErrors.find( path.View() );
    return found != mErrors.end() ? OptRef<const string>( found->second ) : nullopt;
}

OptRef<const OsPath> EditorProject::TryRoot() const
{
    return mRootMaybe ? OptRef<const OsPath>( *mRootMaybe ) : nullopt;
}

expected<Scope<EditorProject>, string> EditorProject::TryOpen( ProjectFiles files,
                                                               AssetRegistry::ReadImported read,
                                                               AudioDevice& audio )
{
    const auto textMaybe = files.TryRead( *AssetPath::TryFrom( cProjectFile ) );
    if ( not textMaybe )
        return unexpected( format( "{}: {}", cProjectFile, textMaybe.error() ) );
    auto settingsMaybe = TryReadProjectSettings( *textMaybe );
    if ( not settingsMaybe )
        return unexpected( format( "{}:{}", cProjectFile, settingsMaybe.error() ) );
    Scope<EditorProject> project(
        new EditorProject( std::move( files ), std::move( read ), std::move( *settingsMaybe ) ) );
    if ( auto startedMaybe = project->TryStart( audio ); not startedMaybe )
        return unexpected( startedMaybe.error() );
    return project;
}

expected<Scope<EditorProject>, string> EditorProject::TryOpen( const OsPath& root, AudioDevice& audio )
{
    auto projectMaybe = TryOpen( ProjectFiles::FromDirectory( root ), AssetRegistry::FromDirectory( root ), audio );
    if ( projectMaybe )
        ( *projectMaybe )->mRootMaybe = root;
    return projectMaybe;
}

EditorProject::EditorProject( ProjectFiles files, AssetRegistry::ReadImported read, ProjectSettings settings )
    : mSettings( std::move( settings ) ),
      mProject( files, Importers(), cEngineFileExtensions ),
      mAssets( mProject.GetCatalog(), std::move( read ), mApp.GetJobs() ),
      mWorkspace( mApp.GetComponentTypes(), files, mProject.GetCatalog() )
{
}

EditorProject::~EditorProject() = default;

expected<void, string> EditorProject::TryStart( AudioDevice& audio )
{
    hmap<string, string> aliases;
    if ( const auto luaurcMaybe = mProject.GetFiles().TryRead( *AssetPath::TryFrom( ".luaurc" ) ) )
    {
        auto readMaybe = TryReadLuaurc( *luaurcMaybe );
        if ( not readMaybe )
            return unexpected( format( ".luaurc: {}", readMaybe.error() ) );
        aliases = std::move( *readMaybe );
    }
    const string game = mSettings.mName.empty() ? "bubble" : mSettings.mName;
    AddEngineModules( mApp, mAssets, audio,
                      ScriptModuleOptions{ .mGlobals = {},
                                           .mAliases = aliases,
                                           .mTimeLimit = 1.0,
                                           .mProgress = CreateRef<ScriptProgress>(),
                                           .mSaves = nullptr,
                                           .mGameVersion = game } );

    // Scripts are checked against the engine's declarations with the
    // project's actions, which go under .bubble too, for a language server.
    vector<string> actions;
    for ( const Action& action : mSettings.mActions.List() )
        actions.push_back( action.mName.ToString() );
    auto checkMaybe =
        TryProjectScriptCheck( mApp, mProject.GetFiles(), mProject.GetCatalog(), ScriptLabels{ .mActions = actions } );
    if ( not checkMaybe )
        return unexpected( checkMaybe.error() );
    const AssetPath declarationsPath = *AssetPath::TryFrom( ".bubble/bubble.d.luau" );
    if ( mProject.GetFiles().TryRead( declarationsPath ).value_or( string() ) != checkMaybe->mDeclarations )
        if ( auto writtenMaybe = mProject.GetFiles().TryWrite( declarationsPath, checkMaybe->mDeclarations );
             not writtenMaybe )
            LogWarning( "{}: {}", declarationsPath.View(), writtenMaybe.error() );
    AddEngineImporters( mProject, mApp, std::move( *checkMaybe ) );
    RegisterEngineLoaders( mAssets );
    AddBuiltinOperators( mOperators );

    const auto refreshMaybe = mProject.TryRefresh();
    if ( not refreshMaybe )
        return unexpected( refreshMaybe.error() );
    Report( *refreshMaybe );
    return {};
}

void EditorProject::Refresh()
{
    const auto refreshMaybe = mProject.TryRefresh();
    if ( not refreshMaybe )
    {
        LogError( "{}", refreshMaybe.error() );
        return;
    }
    Report( *refreshMaybe );
    for ( const AssetPath& path : refreshMaybe->mImported )
        if ( auto reloadedMaybe = mAssets.TryReload( path ); not reloadedMaybe )
            LogError( "{}", reloadedMaybe.error() );
}

expected<void, string> EditorProject::TryOpenStartLevel()
{
    const auto levelIdMaybe = mSettings.TryStartLevel( mProject.GetCatalog() );
    if ( not levelIdMaybe )
        return unexpected( format( "{}: start_level: no such level in the project", cProjectFile ) );
    const auto pathMaybe = mProject.GetCatalog().TryPathOf( *levelIdMaybe );
    if ( not pathMaybe )
        return unexpected( format( "{}: start_level: no such level in the project", cProjectFile ) );
    return mOperators.TryRun( mWorkspace, "document.open",
                              Json( Json::Object{ Json::Member{ .mKey = "path", .mValue = pathMaybe->String() } } ) );
}

string_view EditorProject::KindOf( const AssetPath& path ) const
{
    const string_view text = path.View();
    if ( text.ends_with( ".scene" ) )
        return "level";
    const size_t dot = text.rfind( '.' );
    if ( dot == string_view::npos )
        return {};
    const auto importerMaybe = mProject.GetImporters().TryFind( text.substr( dot ) );
    return importerMaybe ? string_view( importerMaybe->mKind ) : string_view();
}

bool EditorProject::Makes( const AssetPath& path, string_view kind ) const
{
    const string_view text = path.View();
    const size_t dot = text.rfind( '.' );
    if ( dot == string_view::npos )
        return false;
    const auto importerMaybe = mProject.GetImporters().TryFind( text.substr( dot ) );
    return importerMaybe and ( importerMaybe->mKind == kind or
                               ranges::find( importerMaybe->mMoreKinds, kind ) != importerMaybe->mMoreKinds.end() );
}

bool EditorProject::Run( string_view id, const Value::Record& args )
{
    if ( auto ranMaybe = mOperators.TryRun( mWorkspace, id, args ); not ranMaybe )
    {
        LogError( "{}", ranMaybe.error() );
        return false;
    }
    return true;
}

bool EditorProject::Run( string_view id, const Json& args )
{
    if ( auto ranMaybe = mOperators.TryRun( mWorkspace, id, args ); not ranMaybe )
    {
        LogError( "{}", ranMaybe.error() );
        return false;
    }
    return true;
}
}
