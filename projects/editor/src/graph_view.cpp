#include "bubble/editor/graph_view.hpp"
#include "bubble/animation/animation.hpp"
#include "bubble/editor/editor_style.hpp"
#include "bubble/editor/panel.hpp"
#include "bubble/editor/play_session.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/matrix.hpp"
#include <imgui.h>

// After ImGui's own header, which it takes as given.
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace bubble
{
namespace
{
// The canvas's ids: a node and its two pins three apart, links far above
// any node's.
constexpr uintptr_t cLinks = uintptr_t( 1 ) << 24;

ed::NodeId NodeIdOf( u32 index )
{
    return ed::NodeId( uintptr_t( 3 ) * index + 1 );
}

ed::PinId InPinOf( u32 index )
{
    return ed::PinId( uintptr_t( 3 ) * index + 2 );
}

ed::PinId OutPinOf( u32 index )
{
    return ed::PinId( uintptr_t( 3 ) * index + 3 );
}

u32 NodeOfPin( ed::PinId pin )
{
    return static_cast<u32>( ( pin.Get() - 1 ) / 3 );
}

bool IsOutPin( ed::PinId pin )
{
    return ( pin.Get() - 1 ) % 3 == 2;
}

bool IsInPin( ed::PinId pin )
{
    return ( pin.Get() - 1 ) % 3 == 1;
}

constexpr ImVec4 cEntry = EditorColor::cSet;
constexpr ImVec4 cRefused = EditorColor::cError;

Value TextValue( string_view text )
{
    return Value( *Text::TryFrom( text ) );
}

OptRef<const Json> TryMember( const Json& json, string_view key )
{
    return json.TryAs<Json::Object>() ? json.TryFind( key ) : nullopt;
}

opt<string> TryTextAt( const Json& json, string_view key )
{
    const auto memberMaybe = TryMember( json, key );
    return memberMaybe and memberMaybe->TryAs<string>() ? opt<string>( *memberMaybe->TryAs<string>() ) : nullopt;
}

// A point the editor kept: [x, y].
opt<vec2> TryPoint( OptRef<const Json> jsonMaybe )
{
    const auto arrayMaybe = jsonMaybe ? jsonMaybe->TryAs<Json::Array>() : nullopt;
    if ( not arrayMaybe or arrayMaybe->size() != 2 or not( *arrayMaybe )[0].TryAs<f64>() or
         not( *arrayMaybe )[1].TryAs<f64>() )
        return nullopt;
    return vec2( static_cast<f32>( *( *arrayMaybe )[0].TryAs<f64>() ),
                 static_cast<f32>( *( *arrayMaybe )[1].TryAs<f64>() ) );
}

// What a state plays, in a few words.
string Summary( const Json& state )
{
    if ( const auto clipMaybe = TryTextAt( state, "clip" ); clipMaybe and not clipMaybe->empty() )
        return *clipMaybe;
    if ( const auto blendMaybe = TryMember( state, "blend" ) )
    {
        const auto pointsMaybe = TryMember( *blendMaybe, "points" );
        const size_t count =
            pointsMaybe and pointsMaybe->TryAs<Json::Array>() ? pointsMaybe->TryAs<Json::Array>()->size() : 0;
        return format( "blend of {} on {}", count, TryTextAt( *blendMaybe, "param" ).value_or( "?" ) );
    }
    return "rest pose";
}

vector<string> StateNames( const Json& data )
{
    vector<string> names;
    if ( const auto statesMaybe = TryMember( data, "states" ); statesMaybe and statesMaybe->TryAs<Json::Object>() )
        for ( const Json::Member& member : *statesMaybe->TryAs<Json::Object>() )
            names.push_back( member.mKey );
    return names;
}
}

// The canvas's own state, as imgui-node-editor keeps it: made and destroyed
// with the view, its settings in the document rather than a file of its own.
struct GraphView::Canvas
{
    ed::EditorContext* mContext = nullptr;

    Canvas()
    {
        ed::Config config;
        config.SettingsFile = nullptr;
        mContext = ed::CreateEditor( &config );
    }
    ~Canvas() { ed::DestroyEditor( mContext ); }
    Canvas( const Canvas& ) = delete;
    Canvas& operator=( const Canvas& ) = delete;
};

// A node as it is drawn: a state, the any state or the return.
struct GraphView::Node
{
    string mName;
    string mTitle;
    string mSummary;
    bool mIn = true;
    bool mOut = true;
    bool mEntry = false;
    opt<vec2> mPlaceMaybe;
    // Where it goes when the document keeps no place for it.
    vec2 mFallback{ 0.0f };
};

GraphView::GraphView( EditorProject& project ) : mProject( project ), mCanvas( CreateScope<Canvas>() )
{
    IndexOf( "*" );
    IndexOf( "return" );
}

GraphView::~GraphView() = default;

u32 GraphView::IndexOf( const string& node )
{
    const auto found = mIndices.find( node );
    if ( found != mIndices.end() )
        return found->second;
    const u32 index = static_cast<u32>( mNames.size() );
    mIndices.emplace( node, index );
    mNames.push_back( node );
    return index;
}

void GraphView::Select( string node )
{
    mSelect = node;
    mSelectedState = node == "*" or node == "return" ? string() : std::move( node );
    mSelectedTransition.reset();
}

opt<vec2> GraphView::TryNodeAt( string_view node ) const
{
    const auto found = mDrawn.find( string( node ) );
    return found != mDrawn.end() ? opt<vec2>( found->second ) : nullopt;
}

opt<vec2> GraphView::TryNodeOnScreen( string_view node ) const
{
    const auto found = mOnScreen.find( string( node ) );
    return found != mOnScreen.end() ? opt<vec2>( found->second ) : nullopt;
}

void GraphView::Run( string_view op, Value::Record args )
{
    mPending.emplace_back( string( op ), std::move( args ) );
}

void GraphView::Show( DataDocument& document )
{
    // In a window of its own beside the scene: turned to, its document is
    // the active one, the inspector's.
    if ( ClickedInWindow() )
        mProject.GetWorkspace().Activate( document );
    Live( document );
    const Json& data = document.GetData();
    const OptRef<const Json> editorMaybe = TryMember( data, "editor" );
    const auto placeOf = [&]( string_view key ) {
        return editorMaybe ? TryPoint( TryMember( *editorMaybe, key ) ) : nullopt;
    };
    const vector<string> states = StateNames( data );
    const string entry = TryTextAt( data, "entry" ).value_or( states.empty() ? string() : states.front() );
    vector<Node> nodes;
    nodes.push_back( Node{ .mName = "*",
                           .mTitle = "Any state",
                           .mSummary = "tried before the state's own",
                           .mIn = false,
                           .mOut = true,
                           .mEntry = false,
                           .mPlaceMaybe = placeOf( "any" ),
                           .mFallback = vec2( -280.0f, -140.0f ) } );
    nodes.push_back( Node{ .mName = "return",
                           .mTitle = "Return",
                           .mSummary = "back to where it came from",
                           .mIn = true,
                           .mOut = false,
                           .mEntry = false,
                           .mPlaceMaybe = placeOf( "return" ),
                           .mFallback = vec2( 260.0f * static_cast<f32>( states.size() ), 140.0f ) } );
    const OptRef<const Json> placesMaybe = editorMaybe ? TryMember( *editorMaybe, "states" ) : nullopt;
    const OptRef<const Json> statesMaybe = TryMember( data, "states" );
    for ( size_t at = 0; at < states.size(); ++at )
        nodes.push_back( Node{ .mName = states[at],
                               .mTitle = states[at],
                               .mSummary = Summary( *statesMaybe->TryFind( states[at] ) ),
                               .mIn = true,
                               .mOut = true,
                               .mEntry = states[at] == entry,
                               .mPlaceMaybe = placesMaybe ? TryPoint( TryMember( *placesMaybe, states[at] ) ) : nullopt,
                               .mFallback = vec2( 260.0f * static_cast<f32>( at ), 0.0f ) } );

    // Above the canvas: the preview beside it, or not.
    if ( ImGui::SmallButton( mPreviewShown ? "Hide Preview" : "Preview" ) )
        mPreviewShown = not mPreviewShown;
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "The graph played on a model beside the canvas, its parameters set by hand" );
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ed::SetCurrentEditor( mCanvas->mContext );
    ed::Begin( "graph" );
    Nodes( document, nodes );
    Links( document );
    Create();
    Delete();
    Menus( document );
    // The nodes have their sizes the frame after they are first drawn: then
    // the canvas frames them all.
    if ( mFramesShown++ == 1 )
        ed::NavigateToContent( 0.0f );
    if ( not mSelect.empty() )
    {
        ed::ClearSelection();
        if ( mIndices.contains( mSelect ) )
            ed::SelectNode( NodeIdOf( IndexOf( mSelect ) ) );
        mSelect.clear();
    }
    ed::End();

    if ( not mLiveState.empty() )
        ImGui::GetWindowDrawList()->AddText( ImVec2( corner.x + 8, corner.y + 6 ),
                                             ImGui::GetColorU32( EditorColor::cLive ),
                                             format( "playing in {}", mLiveEntity ).c_str() );

    // One state or one transition selected is what the inspector shows.
    array<ed::NodeId, 2> selectedNodes{};
    array<ed::LinkId, 2> selectedLinks{};
    const int nodeCount = ed::GetSelectedNodes( selectedNodes.data(), 2 );
    const int linkCount = ed::GetSelectedLinks( selectedLinks.data(), 2 );
    mSelectedState.clear();
    mSelectedTransition.reset();
    if ( nodeCount == 1 and linkCount == 0 )
    {
        const string& name = mNames[( selectedNodes[0].Get() - 1 ) / 3];
        if ( name != "*" and name != "return" )
            mSelectedState = name;
    }
    else if ( linkCount == 1 and nodeCount == 0 )
        mSelectedTransition = selectedLinks[0].Get() - cLinks + 1;
    Moved( nodes );
    mShownVersion = document.Version();
    ed::SetCurrentEditor( nullptr );
    RunPending( document );
}

void GraphView::Nodes( const DataDocument& document, span<const Node> nodes )
{
    const bool changed = document.Version() != mShownVersion;
    for ( const Node& node : nodes )
    {
        const u32 index = IndexOf( node.mName );
        const ed::NodeId id = NodeIdOf( index );
        const auto put = mPut.find( node.mName );
        // New to the canvas, or moved by the document - undone, placed by an
        // operator -: where the document says, or where it falls.
        if ( put == mPut.end() or ( changed and node.mPlaceMaybe and *node.mPlaceMaybe != put->second ) )
        {
            const vec2 at = node.mPlaceMaybe.value_or( node.mFallback );
            ed::SetNodePosition( id, ImVec2( at.x, at.y ) );
            mPut[node.mName] = at;
        }
        // The state the game plays now, lit.
        const bool live = node.mName == mLiveState;
        if ( live )
        {
            ed::PushStyleColor( ed::StyleColor_NodeBorder, EditorColor::cLive );
            ed::PushStyleVar( ed::StyleVar_NodeBorderWidth, 3.0f );
        }
        ed::BeginNode( id );
        ImGui::PushID( static_cast<int>( index ) );
        if ( node.mEntry )
            ImGui::TextColored( cEntry, "%s  (entry)", node.mTitle.c_str() );
        else
            ImGui::TextUnformatted( node.mTitle.c_str() );
        ImGui::TextDisabled( "%s", node.mSummary.c_str() );
        if ( node.mIn )
        {
            ed::BeginPin( InPinOf( index ), ed::PinKind::Input );
            ImGui::TextUnformatted( "> in" );
            ed::EndPin();
        }
        if ( node.mOut )
        {
            if ( node.mIn )
                ImGui::SameLine( 0.0f, ImGui::GetFontSize() * 4.0f );
            ed::BeginPin( OutPinOf( index ), ed::PinKind::Output );
            ImGui::TextUnformatted( "out >" );
            ed::EndPin();
        }
        ImGui::PopID();
        ed::EndNode();
        if ( live )
        {
            ed::PopStyleVar();
            ed::PopStyleColor();
        }
        const ImVec2 position = ed::GetNodePosition( id );
        const ImVec2 size = ed::GetNodeSize( id );
        mDrawn[node.mName] = vec2( position.x, position.y );
        const ImVec2 middle = ed::CanvasToScreen( ImVec2( position.x + size.x * 0.5f, position.y + size.y * 0.5f ) );
        mOnScreen[node.mName] = vec2( middle.x, middle.y );
    }
}

void GraphView::Links( const DataDocument& document )
{
    const auto transitionsMaybe = TryMember( document.GetData(), "transitions" );
    if ( not transitionsMaybe or not transitionsMaybe->TryAs<Json::Array>() )
        return;
    const vector<string> states = StateNames( document.GetData() );
    const auto known = [&]( const string& name ) { return ranges::contains( states, name ); };
    const Json::Array& transitions = *transitionsMaybe->TryAs<Json::Array>();
    for ( size_t at = 0; at < transitions.size(); ++at )
    {
        const auto fromMaybe = TryTextAt( transitions[at], "from" );
        const auto toMaybe = TryTextAt( transitions[at], "to" );
        // One that names no node is the document's error, which the
        // inspector shows.
        if ( not fromMaybe or not toMaybe or ( *fromMaybe != "*" and not known( *fromMaybe ) ) or
             ( *toMaybe != "return" and not known( *toMaybe ) ) )
            continue;
        const ImVec4 color = *fromMaybe == "*" ? ImVec4( 0.6f, 0.8f, 1.0f, 1.0f ) : ImVec4( 1.0f, 1.0f, 1.0f, 1.0f );
        ed::Link( ed::LinkId( cLinks + at ), OutPinOf( IndexOf( *fromMaybe ) ), InPinOf( IndexOf( *toMaybe ) ), color,
                  2.0f );
    }
}

void GraphView::Create()
{
    if ( ed::BeginCreate() )
    {
        ed::PinId start;
        ed::PinId end;
        if ( ed::QueryNewLink( &start, &end ) and start and end )
        {
            // Drawn either way, a transition goes from a right to a left.
            if ( IsInPin( start ) and IsOutPin( end ) )
            {
                const ed::PinId from = end;
                end = start;
                start = from;
            }
            const string& from = mNames[NodeOfPin( start )];
            const string& to = mNames[NodeOfPin( end )];
            if ( not IsOutPin( start ) or not IsInPin( end ) or ( from == "*" and to == "return" ) )
                ed::RejectNewItem( cRefused, 2.0f );
            else if ( ed::AcceptNewItem() )
                Run( "graph.add_transition",
                     Value::Record{ ValueField{ .mName = "from"_n, .mValue = TextValue( from ) },
                                    ValueField{ .mName = "to"_n, .mValue = TextValue( to ) } } );
        }
    }
    ed::EndCreate();
}

void GraphView::Delete()
{
    if ( ed::BeginDelete() )
    {
        vector<size_t> transitions;
        vector<string> states;
        ed::LinkId link;
        while ( ed::QueryDeletedLink( &link ) )
            if ( ed::AcceptDeletedItem() )
                transitions.push_back( link.Get() - cLinks );
        ed::NodeId node;
        while ( ed::QueryDeletedNode( &node ) )
        {
            const string& name = mNames[( node.Get() - 1 ) / 3];
            if ( name == "*" or name == "return" )
                ed::RejectDeletedItem();
            // Its transitions go with it in the document, not on the canvas.
            else if ( ed::AcceptDeletedItem( false ) )
                states.push_back( name );
        }
        // The last first: a transition taken out moves those after it.
        ranges::sort( transitions, []( size_t a, size_t b ) { return a > b; } );
        for ( const size_t at : transitions )
            Run( "graph.remove_transition",
                 Value::Record{ ValueField{ .mName = "index"_n, .mValue = Value( static_cast<f64>( at + 1 ) ) } } );
        for ( const string& state : states )
            Run( "graph.remove_state",
                 Value::Record{ ValueField{ .mName = "state"_n, .mValue = TextValue( state ) } } );
        if ( transitions.size() + states.size() > 1 )
            mPendingStep = "Delete";
    }
    ed::EndDelete();
}

void GraphView::Menus( const DataDocument& )
{
    // Inside the canvas the mouse is in its units: where a new state goes.
    const ImVec2 mouse = ImGui::GetMousePos();
    ed::Suspend();
    if ( ed::ShowBackgroundContextMenu() )
    {
        mMenuAt = vec2( mouse.x, mouse.y );
        ImGui::OpenPopup( "graph background" );
    }
    ed::NodeId node;
    if ( ed::ShowNodeContextMenu( &node ) )
    {
        mMenuNode = mNames[( node.Get() - 1 ) / 3];
        ImGui::OpenPopup( "graph node" );
    }
    ed::LinkId link;
    if ( ed::ShowLinkContextMenu( &link ) )
    {
        mMenuLink = link.Get() - cLinks;
        ImGui::OpenPopup( "graph link" );
    }
    if ( ImGui::BeginPopup( "graph background" ) )
    {
        if ( ImGui::MenuItem( "Add State" ) )
            Run( "graph.add_state",
                 Value::Record{
                     ValueField{ .mName = "x"_n, .mValue = Value( static_cast<f64>( round( mMenuAt.x ) ) ) },
                     ValueField{ .mName = "y"_n, .mValue = Value( static_cast<f64>( round( mMenuAt.y ) ) ) } } );
        ImGui::EndPopup();
    }
    if ( ImGui::BeginPopup( "graph node" ) )
    {
        const bool state = mMenuNode != "*" and mMenuNode != "return";
        if ( ImGui::MenuItem( "Make Entry", nullptr, false, state ) )
            Run( "data.set", Value::Record{ ValueField{ .mName = "path"_n, .mValue = TextValue( "entry" ) },
                                            ValueField{ .mName = "value"_n, .mValue = TextValue( mMenuNode ) } } );
        if ( ImGui::MenuItem( "Delete", "Del", false, state ) )
            Run( "graph.remove_state",
                 Value::Record{ ValueField{ .mName = "state"_n, .mValue = TextValue( mMenuNode ) } } );
        ImGui::EndPopup();
    }
    if ( ImGui::BeginPopup( "graph link" ) )
    {
        if ( ImGui::MenuItem( "Delete", "Del" ) and mMenuLink )
            Run( "graph.remove_transition",
                 Value::Record{
                     ValueField{ .mName = "index"_n, .mValue = Value( static_cast<f64>( *mMenuLink + 1 ) ) } } );
        ImGui::EndPopup();
    }
    ed::Resume();
}

void GraphView::Moved( span<const Node> nodes )
{
    // Kept when the mouse lets go, whole units: a file that reads well.
    if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
        return;
    size_t moved = 0;
    for ( const Node& node : nodes )
    {
        const ImVec2 position = ed::GetNodePosition( NodeIdOf( IndexOf( node.mName ) ) );
        const vec2 at( round( position.x ), round( position.y ) );
        if ( glm::length( at - mPut[node.mName] ) < 0.5f )
            continue;
        mPut[node.mName] = at;
        Run( "graph.place",
             Value::Record{ ValueField{ .mName = "state"_n, .mValue = TextValue( node.mName ) },
                            ValueField{ .mName = "x"_n, .mValue = Value( static_cast<f64>( at.x ) ) },
                            ValueField{ .mName = "y"_n, .mValue = Value( static_cast<f64>( at.y ) ) } } );
        ++moved;
    }
    if ( moved > 1 and mPendingStep.empty() )
        mPendingStep = "Move States";
}

void GraphView::RunPending( DataDocument& document )
{
    const vector<pair<string, Value::Record>> pending = std::move( mPending );
    mPending.clear();
    const string step = std::move( mPendingStep );
    mPendingStep.clear();
    if ( pending.empty() )
        return;
    // The graph.* operators work on the active data document: this one.
    mProject.GetWorkspace().Activate( document );
    const vector<string> before = StateNames( document.GetData() );
    const bool grouped = not step.empty() and pending.size() > 1;
    if ( grouped )
        document.BeginGroup( step );
    for ( const auto& [op, args] : pending )
        if ( not mProject.Run( op, args ) )
            break;
    if ( grouped )
        document.EndGroup();
    // A state just added is the one selected.
    const vector<string> after = StateNames( document.GetData() );
    if ( after.size() == before.size() + 1 )
        for ( const string& name : after )
            if ( not ranges::contains( before, name ) )
                Select( name );
}

void GraphView::Live( const DataDocument& document )
{
    mLiveState.clear();
    mLiveEntity.clear();
    const auto playMaybe = mProject.TryPlaying();
    // No game playing: the preview's state, when it is shown.
    if ( not playMaybe )
    {
        if ( const auto stateMaybe = mPreview and mPreviewShown ? mPreview->TryState() : nullopt )
        {
            mLiveState = *stateMaybe;
            mLiveEntity = "the preview";
        }
        return;
    }
    const auto graphIdMaybe = mProject.GetCatalog().TryIdOf( document.GetPath() );
    if ( not graphIdMaybe )
        return;
    const Document& played = playMaybe->Played();
    const Scene& scene = played.GetScene();
    const auto plays = [&]( Entity entity ) {
        const auto animatorMaybe = scene.TryGet<Animator>( entity );
        return animatorMaybe and animatorMaybe->mGraph == *graphIdMaybe;
    };
    Entity entity = played.GetSelection().Active();
    if ( entity == Entity::Null or not scene.Alive( entity ) or not plays( entity ) )
    {
        entity = Entity::Null;
        for ( const Entity each : scene.EntitiesWith<Animator>() )
            if ( plays( each ) )
            {
                entity = each;
                break;
            }
    }
    if ( entity == Entity::Null )
        return;
    if ( const auto stateMaybe = playMaybe->GetWorld().Get<AnimationWorld>().TryState( entity ) )
    {
        mLiveState = *stateMaybe;
        mLiveEntity = PathOf( scene, entity ).mPath.String();
    }
}

GraphPreview& GraphView::Preview( const DataDocument& document )
{
    if ( not mPreview )
        mPreview = CreateScope<GraphPreview>( mProject, document );
    return *mPreview;
}

OptRef<GraphPreview> GraphView::TryPreview()
{
    return mPreview ? OptRef<GraphPreview>( *mPreview ) : nullopt;
}

void GraphView::PreviewControls( const DataDocument& document )
{
    GraphPreview& preview = Preview( document );
    const auto modelMaybe = preview.TryModel();
    const string model = modelMaybe ? string( modelMaybe->View() ) : "no model with clips";
    ImGui::SetNextItemWidth( -1.0f );
    if ( ImGui::BeginCombo( "##model", model.c_str() ) )
    {
        for ( const AssetPath& path : preview.Models() )
            if ( ImGui::Selectable( string( path.View() ).c_str(), modelMaybe and *modelMaybe == path ) )
                preview.SetModel( path );
        ImGui::EndCombo();
    }
    // The parameters as the document declares them, set here alone.
    if ( const OptRef<const Json> parametersMaybe = TryMember( document.GetData(), "parameters" ) )
        if ( const auto objectMaybe = parametersMaybe->TryAs<Json::Object>() )
            for ( const Json::Member& member : *objectMaybe )
            {
                ImGui::PushID( member.mKey.c_str() );
                if ( member.mValue.TryAs<f64>() )
                {
                    f32 value = preview.Parameter( member.mKey );
                    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.5f );
                    if ( ImGui::DragFloat( "##value", &value, 0.01f, 0, 0, "%.2f" ) )
                        preview.SetParameter( member.mKey, value );
                    ImGui::SameLine();
                    ImGui::TextUnformatted( member.mKey.c_str() );
                }
                else if ( member.mValue.TryAs<bool>() )
                {
                    bool on = preview.Parameter( member.mKey ) != 0.0f;
                    if ( ImGui::Checkbox( member.mKey.c_str(), &on ) )
                        preview.SetParameter( member.mKey, on ? 1.0f : 0.0f );
                }
                else if ( ImGui::SmallButton( member.mKey.c_str() ) )
                    preview.Trigger( member.mKey );
                ImGui::PopID();
            }
    if ( not preview.Error().empty() )
        ImGui::TextColored( cRefused, "%s", preview.Error().c_str() );
    else if ( const auto stateMaybe = preview.TryState() )
        ImGui::TextDisabled( "playing %s", stateMaybe->c_str() );
}

GraphView& GraphViewOf( EditorProject& project, DataDocument& document )
{
    if ( not document.TryView() )
        document.SetView( CreateScope<GraphView>( project ) );
    // The editor gives a graph's document no other view.
    return static_cast<GraphView&>( *document.TryView() );
}
}
