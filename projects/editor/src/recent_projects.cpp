#include "bubble/editor/recent_projects.hpp"
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"

namespace bubble
{
namespace
{
expected<void, string> TryWrite( const OsPath& file, string_view text )
{
    ofstream out( file, ios::binary | ios::trunc );
    out << text;
    if ( not out )
        return unexpected( format( "{} could not be written", PathToUtf8( file ) ) );
    return {};
}
}

RecentProjects::RecentProjects( opt<OsPath> fileMaybe ) : mFileMaybe( std::move( fileMaybe ) )
{
    if ( not mFileMaybe )
        return;
    ifstream file( *mFileMaybe, ios::binary );
    // None yet: written at the first project opened.
    if ( not file )
        return;
    ostringstream text;
    text << file.rdbuf();
    if ( auto readMaybe = TryParse( text.str() ) )
        mProjects = std::move( *readMaybe );
    else
        LogError( "{}: {}; no recent projects", PathToUtf8( *mFileMaybe ), readMaybe.error() );
}

void RecentProjects::Opened( const OsPath& root, string name, i64 now )
{
    const string path = PathToUtf8( fs::absolute( root ).lexically_normal() );
    Forget( path );
    mProjects.insert( mProjects.begin(), RecentProject{ .mPath = path, .mName = std::move( name ), .mOpened = now } );
    if ( mProjects.size() > cMost )
        mProjects.resize( cMost );
    Save();
}

void RecentProjects::Forget( string_view path )
{
    const auto before = mProjects.size();
    erase_if( mProjects, [path]( const RecentProject& project ) { return project.mPath == path; } );
    if ( mProjects.size() != before )
        Save();
}

expected<vector<RecentProject>, string> RecentProjects::TryParse( string_view text ) const
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( jsonMaybe.error() );
    const auto listMaybe = jsonMaybe->TryFind( "projects" );
    if ( not listMaybe or not listMaybe->TryAs<Json::Array>() )
        return unexpected( "\"projects\": a list expected"s );
    vector<RecentProject> projects;
    for ( const Json& entry : *listMaybe->TryAs<Json::Array>() )
    {
        const auto pathMaybe = entry.TryFind( "path" );
        if ( not pathMaybe or not pathMaybe->TryAs<string>() )
            return unexpected( format( "{}: a project's \"path\" expected", entry.Where() ) );
        RecentProject project{ .mPath = *pathMaybe->TryAs<string>(), .mName = {}, .mOpened = 0 };
        if ( const auto nameMaybe = entry.TryFind( "name" ); nameMaybe and nameMaybe->TryAs<string>() )
            project.mName = *nameMaybe->TryAs<string>();
        if ( const auto openedMaybe = entry.TryFind( "opened" ); openedMaybe and openedMaybe->TryAs<f64>() )
            project.mOpened = static_cast<i64>( *openedMaybe->TryAs<f64>() );
        projects.push_back( std::move( project ) );
    }
    return projects;
}

string RecentProjects::ToJson() const
{
    Json::Array projects;
    for ( const RecentProject& project : mProjects )
        projects.emplace_back(
            Json::Object{ Json::Member{ .mKey = "path", .mValue = Json( project.mPath ) },
                          Json::Member{ .mKey = "name", .mValue = Json( project.mName ) },
                          Json::Member{ .mKey = "opened", .mValue = Json( static_cast<f64>( project.mOpened ) ) } } );
    return Json( Json::Object{ Json::Member{ .mKey = "version", .mValue = Json( 1.0 ) },
                               Json::Member{ .mKey = "projects", .mValue = Json( std::move( projects ) ) } } )
        .Write();
}

void RecentProjects::Save() const
{
    if ( not mFileMaybe )
        return;
    if ( auto writtenMaybe = TryWrite( *mFileMaybe, ToJson() ); not writtenMaybe )
        LogError( "{}", writtenMaybe.error() );
}

expected<OsPath, string> TryCreateProject( const OsPath& parent, string_view name )
{
    if ( name.empty() or name == "." or name == ".." or name.find_first_of( "/\\:*?\"<>|" ) != string_view::npos )
        return unexpected( format( "\"{}\" is no folder's name: letters, digits, spaces, - and _", name ) );
    if ( parent.empty() )
        return unexpected( "no folder to make it in"s );
    const OsPath root = parent / PathFromUtf8( name );
    error_code error;
    if ( fs::exists( root, error ) and not fs::is_empty( root, error ) )
        return unexpected( format( "{} is there already, with something in it", PathToUtf8( root ) ) );
    fs::create_directories( root / "levels", error );
    if ( error )
        return unexpected( format( "{}: {}", PathToUtf8( root ), error.message() ) );

    const Json project( Json::Object{ Json::Member{ .mKey = "version", .mValue = Json( 1.0 ) },
                                      Json::Member{ .mKey = "name", .mValue = Json( string( name ) ) },
                                      Json::Member{ .mKey = "start_level",
                                                    .mValue = Json( Json::Object{ Json::Member{
                                                        .mKey = "path", .mValue = Json( "levels/main.scene"s ) } } ) },
                                      Json::Member{ .mKey = "actions", .mValue = Json( Json::Object{} ) } } );
    // A sun from above and to the side, and a camera looking down at the
    // origin from behind it.
    const string level = format( R"({{
  "version": 1,
  "asset": "{}",
  "root": {{
    "children": [
      {{
        "name": "sun",
        "components": {{
          "transform": {{ "rotation": [-0.9, 0.6, 0] }},
          "directional_light": {{ "color": [1, 0.97, 0.92, 1], "intensity": 2.4, "shadows": true }}
        }}
      }},
      {{
        "name": "camera",
        "components": {{
          "transform": {{ "position": [0, 3, 8], "rotation": [-0.35, 0, 0] }},
          "camera": {{}}
        }}
      }}
    ]
  }}
}}
)",
                                 AssetId::Generate().ToString() );
    for ( const auto& [file, text] :
          { pair<OsPath, string>( root / "project.bubble", project.Write() + "\n" ),
            pair<OsPath, string>( root / "levels" / "main.scene", level ),
            pair<OsPath, string>(
                root / ".gitignore",
                "# Derived: the import cache, the declarations, the editor's layout.\n.bubble/\n" ) } )
        if ( auto writtenMaybe = TryWrite( file, text ); not writtenMaybe )
            return unexpected( writtenMaybe.error() );
    return root;
}
}
