#include "bubble/editor/assets_panel.hpp"
#include "bubble/animation/animation_graph.hpp"
#include "bubble/assets/font_asset.hpp"
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/editor/editor_icons.hpp"
#include "bubble/editor/editor_style.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace bubble
{
namespace
{
// A file that would not import.
constexpr ImVec4 cBroken = EditorColor::cError;
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

// `text` cut to fit `width`, "..." at its end when it was: whole letters,
// never half of one's bytes.
string Fitted( const string& text, f32 width )
{
    if ( ImGui::CalcTextSize( text.c_str() ).x <= width )
        return text;
    string cut = text;
    while ( not cut.empty() )
    {
        // Back to the start of the last letter.
        size_t end = cut.size() - 1;
        while ( end > 0 and ( static_cast<u8>( cut[end] ) & 0xC0 ) == 0x80 )
            --end;
        cut.resize( end );
        if ( ImGui::CalcTextSize( ( cut + "..." ).c_str() ).x <= width )
            return cut + "...";
    }
    return "...";
}

// An icon of the icon font at `size`, in the middle of `low` to `high`.
void DrawIcon( ImDrawList& draw, string_view icon, f32 size, ImVec2 low, ImVec2 high, ImU32 color )
{
    ImFont* font = ImGui::GetFont();
    const char* begin = icon.data();
    const char* end = icon.data() + icon.size();
    const ImVec2 measured = font->CalcTextSizeA( size, FLT_MAX, 0.0f, begin, end );
    draw.AddText( font, size, ImVec2( ( low.x + high.x - measured.x ) * 0.5f, ( low.y + high.y - measured.y ) * 0.5f ),
                  color, begin, end );
}

// What document.open takes: a scene, or a material's or a graph's data.
bool IsDocument( string_view path )
{
    return path.ends_with( ".scene" ) or path.ends_with( ".prefab" ) or path.ends_with( ".mat" ) or
           path.ends_with( ".anim" );
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

string_view FileIcon( string_view path, string_view kind )
{
    if ( path.ends_with( ".scene" ) )
        return Icon::cLevel;
    if ( path.ends_with( ".prefab" ) or kind == cPrefabKind )
        return Icon::cPrefab;
    if ( path.ends_with( ".mat" ) or kind == cMaterialKind )
        return Icon::cMaterial;
    if ( path.ends_with( ".anim" ) or kind == cAnimationGraphKind )
        return Icon::cGraph;
    if ( kind == cTextureKind )
        return Icon::cImage;
    if ( kind == cModelKind )
        return Icon::cBox;
    if ( kind == cSoundKind )
        return Icon::cSound;
    if ( kind == cFontKind )
        return Icon::cFont;
    if ( kind == cScriptKind or path.ends_with( ".luau" ) )
        return Icon::cScript;
    if ( kind == cShaderKind or path.ends_with( ".wgsl" ) )
        return Icon::cShader;
    return Icon::cFile;
}

opt<ImVec4> KindColor( string_view path, string_view kind )
{
    if ( path.ends_with( ".scene" ) )
        return EditorColor::cKindLevel;
    if ( path.ends_with( ".prefab" ) or kind == cPrefabKind )
        return EditorColor::cKindPrefab;
    if ( path.ends_with( ".mat" ) or kind == cMaterialKind )
        return EditorColor::cKindMaterial;
    if ( path.ends_with( ".anim" ) or kind == cAnimationGraphKind )
        return EditorColor::cKindGraph;
    if ( kind == cTextureKind )
        return EditorColor::cKindTexture;
    if ( kind == cModelKind )
        return EditorColor::cKindModel;
    if ( kind == cSoundKind )
        return EditorColor::cKindSound;
    if ( kind == cFontKind )
        return EditorColor::cKindFont;
    if ( kind == cScriptKind or path.ends_with( ".luau" ) )
        return EditorColor::cKindScript;
    if ( kind == cShaderKind or path.ends_with( ".wgsl" ) )
        return EditorColor::cKindShader;
    return nullopt;
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
    // Folders made here with nothing in them yet: the panel's, until a file
    // put in one makes it the disk's.
    for ( const string& made : mMadeFolders )
        for ( string folder = made; not folder.empty(); folder = FolderOf( folder ) )
            mFolders.push_back( folder );
    ranges::sort( mFolders );
    mFolders.erase( ranges::unique( mFolders ).begin(), mFolders.end() );
    if ( ranges::find( mFolders, mFolder ) == mFolders.end() )
        mFolder.clear();
}

void AssetsPanel::SetTileSize( f32 size )
{
    mTileSize = clamp( size, 48.0f, 192.0f );
}

opt<vec2> AssetsPanel::TryWhere( string_view path ) const
{
    const auto found = mWhere.find( string( path ) );
    return found != mWhere.end() ? opt<vec2>( found->second ) : nullopt;
}

bool AssetsPanel::MakeFolder( string_view name )
{
    if ( name.empty() or name.find( '/' ) != string_view::npos or name.starts_with( '.' ) )
        return false;
    const string folder = Joined( mFolder, name );
    if ( not ranges::contains( mMadeFolders, folder ) )
        mMadeFolders.push_back( folder );
    mFolder = folder;
    mListedAt = -1;
    return true;
}

opt<string> AssetsPanel::TakePicked()
{
    return exchange( mPickedMaybe, nullopt );
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

void AssetsPanel::Header()
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled( "%s", mFolder.empty() ? "project" : mFolder.c_str() );
    // Tiles or rows, and the tiles' size, at the right.
    const f32 button = ImGui::GetFrameHeight();
    const f32 slider = mView == AssetsView::Tiles ? ImGui::GetFontSize() * 6.0f : 0.0f;
    const ImGuiStyle& style = ImGui::GetStyle();
    const f32 right = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
    ImGui::SameLine( max( ImGui::GetCursorPosX(), right - button * 2 - style.ItemSpacing.x * 2 - slider ) );
    if ( mView == AssetsView::Tiles )
    {
        ImGui::SetNextItemWidth( slider );
        f32 size = mTileSize;
        if ( ImGui::SliderFloat( "##size", &size, 48.0f, 192.0f, "" ) )
            SetTileSize( size );
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "The tiles' size" );
        ImGui::SameLine();
    }
    const auto toggle = [&]( const char* icon, AssetsView view, const char* tip ) {
        const bool on = mView == view;
        if ( on )
            ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
        if ( ImGui::Button( icon, ImVec2( button, button ) ) )
            mView = view;
        if ( on )
            ImGui::PopStyleColor();
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "%s", tip );
    };
    toggle( Icon::cTiles, AssetsView::Tiles, "Tiles" );
    ImGui::SameLine();
    toggle( Icon::cList, AssetsView::List, "List" );
}

void AssetsPanel::Files()
{
    Header();
    if ( mView == AssetsView::Tiles )
    {
        if ( ImGui::BeginChild( "tiles" ) )
        {
            Tiles();
            BackgroundMenu();
        }
        ImGui::EndChild();
        return;
    }
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
                const ImVec2 low = ImGui::GetItemRectMin();
                const ImVec2 high = ImGui::GetItemRectMax();
                mWhere[folder + "/"] = vec2( low.x + ImGui::GetFontSize() * 2, ( low.y + high.y ) * 0.5f );
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

void AssetsPanel::Tiles()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const f32 width = mTileSize;
    const auto columns = max( 1, static_cast<int>( ( ImGui::GetContentRegionAvail().x + style.ItemSpacing.x ) /
                                                   ( width + style.ItemSpacing.x ) ) );
    int placed = 0;
    // A row of tiles as wide as fits, then the next.
    const auto next = [&] {
        if ( placed % columns != 0 )
            ImGui::SameLine();
        ++placed;
    };
    for ( const string& folder : mFolders )
        if ( not folder.empty() and FolderOf( folder ) == mFolder )
        {
            next();
            FolderTile( folder );
        }
    for ( const string& path : mFiles )
        if ( FolderOf( path ) == mFolder )
        {
            next();
            FileTile( path );
        }
}

void AssetsPanel::FolderTile( const string& folder )
{
    ImGui::PushID( ( folder + "/" ).c_str() );
    const ImVec2 low = ImGui::GetCursorScreenPos();
    const ImVec2 size( mTileSize, mTileSize + ImGui::GetTextLineHeightWithSpacing() );
    ImGui::Selectable( "##folder", false, ImGuiSelectableFlags_AllowDoubleClick, size );
    const ImVec2 high( low.x + size.x, low.y + size.y );
    mWhere[folder + "/"] = vec2( ( low.x + high.x ) * 0.5f, ( low.y + high.y ) * 0.5f );
    if ( ImGui::IsItemHovered() and ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
        mFolder = folder;
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "%s/", folder.c_str() );
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
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const f32 inset = mTileSize * 0.1f;
    DrawIcon( draw, Icon::cFolder, mTileSize * 0.5f, ImVec2( low.x + inset, low.y + inset ),
              ImVec2( high.x - inset, low.y + mTileSize - inset ), ImGui::GetColorU32( EditorColor::cTextDim ) );
    const string name = Fitted( NameOf( folder ), mTileSize );
    draw.AddText( ImVec2( low.x + ( mTileSize - ImGui::CalcTextSize( name.c_str() ).x ) * 0.5f, low.y + mTileSize ),
                  ImGui::GetColorU32( ImGuiCol_Text ), name.c_str() );
    ImGui::PopID();
}

void AssetsPanel::FileTile( const string& path )
{
    const AssetPath assetPath = *AssetPath::TryFrom( path );
    ImGui::PushID( path.c_str() );
    const ImVec2 low = ImGui::GetCursorScreenPos();
    const f32 line = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 size( mTileSize, mTileSize + line );
    const ImVec2 high( low.x + size.x, low.y + size.y );
    const auto errorMaybe = mProject.TryErrorOf( assetPath );
    if ( ImGui::Selectable( "##tile", mSelected == path,
                            ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, size ) )
    {
        mSelected = path;
        mPickedMaybe = path;
    }
    mWhere[path] = vec2( ( low.x + high.x ) * 0.5f, ( low.y + high.y ) * 0.5f );
    FileActions( path, errorMaybe );

    // The picture: a texture as it is, in its own proportions, or the
    // kind's icon.
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const f32 inset = mTileSize * 0.08f;
    const ImVec2 pictureLow( low.x + inset, low.y + inset );
    const ImVec2 pictureHigh( high.x - inset, low.y + mTileSize - inset );
    const auto pictureMaybe = mThumbnail ? mThumbnail( assetPath ) : nullopt;
    if ( pictureMaybe )
    {
        const vec2 room( pictureHigh.x - pictureLow.x, pictureHigh.y - pictureLow.y );
        const vec2 picture = max( pictureMaybe->mSize, vec2( 1.0f ) );
        const f32 scale = min( room.x / picture.x, room.y / picture.y );
        const vec2 shown = picture * scale;
        const ImVec2 at( pictureLow.x + ( room.x - shown.x ) * 0.5f, pictureLow.y + ( room.y - shown.y ) * 0.5f );
        draw.AddImage( ImTextureRef( pictureMaybe->mId ), at, ImVec2( at.x + shown.x, at.y + shown.y ) );
    }
    else
        DrawIcon( draw, FileIcon( path, mProject.KindOf( assetPath ) ), mTileSize * 0.45f, pictureLow, pictureHigh,
                  ImGui::GetColorU32( errorMaybe ? cBroken : EditorColor::cTextDim ) );
    // Its kind's stripe under the picture, as Unreal marks an asset.
    if ( const auto colorMaybe = KindColor( path, mProject.KindOf( assetPath ) ) )
    {
        const f32 thick = max( 3.0f, mTileSize * 0.045f );
        draw.AddRectFilled( ImVec2( pictureLow.x, pictureHigh.y + 1.0f ),
                            ImVec2( pictureHigh.x, pictureHigh.y + 1.0f + thick ), ImGui::GetColorU32( *colorMaybe ),
                            thick * 0.5f );
    }

    // The name under it, or the field it is renamed in.
    if ( mRenaming == path )
    {
        ImGui::SetCursorScreenPos( ImVec2( low.x, low.y + mTileSize ) );
        ImGui::PushItemWidth( mTileSize );
        Rename( path );
        ImGui::PopItemWidth();
        ImGui::SetCursorScreenPos( low );
        ImGui::Dummy( size );
    }
    else
    {
        const string name = Fitted( NameOf( path ), mTileSize );
        draw.AddText( ImVec2( low.x + ( mTileSize - ImGui::CalcTextSize( name.c_str() ).x ) * 0.5f, low.y + mTileSize ),
                      ImGui::GetColorU32( errorMaybe ? cBroken : ImGui::GetStyleColorVec4( ImGuiCol_Text ) ),
                      name.c_str() );
    }
    ImGui::PopID();
}

void AssetsPanel::FileActions( const string& path, OptRef<const string> errorMaybe )
{
    if ( ImGui::IsItemHovered() and ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
        Open( path );
    if ( ImGui::IsItemHovered() )
    {
        if ( errorMaybe )
            ImGui::SetTooltip( "%s:%s", path.c_str(), errorMaybe->c_str() );
        else if ( mView == AssetsView::Tiles )
            ImGui::SetTooltip( "%s\n%s", NameOf( path ).c_str(),
                               string( mProject.KindOf( *AssetPath::TryFrom( path ) ) ).c_str() );
    }
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
        // A texture's picture, small, before its name.
        // Its kind's colour, a small square first: every row has its place,
        // so the names line up.
        {
            const f32 line = ImGui::GetTextLineHeight();
            const f32 mark = line * 0.45f;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            if ( const auto colorMaybe = KindColor( path, mProject.KindOf( assetPath ) ) )
                ImGui::GetWindowDrawList()->AddRectFilled( ImVec2( at.x, at.y + ( line - mark ) * 0.5f ),
                                                           ImVec2( at.x + mark, at.y + ( line + mark ) * 0.5f ),
                                                           ImGui::GetColorU32( *colorMaybe ), 1.0f );
            ImGui::Dummy( ImVec2( mark, line ) );
            ImGui::SameLine();
        }
        // Its picture, small, before its name; a texture's place kept while
        // it loads.
        if ( mThumbnail )
        {
            const f32 side = ImGui::GetTextLineHeight();
            if ( const auto pictureMaybe = mThumbnail( assetPath ) )
            {
                ImGui::Image( ImTextureRef( pictureMaybe->mId ), ImVec2( side, side ) );
                ImGui::SameLine();
            }
            else if ( mProject.Makes( assetPath, cTextureKind ) )
            {
                ImGui::Dummy( ImVec2( side, side ) );
                ImGui::SameLine();
            }
        }
        if ( errorMaybe )
            ImGui::PushStyleColor( ImGuiCol_Text, cBroken );
        if ( ImGui::Selectable( NameOf( path ).c_str(), mSelected == path,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) )
        {
            mSelected = path;
            mPickedMaybe = path;
        }
        if ( errorMaybe )
            ImGui::PopStyleColor();
        const ImVec2 low = ImGui::GetItemRectMin();
        const ImVec2 high = ImGui::GetItemRectMax();
        mWhere[path] = vec2( low.x + ImGui::GetFontSize() * 2, ( low.y + high.y ) * 0.5f );
        FileActions( path, errorMaybe );
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
    ImGui::Separator();
    // A folder by the name typed, in the one shown.
    if ( ImGui::BeginMenu( "New Folder" ) )
    {
        if ( ImGui::IsWindowAppearing() )
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth( ImGui::GetFontSize() * 10.0f );
        if ( ImGui::InputTextWithHint( "##folder", "its name", &mNewFolder, ImGuiInputTextFlags_EnterReturnsTrue ) and
             MakeFolder( mNewFolder ) )
        {
            mNewFolder.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
    }
    if ( const auto rootMaybe = mProject.TryRoot() )
    {
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
    if ( mView == AssetsView::List )
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
