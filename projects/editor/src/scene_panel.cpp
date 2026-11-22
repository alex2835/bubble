#include "bubble/editor/scene_panel.hpp"
#include "bubble/core/log.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/editor_icons.hpp"
#include "bubble/editor/editor_style.hpp"
#include "bubble/editor/panel.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/types/algorithm.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace bubble
{
namespace
{
// A drop in the top or the bottom quarter of a row lands beside it.
constexpr f32 cBeside = 0.25f;

Json::Member PathMember( const Scene& scene, string key, Entity entity )
{
    return Json::Member{ .mKey = std::move( key ), .mValue = Json( PathOf( scene, entity ).mPath.String() ) };
}

Json::Member EntitiesMember( const Scene& scene, span<const Entity> entities )
{
    Json::Array paths;
    for ( const Entity entity : entities )
        paths.push_back( Json( PathOf( scene, entity ).mPath.String() ) );
    return Json::Member{ .mKey = "entities", .mValue = Json( std::move( paths ) ) };
}

Json EntitiesJson( const Scene& scene, span<const Entity> entities )
{
    return Json( Json::Object{ EntitiesMember( scene, entities ) } );
}

Json EntityJson( const Scene& scene, string key, Entity entity )
{
    return Json( Json::Object{ PathMember( scene, std::move( key ), entity ) } );
}

// Instances of prefabs in blue, as the editors people know have them; the
// nodes inside one lighter.
opt<ImVec4> ColorOf( const Scene& scene, Entity entity )
{
    if ( scene.Has<PrefabInstance>( entity ) )
        return EditorColor::cPrefab;
    if ( scene.Has<PrefabLink>( entity ) )
        return EditorColor::cPrefabInside;
    return nullopt;
}
}

OperatorCall ClickCall( const Scene& scene,
                        const Selection& selection,
                        span<const Entity> rows,
                        Entity entity,
                        bool ctrl,
                        bool shift )
{
    if ( ctrl )
        return OperatorCall{ .mId = "selection.toggle", .mArgs = EntitiesJson( scene, vector<Entity>{ entity } ) };
    const Entity anchor = selection.Active();
    const auto from = ranges::find( rows, anchor );
    const auto to = ranges::find( rows, entity );
    if ( shift and anchor != Entity::Null and from != rows.end() and to != rows.end() )
    {
        // From the active one to the one clicked, which is active then.
        vector<Entity> range;
        if ( from <= to )
            range.assign( from, to + 1 );
        else
        {
            range.assign( to, from + 1 );
            ranges::reverse( range );
        }
        return OperatorCall{ .mId = "selection.set", .mArgs = EntitiesJson( scene, range ) };
    }
    return OperatorCall{ .mId = "selection.set", .mArgs = EntitiesJson( scene, vector<Entity>{ entity } ) };
}

opt<OperatorCall> TryDropCall( const Scene& scene, span<const Entity> dragged, Entity target, DropPlace place )
{
    if ( dragged.empty() )
        return nullopt;
    for ( const Entity entity : dragged )
        if ( entity == target or IsAncestor( scene, entity, target ) )
            return nullopt;
    Json::Object members{ EntitiesMember( scene, dragged ) };
    if ( place == DropPlace::Into or target == scene.Root() )
    {
        members.push_back( PathMember( scene, "parent", target ) );
        return OperatorCall{ .mId = "entity.move", .mArgs = Json( std::move( members ) ) };
    }
    // The index counts the children that stay: the dragged ones are taken
    // out first.
    const Entity parent = ParentOf( scene, target );
    size_t index = 0;
    for ( const Entity child : ChildrenOf( scene, parent ) )
    {
        if ( child == target )
            break;
        if ( ranges::find( dragged, child ) == dragged.end() )
            ++index;
    }
    if ( place == DropPlace::After )
        ++index;
    members.push_back( PathMember( scene, "parent", parent ) );
    // Operators count places from 1.
    members.push_back( Json::Member{ .mKey = "index", .mValue = Json( static_cast<f64>( index + 1 ) ) } );
    return OperatorCall{ .mId = "entity.move", .mArgs = Json( std::move( members ) ) };
}

ScenePanel::ScenePanel( EditorProject& project ) : mProject( project )
{
}

void ScenePanel::Run( string_view op, const Json& args )
{
    mPending.push_back( OperatorCall{ .mId = string( op ), .mArgs = args } );
}

void ScenePanel::MenuItem( const char* label, const char* keys, string_view op, const Json& args )
{
    const auto pollMaybe = mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op, args );
    if ( ImGui::MenuItem( label, keys, false, pollMaybe.has_value() ) )
        Run( op, args );
    if ( not pollMaybe and ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        ImGui::SetTooltip( "%s", pollMaybe.error().c_str() );
}

string RootLabel( const Document& document )
{
    const auto pathMaybe = document.TryPath();
    if ( not pathMaybe )
        return "prefab";
    string_view name = pathMaybe->View().substr( pathMaybe->View().rfind( '/' ) + 1 );
    if ( const size_t dot = name.rfind( '.' ); dot != string_view::npos )
        name = name.substr( 0, dot );
    return string( name );
}

void ScenePanel::Show()
{
    Workspace& workspace = mProject.GetWorkspace();
    // The scene shown beside a data document's window: turned to, it is the
    // active document again.
    const auto documentMaybe = workspace.TryShownScene();
    if ( not documentMaybe )
    {
        ImGui::TextDisabled( "No level or prefab is open" );
        return;
    }
    Document& document = *documentMaybe;
    if ( ClickedInWindow() )
        workspace.Activate( document );
    const Scene& scene = document.GetScene();
    if ( ImGui::GetDragDropPayload() == nullptr )
        mDragged.clear();
    // A row renamed may be gone: undone, deleted from elsewhere.
    if ( not scene.Alive( mRenaming ) )
        mRenaming = Entity::Null;
    Keys( document );
    if ( const Entity active = document.GetSelection().Active(); active != mActive )
    {
        mActive = active;
        mReveal = active;
    }

    mShown.clear();
    // A prefab's root is its own node, with components of its own: a row of
    // its own, the others under it. A level's root is the level itself.
    if ( document.TryPrefabNodes() )
        Row( document, scene.Root() );
    else
    {
        const vector<Entity> top( ChildrenOf( scene, scene.Root() ).begin(), ChildrenOf( scene, scene.Root() ).end() );
        for ( const Entity entity : top )
            Row( document, entity );
    }
    mRows = mShown;
    mReveal = Entity::Null;

    // Below them: a click selects nothing, a drop goes to the end of the
    // root, the menu makes and pastes there.
    const ImVec2 rest = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton( "below", ImVec2( max( rest.x, 1.0f ), max( rest.y, ImGui::GetFrameHeight() ) ) );
    if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) )
        Run( "selection.clear" );
    DropOn( document, scene.Root(), false );
    if ( ImGui::BeginPopupContextItem( "below menu" ) )
    {
        MenuItem( "Create Entity", nullptr, "entity.create", EntityJson( scene, "parent", scene.Root() ) );
        MenuItem( "Paste", "Ctrl+V", "edit.paste", EntityJson( scene, "parent", scene.Root() ) );
        ImGui::EndPopup();
    }

    // A press on a selected row that came up with no drag: that row alone.
    if ( mPressed != Entity::Null and ImGui::IsMouseReleased( ImGuiMouseButton_Left ) )
    {
        if ( mDragged.empty() and scene.Alive( mPressed ) )
            Run( "selection.set", EntitiesJson( scene, vector<Entity>{ mPressed } ) );
        mPressed = Entity::Null;
    }

    const vector<OperatorCall> pending = std::move( mPending );
    mPending.clear();
    // What the tree asks is of its own document, whichever is active: a drop
    // from the assets comes with no click here.
    if ( not pending.empty() or mPlaceMaybe )
        workspace.Activate( document );
    for ( const OperatorCall& call : pending )
        if ( not mProject.Run( call.mId, call.mArgs ) )
            break;
    if ( mPlaceMaybe )
    {
        if ( auto placedMaybe = TryPlaceAsset( mProject, mPlaceMaybe->first, mPlaceMaybe->second ); not placedMaybe )
            LogError( "{}", placedMaybe.error() );
        mPlaceMaybe.reset();
    }
}

void ScenePanel::Row( Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    const Selection& selection = document.GetSelection();
    mShown.push_back( entity );
    const vector<Entity> children( ChildrenOf( scene, entity ).begin(), ChildrenOf( scene, entity ).end() );

    const bool renaming = mRenaming == entity;
    const bool root = entity == scene.Root();
    // Renamed, the row is as wide as its arrow, and the field follows.
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if ( root )
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if ( not renaming )
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
    if ( children.empty() )
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if ( selection.Has( entity ) )
        flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::PushID( static_cast<int>( static_cast<u32>( entity ) ) );
    const auto colorMaybe = ColorOf( scene, entity );
    if ( colorMaybe )
        ImGui::PushStyleColor( ImGuiCol_Text, *colorMaybe );
    // An icon of what it is before its name; the root is the document's.
    const string name =
        root ? RootLabel( document ) : format( "{}  {}", IconOf( scene, entity ), NameOf( scene, entity ).String() );
    if ( mReveal != Entity::Null and IsAncestor( scene, entity, mReveal ) )
        ImGui::SetNextItemOpen( true );
    const bool open = renaming ? ImGui::TreeNodeEx( "node", flags, "%s", "" )
                               : ImGui::TreeNodeEx( "node", flags, "%s", name.c_str() );
    if ( colorMaybe )
        ImGui::PopStyleColor();
    if ( entity == mReveal and not ImGui::IsItemVisible() )
        ImGui::SetScrollHereY( 0.5f );

    if ( not renaming and ImGui::IsItemClicked( ImGuiMouseButton_Left ) and not ImGui::IsItemToggledOpen() )
    {
        const ImGuiIO& io = ImGui::GetIO();
        // One of several selected may be the start of dragging them all.
        if ( selection.Has( entity ) and not io.KeyCtrl and not io.KeyShift )
            mPressed = entity;
        else
        {
            const OperatorCall call = ClickCall( scene, selection, mRows, entity, io.KeyCtrl, io.KeyShift );
            Run( call.mId, call.mArgs );
        }
    }
    if ( ImGui::IsItemHovered() and ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
        if ( const auto instanceMaybe = scene.TryGet<PrefabInstance>( entity ) )
            if ( const auto pathMaybe = mProject.GetWorkspace().GetAssets().TryPathOf( instanceMaybe->mPrefab ) )
                Run( "document.open",
                     Json( Json::Object{ Json::Member{ .mKey = "path", .mValue = Json( pathMaybe->String() ) } } ) );

    if ( not renaming and not root and ImGui::BeginDragDropSource() )
    {
        if ( mDragged.empty() )
        {
            mDragged = selection.Has( entity ) ? selection.TopLevel() : vector<Entity>{ entity };
            if ( not selection.Has( entity ) )
                Run( "selection.set", EntitiesJson( scene, mDragged ) );
        }
        mPressed = Entity::Null;
        ImGui::SetDragDropPayload( cEntitiesPayload, mDragged.data(), mDragged.size() * sizeof( Entity ) );
        if ( mDragged.size() == 1 )
            ImGui::TextUnformatted( NameOf( scene, mDragged.front() ).String().c_str() );
        else
            ImGui::Text( "%zu entities", mDragged.size() );
        ImGui::EndDragDropSource();
    }
    DropOn( document, entity, open and not children.empty() );

    if ( ImGui::BeginPopupContextItem( "menu" ) )
    {
        if ( ImGui::IsWindowAppearing() and not selection.Has( entity ) )
            Run( "selection.set", EntitiesJson( scene, vector<Entity>{ entity } ) );
        RowMenu( document, entity );
        ImGui::EndPopup();
    }
    if ( renaming )
    {
        ImGui::SameLine();
        Rename( document, entity );
    }

    if ( open and not children.empty() )
    {
        for ( const Entity child : children )
            Row( document, child );
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void ScenePanel::RowMenu( Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    MenuItem( "Create Child", nullptr, "entity.create", EntityJson( scene, "parent", entity ) );
    // A prefab's root stays where it is, as it is called.
    if ( entity == scene.Root() )
    {
        MenuItem( "Paste", "Ctrl+V", "edit.paste", EntityJson( scene, "parent", entity ) );
        return;
    }
    if ( ImGui::MenuItem( "Rename", "F2" ) )
    {
        mRenaming = entity;
        mNewName = NameOf( scene, entity ).String();
        mRenameStarted = true;
    }
    MenuItem( "Duplicate", "Ctrl+D", "entity.duplicate" );
    MenuItem( "Delete", "Del", "entity.destroy" );
    ImGui::Separator();
    MenuItem( "Copy", "Ctrl+C", "edit.copy" );
    MenuItem( "Cut", "Ctrl+X", "edit.cut" );
    MenuItem( "Paste", "Ctrl+V", "edit.paste" );
    ImGui::Separator();
    MenuItem( "Group", "Ctrl+G", "entity.group" );
    MenuItem( "Ungroup", nullptr, "entity.ungroup", EntityJson( scene, "entity", entity ) );
    if ( const auto instanceMaybe = scene.TryGet<PrefabInstance>( entity ) )
    {
        ImGui::Separator();
        if ( const auto pathMaybe = mProject.GetWorkspace().GetAssets().TryPathOf( instanceMaybe->mPrefab ) )
            MenuItem( "Open Prefab", nullptr, "document.open",
                      Json( Json::Object{ Json::Member{ .mKey = "path", .mValue = Json( pathMaybe->String() ) } } ) );
        MenuItem( "Unpack Prefab", nullptr, "prefab.unpack", EntityJson( scene, "entity", entity ) );
    }
}

void ScenePanel::Rename( Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    if ( mRenameStarted )
    {
        ImGui::SetKeyboardFocusHere();
        mRenameStarted = false;
    }
    ImGui::SetNextItemWidth( -1.0f );
    const bool entered =
        ImGui::InputText( "name", &mNewName, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll );
    if ( ImGui::IsKeyPressed( ImGuiKey_Escape ) )
    {
        mRenaming = Entity::Null;
        return;
    }
    if ( entered or ImGui::IsItemDeactivated() )
    {
        if ( mNewName != NameOf( scene, entity ).View() )
        {
            Run( "entity.rename", Json( Json::Object{ PathMember( scene, "entity", entity ),
                                                      Json::Member{ .mKey = "name", .mValue = Json( mNewName ) } } ) );
        }
        mRenaming = Entity::Null;
    }
}

void ScenePanel::DropOn( Document& document, Entity entity, bool open )
{
    if ( not ImGui::BeginDragDropTarget() )
        return;
    const Scene& scene = document.GetScene();
    if ( const ImGuiPayload* asset = ImGui::AcceptDragDropPayload( cAssetPayload ) )
        if ( auto pathMaybe = AssetPath::TryFrom(
                 string_view( static_cast<const char*>( asset->Data ), static_cast<size_t>( asset->DataSize ) ) ) )
            mPlaceMaybe = pair<AssetPath, EntityPath>( std::move( *pathMaybe ), PathOf( scene, entity ) );
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
        cEntitiesPayload, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect );
    if ( payload != nullptr )
    {
        const ImVec2 low = ImGui::GetItemRectMin();
        const ImVec2 high = ImGui::GetItemRectMax();
        DropPlace place = DropPlace::Into;
        if ( entity != scene.Root() )
        {
            const f32 at = ( ImGui::GetMousePos().y - low.y ) / max( high.y - low.y, 1.0f );
            place = at < cBeside ? DropPlace::Before : at > 1.0f - cBeside ? DropPlace::After : DropPlace::Into;
        }
        Entity target = entity;
        // Below an open node is where its first child is.
        if ( place == DropPlace::After and open )
        {
            target = ChildrenOf( scene, entity ).front();
            place = DropPlace::Before;
        }
        if ( const auto callMaybe = TryDropCall( scene, mDragged, target, place ) )
        {
            ImDrawList& draw = *ImGui::GetWindowDrawList();
            const ImU32 color = ImGui::GetColorU32( ImGuiCol_DragDropTarget );
            if ( place == DropPlace::Into )
                draw.AddRect( low, high, color, 0.0f, 0, 2.0f );
            else
            {
                const f32 y = place == DropPlace::Before and target == entity ? low.y : high.y;
                draw.AddLine( ImVec2( low.x, y ), ImVec2( high.x, y ), color, 2.0f );
            }
            if ( payload->IsDelivery() )
                Run( callMaybe->mId, callMaybe->mArgs );
        }
    }
    ImGui::EndDragDropTarget();
}

void ScenePanel::Keys( Document& document )
{
    // The panel's keys while it has the focus and no field takes them.
    if ( mRenaming != Entity::Null or not ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) or
         ImGui::GetIO().WantTextInput )
        return;
    const Scene& scene = document.GetScene();
    const auto key = [&]( ImGuiKeyChord chord, string_view op ) {
        if ( ImGui::IsKeyChordPressed( chord ) and mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op ) )
            Run( op );
    };
    // Copy, cut, paste, duplicate and group are the editor's, wherever the
    // focus is.
    key( ImGuiKey_Delete, "entity.destroy" );
    const Entity active = document.GetSelection().Active();
    // The arrows walk the rows as they were shown.
    for ( const auto& [arrow, step] :
          { pair<ImGuiKey, i32>{ ImGuiKey_UpArrow, -1 }, pair<ImGuiKey, i32>{ ImGuiKey_DownArrow, 1 } } )
        if ( ImGui::IsKeyPressed( arrow ) and not mRows.empty() )
        {
            const auto found = ranges::find( mRows, active );
            const i32 at = found == mRows.end() ? ( step > 0 ? -1 : static_cast<i32>( mRows.size() ) )
                                                : static_cast<i32>( found - mRows.begin() );
            const i32 next = clamp( at + step, 0, static_cast<i32>( mRows.size() ) - 1 );
            if ( scene.Alive( mRows[static_cast<size_t>( next )] ) )
            {
                Run( "selection.set", EntitiesJson( scene, vector<Entity>{ mRows[static_cast<size_t>( next )] } ) );
                mReveal = mRows[static_cast<size_t>( next )];
            }
        }
    if ( ImGui::IsKeyChordPressed( ImGuiKey_F2 ) and active != Entity::Null and active != scene.Root() )
    {
        mRenaming = active;
        mNewName = NameOf( scene, active ).String();
        mRenameStarted = true;
    }
}
}
