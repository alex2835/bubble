#include "bubble/editor/assets_panel.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace bubble
{
namespace
{
// A file that would not import.
constexpr ImVec4 cBroken{ 1.0f, 0.45f, 0.4f, 1.0f };
// How often the project's files are listed again, in seconds, besides after
// the panel's own changes.
constexpr f64 cListEvery = 1.0;

string FolderOf( string_view path )
{
    const size_t slash = path.rfind( '/' );
    return slash == string_view::npos ? string() : string( path.substr( 0, slash ) );
}

string NameOf( string_view path )
{
    return string( path.substr( path.rfind( '/' ) + 1 ) );
}

string Joined( const string& folder, string_view name )
{
    return folder.empty() ? string( name ) : format( "{}/{}", folder, name );
}

Json Args( vector<Json::Member> members )
{
    return Json( Json::Object( std::move( members ) ) );
}

Json::Member Member( string key, string value )
{
    return Json::Member{ .mKey = std::move( key ), .mValue = Json( std::move( value ) ) };
}

bool IsDocument( string_view path )
{
    return path.ends_with( ".scene" ) or path.ends_with( ".prefab" );
}
}

expected<void, string> TryPlaceAsset( EditorProject& project,
                                      const AssetPath& path,
                                      const EntityPath& parent,
                                      opt<vec3> position )
{
    const auto documentMaybe = project.GetWorkspace().TryActive();
    if ( not documentMaybe )
        return unexpected( "no document is open"s );
    Document& document = *documentMaybe;
    const bool prefab = path.View().ends_with( ".prefab" );
    const bool model = project.Makes( path, cModelKind );
    if ( not prefab and not model )
        return unexpected( format( "{}: a prefab or a model goes into a scene, not this", path.View() ) );
    const auto run = [&]( string_view op, Value::Record args ) {
        return project.GetOperators().TryRun( project.GetWorkspace(), op, args );
    };
    const auto field = []( string_view name, Value value ) {
        return ValueField{ .mName = *Name::TryFrom( name ), .mValue = std::move( value ) };
    };
    const Text stem = *Text::TryFrom( path.Stem() );

    History& history = document.GetHistory();
    history.BeginGroup( format( "Place {}", path.Stem() ) );
    expected<void, string> placed;
    if ( prefab )
        placed = run( "prefab.instantiate", Value::Record{ field( "prefab", Value( *Text::TryFrom( path.View() ) ) ),
                                                           field( "parent", Value( parent ) ) } );
    else if ( const auto modelIdMaybe = project.GetCatalog().TryIdOf( path ); not modelIdMaybe )
        placed = unexpected( format( "{}: not in the project yet", path.View() ) );
    else
    {
        // The entity made is the selection, which what follows acts on.
        placed =
            run( "entity.create", Value::Record{ field( "parent", Value( parent ) ), field( "name", Value( stem ) ) } );
        if ( placed )
            placed = run( "component.add", Value::Record{ field( "component", Value( "transform"_n ) ) } );
        if ( placed )
            placed = run( "component.add", Value::Record{ field( "component", Value( "model"_n ) ) } );
        if ( placed )
            placed = run( "property.set", Value::Record{ field( "component", Value( "model"_n ) ),
                                                         field( "path", Value( *Text::TryFrom( "asset" ) ) ),
                                                         field( "value", Value( *modelIdMaybe ) ) } );
    }
    const Entity placedEntity = document.GetSelection().Active();
    if ( placed and position and document.GetScene().Has<Transform>( placedEntity ) )
        placed = run( "property.set", Value::Record{ field( "component", Value( "transform"_n ) ),
                                                     field( "path", Value( *Text::TryFrom( "position" ) ) ),
                                                     field( "value", Value( *position ) ) } );
    if ( not placed )
    {
        history.CancelGroup();
        return unexpected( placed.error() );
    }
    history.EndGroup();
    return {};
}

AssetsPanel::AssetsPanel( EditorProject& project, function<void( const OsPath& )> open )
    : mProject( project ),
      mOpen( std::move( open ) )
{
}

bool AssetsPanel::Run( string_view op, const Json& args )
{
    const bool ran = mProject.Run( op, args );
    // What it changed is listed at once.
    mListedAt = -1;
    return ran;
}

void AssetsPanel::List()
{
    mListedAt = ImGui::GetTime();
    const auto listedMaybe = mProject.GetFiles().TryList();
    if ( not listedMaybe )
        return;
    mFiles.clear();
    mFolders.assign( 1, string() );
    for ( const ListedFile& file : *listedMaybe )
    {
        const string_view path = file.mPath.View();
        if ( path.ends_with( ".import" ) )
            continue;
        mFiles.emplace_back( path );
        for ( string folder = FolderOf( path ); not folder.empty(); folder = FolderOf( folder ) )
            mFolders.push_back( folder );
    }
    ranges::sort( mFolders );
    mFolders.erase( ranges::unique( mFolders ).begin(), mFolders.end() );
    if ( ranges::find( mFolders, mFolder ) == mFolders.end() )
        mFolder.clear();
}

opt<vec2> AssetsPanel::TryWhere( string_view path ) const
{
    const auto found = mWhere.find( string( path ) );
    return found != mWhere.end() ? opt<vec2>( found->second ) : nullopt;
}

void AssetsPanel::Show()
{
    if ( mListedAt < 0 or ImGui::GetTime() - mListedAt > cListEvery )
        List();
    mWhere.clear();
    // F2 renames the file picked, while the panel has the focus and no field
    // takes the keys.
    if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) and not ImGui::GetIO().WantTextInput and
         mRenaming.empty() and not mSelected.empty() and ImGui::IsKeyChordPressed( ImGuiKey_F2 ) )
    {
        mRenaming = mSelected;
        mNewName = NameOf( mSelected );
        mRenameStarted = true;
    }
    if ( ImGui::BeginTable( "assets", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) )
    {
        ImGui::TableSetupColumn( "folders", ImGuiTableColumnFlags_WidthStretch, 0.3f );
        ImGui::TableSetupColumn( "files", ImGuiTableColumnFlags_WidthStretch, 0.7f );
        ImGui::TableNextColumn();
        if ( ImGui::BeginChild( "folders" ) )
            FolderRow( "" );
        ImGui::EndChild();
        ImGui::TableNextColumn();
        if ( ImGui::BeginChild( "files" ) )
            Files();
        ImGui::EndChild();
        ImGui::EndTable();
    }
    DeleteDialog();
}

void AssetsPanel::FolderRow( const string& folder )
{
    vector<string> children;
    for ( const string& other : mFolders )
        if ( not other.empty() and FolderOf( other ) == folder )
            children.push_back( other );
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if ( children.empty() )
        flags |= ImGuiTreeNodeFlags_Leaf;
    if ( folder.empty() )
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if ( folder == mFolder )
        flags |= ImGuiTreeNodeFlags_Selected;
    const string label = folder.empty() ? "project" : NameOf( folder );
    const bool open = ImGui::TreeNodeEx( ( label + "##" + folder ).c_str(), flags );
    if ( ImGui::IsItemClicked() and not ImGui::IsItemToggledOpen() )
        mFolder = folder;
    // A file dropped on a folder moves there.
    if ( ImGui::BeginDragDropTarget() )
    {
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( cAssetPayload ) )
        {
            const string path( static_cast<const char*>( payload->Data ), static_cast<size_t>( payload->DataSize ) );
            if ( FolderOf( path ) != folder )
                Run( "asset.rename",
                     Args( { Member( "path", path ), Member( "to", Joined( folder, NameOf( path ) ) ) } ) );
        }
        ImGui::EndDragDropTarget();
    }
    if ( open )
    {
        for ( const string& child : children )
            FolderRow( child );
        ImGui::TreePop();
    }
}

void AssetsPanel::Files()
{
    ImGui::TextDisabled( "%s", mFolder.empty() ? "project" : mFolder.c_str() );
    if ( ImGui::BeginTable( "list", 2, ImGuiTableFlags_RowBg ) )
    {
        ImGui::TableSetupColumn( "name", ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( "kind", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7 );
        // Folders first: a double click goes into one.
        for ( const string& folder : mFolders )
            if ( not folder.empty() and FolderOf( folder ) == mFolder )
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Selectable( ( NameOf( folder ) + "/##" + folder ).c_str(), false,
                                   ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick );
                if ( ImGui::IsItemHovered() and ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
                    mFolder = folder;
                ImGui::TableNextColumn();
                ImGui::TextDisabled( "folder" );
            }
        for ( const string& path : mFiles )
            if ( FolderOf( path ) == mFolder )
                FileRow( path );
        ImGui::EndTable();
    }
    BackgroundMenu();
}

void AssetsPanel::FileRow( const string& path )
{
    const AssetPath assetPath = *AssetPath::TryFrom( path );
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID( path.c_str() );
    if ( mRenaming == path )
        Rename( path );
    else
    {
        const auto errorMaybe = mProject.TryErrorOf( assetPath );
        if ( errorMaybe )
            ImGui::PushStyleColor( ImGuiCol_Text, cBroken );
        if ( ImGui::Selectable( NameOf( path ).c_str(), mSelected == path,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) )
            mSelected = path;
        if ( errorMaybe )
            ImGui::PopStyleColor();
        const ImVec2 low = ImGui::GetItemRectMin();
        const ImVec2 high = ImGui::GetItemRectMax();
        mWhere[path] = vec2( low.x + ImGui::GetFontSize() * 2, ( low.y + high.y ) * 0.5f );
        if ( ImGui::IsItemHovered() and ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
            Open( path );
        if ( errorMaybe and ImGui::IsItemHovered() )
            ImGui::SetTooltip( "%s:%s", path.c_str(), errorMaybe->c_str() );
        if ( ImGui::BeginDragDropSource() )
        {
            ImGui::SetDragDropPayload( cAssetPayload, path.data(), path.size() );
            ImGui::TextUnformatted( path.c_str() );
            ImGui::EndDragDropSource();
        }
        if ( ImGui::BeginPopupContextItem( "file" ) )
        {
            FileMenu( path );
            ImGui::EndPopup();
        }
    }
    ImGui::TableNextColumn();
    ImGui::TextDisabled( "%s", string( mProject.KindOf( assetPath ) ).c_str() );
    ImGui::PopID();
}

void AssetsPanel::FileMenu( const string& path )
{
    if ( ImGui::MenuItem( "Open" ) )
        Open( path );
    if ( ImGui::MenuItem( "Rename", "F2" ) )
    {
        mRenaming = path;
        mNewName = NameOf( path );
        mRenameStarted = true;
    }
    if ( ImGui::MenuItem( "Delete" ) )
        mDeleting = path;
    if ( const auto rootMaybe = mProject.TryRoot(); rootMaybe and ImGui::MenuItem( "Show in Folder" ) )
        mOpen( *rootMaybe / PathFromUtf8( FolderOf( path ) ) );
}

void AssetsPanel::BackgroundMenu()
{
    if ( not ImGui::BeginPopupContextWindow( "new",
                                             ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems ) )
        return;
    for ( const auto& [label, stem, extension] :
          { tuple<const char*, string_view, string_view>{ "New Material", "material", ".mat" },
            tuple<const char*, string_view, string_view>{ "New Prefab", "prefab", ".prefab" },
            tuple<const char*, string_view, string_view>{ "New Level", "level", ".scene" },
            tuple<const char*, string_view, string_view>{ "New Animation Graph", "animation", ".anim" },
            tuple<const char*, string_view, string_view>{ "New Script", "script", ".luau" } } )
        if ( ImGui::MenuItem( label ) )
            New( stem, extension );
    if ( const auto rootMaybe = mProject.TryRoot() )
    {
        ImGui::Separator();
        if ( ImGui::MenuItem( "Show in Folder" ) )
            mOpen( *rootMaybe / PathFromUtf8( mFolder ) );
    }
    ImGui::EndPopup();
}

void AssetsPanel::New( string_view stem, string_view extension )
{
    // new_material.mat, then new_material_2.mat, ...
    string path = Joined( mFolder, format( "new_{}{}", stem, extension ) );
    for ( int number = 2; ranges::find( mFiles, path ) != mFiles.end(); ++number )
        path = Joined( mFolder, format( "new_{}_{}{}", stem, number, extension ) );
    if ( not Run( "asset.create", Args( { Member( "path", path ) } ) ) )
        return;
    List();
    mSelected = path;
    mRenaming = path;
    mNewName = NameOf( path );
    mRenameStarted = true;
}

void AssetsPanel::Rename( const string& path )
{
    if ( mRenameStarted )
    {
        ImGui::SetKeyboardFocusHere();
        mRenameStarted = false;
    }
    ImGui::SetNextItemWidth( -1.0f );
    const bool entered = ImGui::InputText( "##name", &mNewName,
                                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll );
    if ( ImGui::IsKeyPressed( ImGuiKey_Escape ) )
    {
        mRenaming.clear();
        return;
    }
    if ( entered or ImGui::IsItemDeactivated() )
    {
        const string to = Joined( FolderOf( path ), mNewName );
        mRenaming.clear();
        if ( to != path and not mNewName.empty() and
             Run( "asset.rename", Args( { Member( "path", path ), Member( "to", to ) } ) ) )
            mSelected = to;
    }
}

void AssetsPanel::DeleteDialog()
{
    if ( not mDeleting.empty() and not ImGui::IsPopupOpen( "Delete asset" ) )
        ImGui::OpenPopup( "Delete asset" );
    if ( not ImGui::BeginPopupModal( "Delete asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;
    ImGui::Text( "Delete %s? This cannot be undone.", mDeleting.c_str() );
    if ( ImGui::Button( "Delete" ) )
    {
        Run( "asset.delete", Args( { Member( "path", mDeleting ) } ) );
        mDeleting.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel" ) )
    {
        mDeleting.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void AssetsPanel::Open( const string& path )
{
    if ( IsDocument( path ) )
        Run( "document.open", Args( { Member( "path", path ) } ) );
    else if ( const auto rootMaybe = mProject.TryRoot() )
        mOpen( *rootMaybe / PathFromUtf8( path ) );
}
}
