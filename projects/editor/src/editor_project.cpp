#include "bubble/editor/editor_project.hpp"
#include "bubble/animation/animation_assets.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/editor/play_session.hpp"
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
    project->mSettingsText = *textMaybe;
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
                                           // A game played in the editor saves into
                                           // memory, never over the player's slots.
                                           .mSaves = SaveSlotsInMemory(),
                                           .mGameVersion = game } );

    auto checkMaybe = TryScriptCheck();
    if ( not checkMaybe )
        return unexpected( checkMaybe.error() );
    AddEngineImporters( mProject, mApp, std::move( *checkMaybe ) );
    RegisterEngineLoaders( mAssets );
    AddBuiltinOperators( mOperators );
    AddPlayOperators();

    const auto refreshMaybe = mProject.TryRefresh();
    if ( not refreshMaybe )
        return unexpected( refreshMaybe.error() );
    Report( *refreshMaybe );
    return {};
}

expected<ScriptCheck, string> EditorProject::TryScriptCheck()
{
    // Scripts are checked against the engine's declarations with the
    // project's actions, which go under .bubble too, for a language server.
    auto checkMaybe = TryProjectScriptCheck( mApp, mProject.GetFiles(), mProject.GetCatalog(),
                                             ScriptLabels{ .mActions = ActionNames( mSettings.mActions ) } );
    if ( not checkMaybe )
        return unexpected( checkMaybe.error() );
    const AssetPath declarationsPath = *AssetPath::TryFrom( ".bubble/bubble.d.luau" );
    if ( mProject.GetFiles().TryRead( declarationsPath ).value_or( string() ) != checkMaybe->mDeclarations )
        if ( auto writtenMaybe = mProject.GetFiles().TryWrite( declarationsPath, checkMaybe->mDeclarations );
             not writtenMaybe )
            LogWarning( "{}: {}", declarationsPath.View(), writtenMaybe.error() );
    return checkMaybe;
}

void EditorProject::FollowSettings()
{
    const auto textMaybe = mProject.GetFiles().TryRead( *AssetPath::TryFrom( cProjectFile ) );
    if ( not textMaybe or *textMaybe == mSettingsText )
        return;
    // Kept even when it does not read: the error is logged once, not at
    // every look.
    mSettingsText = *textMaybe;
    auto settingsMaybe = TryReadProjectSettings( *textMaybe );
    if ( not settingsMaybe )
    {
        LogError( "{}:{}", cProjectFile, settingsMaybe.error() );
        return;
    }
    const bool othersActions = ActionNames( settingsMaybe->mActions ) != ActionNames( mSettings.mActions );
    mSettings = std::move( *settingsMaybe );
    if ( not othersActions )
        return;
    auto checkMaybe = TryScriptCheck();
    if ( not checkMaybe )
    {
        LogError( "{}", checkMaybe.error() );
        return;
    }
    mProject.ReplaceImporter( ScriptImporter( std::move( *checkMaybe ) ) );
}

void EditorProject::Refresh()
{
    FollowSettings();
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

vector<string> EditorProject::ClipNames() const
{
    vector<string> names;
    for ( const AssetPath& path : GetCatalog().Paths() )
    {
        if ( not Makes( path, cClipsKind ) )
            continue;
        const auto idMaybe = GetCatalog().TryIdOf( path );
        if ( not idMaybe )
            continue;
        if ( const auto bytesMaybe = GetFiles().TryRead( ImportedPath( *idMaybe, cClipsKind ) ) )
            for ( string& name : bubble::ClipNames( *bytesMaybe ) )
                if ( not ranges::contains( names, name ) )
                    names.push_back( std::move( name ) );
    }
    return names;
}

OptRef<PlaySession> EditorProject::TryPlaying()
{
    return mPlay ? OptRef<PlaySession>( *mPlay ) : nullopt;
}

void EditorProject::AddPlayOperators()
{
    using Result = expected<void, string>;
    const auto playing = [this]( const Workspace&, const OperatorArgs& ) -> Result {
        if ( not mPlay )
            return unexpected( "no game is playing: play.start begins one"s );
        return {};
    };
    mOperators.Add( Operator{ .mId = "play.start",
                              .mLabel = "Play",
                              .mArgs = {},
                              .mEdits = false,
                              .mTryPoll = [this]( const Workspace& workspace, const OperatorArgs& ) -> Result {
                                  if ( mPlay )
                                      return unexpected( "a game is playing already: play.stop ends it"s );
                                  if ( not workspace.TryShownScene() )
                                      return unexpected( "no level or prefab is open to play"s );
                                  return {};
                              },
                              .mTryRun = [this]( Workspace& workspace, const OperatorArgs& ) -> Result {
                                  mPlay = CreateScope<PlaySession>( *this, *workspace.TryShownScene() );
                                  return {};
                              } } );
    mOperators.Add( Operator{ .mId = "play.stop",
                              .mLabel = "Stop",
                              .mArgs = {},
                              .mEdits = false,
                              .mTryPoll = playing,
                              .mTryRun = [this]( Workspace&, const OperatorArgs& ) -> Result {
                                  mPlay.reset();
                                  return {};
                              } } );
    // Paused or going on as `paused` says; left out, the other way round.
    mOperators.Add( Operator{
        .mId = "play.pause",
        .mLabel = "Pause",
        .mArgs = { OperatorArg{ .mName = *Name::TryFrom( "paused" ), .mType = ValueType::Of( ValueKind::Bool ) } },
        .mEdits = false,
        .mTryPoll = playing,
        .mTryRun = [this]( Workspace&, const OperatorArgs& args ) -> Result {
            World& world = mPlay->GetWorld();
            const auto pausedMaybe = args.TryGet<bool>( *Name::TryFrom( "paused" ) );
            world.SetPaused( pausedMaybe ? *pausedMaybe : not world.Paused() );
            return {};
        } } );
    mOperators.Add( Operator{ .mId = "play.step",
                              .mLabel = "Step",
                              .mArgs = {},
                              .mEdits = false,
                              .mTryPoll = [this]( const Workspace&, const OperatorArgs& ) -> Result {
                                  if ( not mPlay or not mPlay->GetWorld().Paused() )
                                      return unexpected( "a game steps one step at a time while it is paused"s );
                                  return {};
                              },
                              .mTryRun = [this]( Workspace&, const OperatorArgs& ) -> Result {
                                  mPlay->GetWorld().Step();
                                  return {};
                              } } );
}

OptRef<const Importer> EditorProject::TryImporterOf( const AssetPath& path ) const
{
    const string_view text = path.View();
    const size_t dot = text.rfind( '.' );
    if ( dot == string_view::npos )
        return nullopt;
    return mProject.GetImporters().TryFind( text.substr( dot ) );
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
