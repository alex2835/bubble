#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/animation_graph_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/scene/components/animator_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <imgui_node_editor.h>
#include <nlohmann/json.hpp>

namespace ed = ax::NodeEditor;

namespace bubble
{
namespace
{
// Ids. Node editor ids must be unique within a context and non zero; each
// tab has its own context, so state indices can be used directly.
constexpr u64 cAnyNode = 1;
constexpr u64 cReturnNode = 2;
constexpr u64 cFirstStateNode = 10;
constexpr u64 cInPinBase = 10000;
constexpr u64 cOutPinBase = 20000;
constexpr u64 cAnyOutPin = cOutPinBase + 9000;
constexpr u64 cReturnInPin = cInPinBase + 9000;
constexpr u64 cLinkBase = 30000;

ed::NodeId StateNode( i32 state ) { return cFirstStateNode + state; }
ed::PinId InPin( i32 state ) { return cInPinBase + state; }
ed::PinId OutPin( i32 state ) { return cOutPinBase + state; }
ed::LinkId TransitionLink( i32 transition ) { return cLinkBase + transition; }

// The state a pin belongs to: Transition::cAnyState for the any node's out
// pin, Transition::cReturn for the return node's in pin, -2 for neither.
i32 StateOfOutPin( ed::PinId pin )
{
    const u64 id = pin.Get();
    if ( id == cAnyOutPin ) return Transition::cAnyState;
    if ( id >= cOutPinBase and id < cOutPinBase + 9000 ) return (i32)( id - cOutPinBase );
    return -2;
}
i32 StateOfInPin( ed::PinId pin )
{
    const u64 id = pin.Get();
    if ( id == cReturnInPin ) return Transition::cReturn;
    if ( id >= cInPinBase and id < cInPinBase + 9000 ) return (i32)( id - cInPinBase );
    return -2;
}

const ImVec4 cEntryColor( 0.35f, 0.75f, 0.35f, 1.0f );
const ImVec4 cLiveColor( 1.0f, 0.75f, 0.2f, 1.0f );
const ImVec4 cSatisfiedColor( 0.4f, 1.0f, 0.4f, 1.0f );
const ImVec4 cLinkColor( 0.75f, 0.75f, 0.75f, 1.0f );
const ImVec4 cAnyColor( 0.55f, 0.55f, 0.85f, 1.0f );

string Join( const vector<string>& parts, const char* separator )
{
    string out;
    for ( const string& part : parts )
        out += ( out.empty() ? "" : separator ) + part;
    return out;
}

vector<string> Split( string_view text, char separator )
{
    vector<string> out;
    for ( const auto part : std::views::split( text, separator ) )
    {
        string s( part.begin(), part.end() );
        const auto begin = s.find_first_not_of( ' ' );
        if ( begin == string::npos )
            continue;
        out.push_back( s.substr( begin, s.find_last_not_of( ' ' ) - begin + 1 ) );
    }
    return out;
}

// A combo over names, with the current one shown; returns true and sets
// `value` when another is picked. An empty list draws nothing.
bool NameCombo( const char* label, string& value, const vector<string>& names )
{
    bool changed = false;
    if ( ImGui::BeginCombo( label, value.empty() ? "-" : value.c_str() ) )
    {
        for ( const string& name : names )
            if ( ImGui::Selectable( name.c_str(), name == value ) and name != value )
            {
                value = name;
                changed = true;
            }
        ImGui::EndCombo();
    }
    return changed;
}
}


AnimationGraphWindow::AnimationGraphWindow( BubbleEditor& editor )
    : UserInterfaceWindowBase( editor )
{
    mOpen = false;
}

AnimationGraphWindow::~AnimationGraphWindow()
{
    Close();
}

string_view AnimationGraphWindow::Name()
{
    return "Animation Graph"sv;
}

void AnimationGraphWindow::OnUpdate( DeltaTime )
{
}


// ------------------------------------------------------------ document --

void AnimationGraphWindow::Open( const Ref<AnimationController>& controller )
{
    Close();
    mSource = controller;
    if ( not controller )
        return;
    mDraft = *controller;
    mDirty = false;
    mTab = -1;
    mSelectedState = -1;
    mSelectedTransition = -1;
}

void AnimationGraphWindow::Close()
{
    for ( auto& [_, context] : mEditors )
        ed::DestroyEditor( context );
    mEditors.clear();
    mPlaced.clear();
    mSource = nullptr;
    mDirty = false;
}

void AnimationGraphWindow::Save()
{
    if ( not mSource )
        return;
    // Every tab's layout, not just the one showing.
    const i32 tab = mTab;
    for ( const auto& [layer, _] : mEditors )
    {
        mTab = layer;
        StorePositions();
    }
    mTab = tab;

    if ( mDraft.Save() )
        mDirty = false;
    // The hot reloader picks the file up from here and swaps it into the
    // loader's controller, so the animators see it within a second.
}

StateMachine& AnimationGraphWindow::Machine()
{
    if ( mTab >= 0 and mTab < (i32)mDraft.mLayers.size() )
        return mDraft.mLayers[mTab].mMachine;
    return mDraft;
}

const StateMachine& AnimationGraphWindow::Machine() const
{
    if ( mTab >= 0 and mTab < (i32)mDraft.mLayers.size() )
        return mDraft.mLayers[mTab].mMachine;
    return mDraft;
}

string AnimationGraphWindow::PositionKey( const string& state ) const
{
    if ( mTab >= 0 and mTab < (i32)mDraft.mLayers.size() )
        return mDraft.mLayers[mTab].mName + "/" + state;
    return state;
}

void AnimationGraphWindow::PlaceNodes()
{
    const StateMachine& machine = Machine();
    ed::SetNodePosition( cAnyNode, ImVec2( -260, 0 ) );
    ed::SetNodePosition( cReturnNode, ImVec2( -260, 140 ) );
    for ( size_t i = 0; i < machine.mStates.size(); i++ )
    {
        const auto saved = mDraft.mNodePositions.find( PositionKey( machine.mStates[i].mName ) );
        const ImVec2 position = saved != mDraft.mNodePositions.end()
                                ? ImVec2( saved->second.x, saved->second.y )
                                : ImVec2( 240.0f * ( i % 4 ), 140.0f * ( i / 4 ) );
        ed::SetNodePosition( StateNode( (i32)i ), position );
    }
}

void AnimationGraphWindow::StorePositions()
{
    const auto context = mEditors.find( mTab );
    if ( context == mEditors.end() or not mPlaced.contains( mTab ) )
        return;
    ed::SetCurrentEditor( context->second );
    const StateMachine& machine = Machine();
    for ( size_t i = 0; i < machine.mStates.size(); i++ )
    {
        const ImVec2 position = ed::GetNodePosition( StateNode( (i32)i ) );
        mDraft.mNodePositions[PositionKey( machine.mStates[i].mName )] = vec2( position.x, position.y );
    }
}


// --------------------------------------------------------------- edits --

void AnimationGraphWindow::AddState( string name, const ImVec2& position )
{
    StateMachine& machine = Machine();
    // Unique: "state", "state 2", ...
    string candidate = name;
    for ( int n = 2; machine.FindState( candidate ) >= 0; n++ )
        candidate = std::format( "{} {}", name, n );
    ControllerState state;
    state.mName = candidate;
    machine.mStates.push_back( std::move( state ) );
    mDraft.mNodePositions[PositionKey( candidate )] = vec2( position.x, position.y );
    if ( mPlaced.contains( mTab ) )
        ed::SetNodePosition( StateNode( (i32)machine.mStates.size() - 1 ), position );
    mDirty = true;
}

void AnimationGraphWindow::RemoveState( i32 state )
{
    StateMachine& machine = Machine();
    if ( state < 0 or state >= (i32)machine.mStates.size() )
        return;
    // The transitions that named it go; the rest shift down past it.
    std::erase_if( machine.mTransitions, [&]( const Transition& t ) { return t.mFrom == state or t.mTo == state; } );
    for ( Transition& t : machine.mTransitions )
    {
        if ( t.mFrom > state ) t.mFrom--;
        if ( t.mTo > state ) t.mTo--;
    }
    mDraft.mNodePositions.erase( PositionKey( machine.mStates[state].mName ) );
    machine.mStates.erase( machine.mStates.begin() + state );
    if ( machine.mEntry == state )
        machine.mEntry = 0;
    else if ( machine.mEntry > state )
        machine.mEntry--;
    // The nodes above it took its neighbours' ids: lay them out again from
    // the stored positions rather than let them jump.
    mPlaced.erase( mTab );
    mSelectedState = -1;
    mSelectedTransition = -1;
    mDirty = true;
}

void AnimationGraphWindow::AddTransition( i32 from, i32 to )
{
    Transition transition;
    transition.mFrom = from;
    transition.mTo = to;
    // Something to fire on, so the file stays valid: an exit time, which
    // the sidebar can change to a condition.
    transition.mExitTime = 1.0f;
    Machine().mTransitions.push_back( std::move( transition ) );
    mSelectedTransition = (i32)Machine().mTransitions.size() - 1;
    mSelectedState = -1;
    mDirty = true;
}


// ---------------------------------------------------------------- live --

const AnimatorComponent* AnimationGraphWindow::LiveAnimator() const
{
    if ( not mSource )
        return nullptr;
    const AnimatorComponent* found = nullptr;
    mProject.mLevel.mScene.ForEach<AnimatorComponent>( [&]( Entity, const AnimatorComponent& animator )
    {
        if ( not found and animator.mController == mSource )
            found = &animator;
    } );
    return found;
}

const Playback* AnimationGraphWindow::LivePlayback() const
{
    const AnimatorComponent* animator = LiveAnimator();
    if ( not animator )
        return nullptr;
    if ( mTab < 0 )
        return &animator->mBase;
    if ( mTab < (i32)animator->mLayers.size() and mTab < (i32)mDraft.mLayers.size() and
         animator->mLayers[mTab].mName == mDraft.mLayers[mTab].mName )
        return &animator->mLayers[mTab].mPlayback;
    return nullptr;
}

vector<string> AnimationGraphWindow::ClipNames() const
{
    Ref<Model> model;
    if ( const AnimatorComponent* animator = LiveAnimator() )
    {
        // Through the scene: the animator's model is the entity's.
        mProject.mLevel.mScene.ForEach<ModelComponent, AnimatorComponent>(
            [&]( Entity, const ModelComponent& m, const AnimatorComponent& a )
        {
            if ( &a == animator )
                model = m.mModel;
        } );
    }
    if ( not model )
        for ( const auto& [_, candidate] : mProject.mLoader.mModels )
            if ( candidate and candidate->Skinned() )
            {
                model = candidate;
                break;
            }
    vector<string> names;
    if ( model )
        for ( const auto& clip : model->mClips )
            names.push_back( clip->mName );
    return names;
}

vector<string> AnimationGraphWindow::JointNames() const
{
    for ( const auto& [_, candidate] : mProject.mLoader.mModels )
    {
        if ( not candidate or not candidate->Skinned() )
            continue;
        vector<string> names;
        for ( const auto& [name, index] : candidate->mSkeleton->mJointByName )
            names.push_back( name );
        std::ranges::sort( names );
        return names;
    }
    return {};
}


// ---------------------------------------------------------------- draw --

void AnimationGraphWindow::OnDraw( DeltaTime )
{
    if ( not mUIGlobals.mShowAnimationGraph )
        return;
    mOpen = true;
    ImGui::SetNextWindowSize( ImVec2( 1100, 650 ), ImGuiCond_FirstUseEver );
    const string title = std::format( "{}{}###AnimationGraph", Name(), mDirty ? " *" : "" );
    if ( not ImGui::Begin( title.c_str(), &mOpen ) )
    {
        ImGui::End();
        if ( not mOpen )
            mUIGlobals.mShowAnimationGraph = false;
        return;
    }
    if ( not mOpen )
        mUIGlobals.mShowAnimationGraph = false;

    // Nothing open: the selected entity's controller, if it has one, is the
    // obvious thing to be looking at.
    if ( not mSource and mSelection.IsSingleSelection() )
    {
        const Entity entity = mSelection.GetSingleEntity();
        if ( mProject.mLevel.mScene.HasComponent<AnimatorComponent>( entity ) )
            if ( const auto& controller = mProject.mLevel.mScene.GetComponent<AnimatorComponent>( entity ).mController )
                Open( controller );
    }
    if ( not mSource and not mProject.mLoader.mControllers.empty() )
        Open( mProject.mLoader.mControllers.begin()->second );
    DrawToolbar();
    if ( mSource )
    {
        DrawTabs();
        const float sidebar = 340.0f * mWindow.GetUIScale();
        ImGui::BeginChild( "graph", ImVec2( -sidebar, 0 ), false );
        DrawGraph();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild( "sidebar", ImVec2( 0, 0 ), true );
        DrawSidebar();
        ImGui::EndChild();
    }
    else
    {
        ImGui::TextDisabled( "Pick a controller above, or add a .anim file to the project." );
    }
    ImGui::End();
}

void AnimationGraphWindow::DrawToolbar()
{
    ImGui::SetNextItemWidth( 240.0f * mWindow.GetUIScale() );
    if ( ImGui::BeginCombo( "##controller", mSource ? mSource->mName.c_str() : "Controller" ) )
    {
        for ( const auto& [path, controller] : mProject.mLoader.mControllers )
            if ( ImGui::Selectable( path.string().c_str(), controller == mSource ) and controller != mSource )
                Open( controller );
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled( not mSource or not mDirty );
    if ( ImGui::Button( "Save" ) )
        Save();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled( not mSource );
    if ( ImGui::Button( "Revert" ) )
        Open( mSource );
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled( "right click the canvas for a state, drag pin to pin for a transition, Delete removes" );
}

void AnimationGraphWindow::DrawTabs()
{
    if ( not ImGui::BeginTabBar( "machines" ) )
        return;
    const auto tab = [&]( const char* label, i32 index )
    {
        if ( ImGui::BeginTabItem( label ) )
        {
            if ( mTab != index )
            {
                StorePositions();
                mTab = index;
                mSelectedState = -1;
                mSelectedTransition = -1;
            }
            ImGui::EndTabItem();
        }
    };
    tab( "Base", -1 );
    for ( size_t i = 0; i < mDraft.mLayers.size(); i++ )
        tab( mDraft.mLayers[i].mName.c_str(), (i32)i );
    if ( ImGui::TabItemButton( "+", ImGuiTabItemFlags_Trailing ) )
        ImGui::OpenPopup( "new layer" );
    if ( ImGui::BeginPopup( "new layer" ) )
    {
        ImGui::InputText( "name", mNewLayer );
        if ( ImGui::Button( "Add layer" ) and not mNewLayer.empty() )
        {
            ControllerLayer layer;
            layer.mName = mNewLayer;
            ControllerState none;
            none.mName = "none";
            layer.mMachine.mStates.push_back( none );
            mDraft.mLayers.push_back( std::move( layer ) );
            mNewLayer.clear();
            mDirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::EndTabBar();
}

void AnimationGraphWindow::DrawGraph()
{
    auto context = mEditors.find( mTab );
    if ( context == mEditors.end() )
    {
        ed::Config config;
        config.SettingsFile = nullptr; // positions live in the .anim
        context = mEditors.emplace( mTab, ed::CreateEditor( &config ) ).first;
    }
    ed::SetCurrentEditor( context->second );

    const StateMachine& machine = Machine();
    const Playback* live = LivePlayback();
    const AnimatorComponent* animator = LiveAnimator();
    const i32 liveState = live and live->mRuntime.mCurrent >= 0 and live->mRuntime.mCurrent < (i32)machine.mStates.size()
                          and machine.mStates[live->mRuntime.mCurrent].mName == live->mStateName
                          ? live->mRuntime.mCurrent : -1;

    ed::Begin( "graph" );

    if ( not mPlaced.contains( mTab ) )
    {
        PlaceNodes();
        mPlaced.insert( mTab );
        ed::NavigateToContent( 0.0f );
    }

    // The two fixed nodes.
    ed::PushStyleColor( ed::StyleColor_NodeBorder, cAnyColor );
    ed::BeginNode( cAnyNode );
    ImGui::TextUnformatted( "any state" );
    ImGui::TextDisabled( "from \"*\"" );
    ed::BeginPin( cAnyOutPin, ed::PinKind::Output );
    ImGui::TextUnformatted( "   out >" );
    ed::EndPin();
    ed::EndNode();
    ed::BeginNode( cReturnNode );
    ImGui::TextUnformatted( "return" );
    ImGui::TextDisabled( "to the state before" );
    ed::BeginPin( cReturnInPin, ed::PinKind::Input );
    ImGui::TextUnformatted( "> in" );
    ed::EndPin();
    ed::EndNode();
    ed::PopStyleColor();

    for ( size_t i = 0; i < machine.mStates.size(); i++ )
    {
        const bool isLive = (i32)i == liveState;
        const bool isEntry = (i32)i == machine.mEntry;
        if ( isLive )
            ed::PushStyleColor( ed::StyleColor_NodeBorder, cLiveColor );
        else if ( isEntry )
            ed::PushStyleColor( ed::StyleColor_NodeBorder, cEntryColor );
        if ( isLive or isEntry )
            ed::PushStyleVar( ed::StyleVar_NodeBorderWidth, 3.0f );
        DrawNode( (i32)i );
        if ( isLive or isEntry )
        {
            ed::PopStyleVar();
            ed::PopStyleColor();
        }
    }

    for ( size_t i = 0; i < machine.mTransitions.size(); i++ )
    {
        const Transition& transition = machine.mTransitions[i];
        const ed::PinId from = transition.mFrom == Transition::cAnyState ? ed::PinId( cAnyOutPin ) : OutPin( transition.mFrom );
        const ed::PinId to = transition.mTo == Transition::cReturn ? ed::PinId( cReturnInPin ) : InPin( transition.mTo );
        // Lit when its conditions hold on the live animator and it is a way
        // out of the live state - what the inspector shows, on the graph.
        bool satisfied = false;
        if ( animator and liveState >= 0 and ( transition.mFrom == Transition::cAnyState or transition.mFrom == liveState ) )
            satisfied = std::ranges::all_of( transition.mConditions,
                [&]( const Condition& c ) { return c.Holds( animator->mParameters ); } );
        ed::Link( TransitionLink( (i32)i ), from, to, satisfied ? cSatisfiedColor : cLinkColor, satisfied ? 3.0f : 1.5f );
    }

    HandleCreate();
    HandleDelete();

    // What is selected, for the sidebar.
    {
        ed::NodeId node;
        ed::LinkId link;
        if ( ed::GetSelectedNodes( &node, 1 ) == 1 and node.Get() >= cFirstStateNode )
        {
            mSelectedState = (i32)( node.Get() - cFirstStateNode );
            mSelectedTransition = -1;
        }
        else if ( ed::GetSelectedLinks( &link, 1 ) == 1 )
        {
            mSelectedTransition = (i32)( link.Get() - cLinkBase );
            mSelectedState = -1;
        }
        else if ( ed::GetSelectedObjectCount() == 0 and ed::HasSelectionChanged() )
        {
            mSelectedState = -1;
            mSelectedTransition = -1;
        }
    }

    HandleContextMenus();
    ed::End();

    // A hovered link says what it fires on.
    if ( const ed::LinkId hovered = ed::GetHoveredLink() )
    {
        const i32 index = (i32)( hovered.Get() - cLinkBase );
        if ( index >= 0 and index < (i32)machine.mTransitions.size() )
        {
            const Transition& t = machine.mTransitions[index];
            vector<string> when;
            for ( const Condition& c : t.mConditions )
                when.push_back( c.ToString() );
            if ( t.mExitTime )
                when.push_back( std::format( "exit {:.2f}", *t.mExitTime ) );
            ImGui::SetTooltip( "%s  (%.2fs%s)", Join( when, " and " ).c_str(), t.mDuration, t.mInterrupt ? ", interrupts" : "" );
        }
    }
}

void AnimationGraphWindow::DrawNode( i32 index )
{
    const ControllerState& state = Machine().mStates[index];
    ed::BeginNode( StateNode( index ) );
    ImGui::PushID( index );
    ImGui::TextUnformatted( state.mName.c_str() );
    string detail;
    if ( state.IsBlend() )
        detail = std::format( "blend on {} ({} clips)", state.mBlendParameter, state.mBlend.mPoints.size() );
    else if ( state.mClip.empty() )
        detail = "nothing";
    else
        detail = state.mClip;
    if ( not state.mLoop )
        detail += "  once";
    if ( state.mRootMotion )
        detail += "  root";
    ImGui::TextDisabled( "%s", detail.c_str() );
    ed::BeginPin( InPin( index ), ed::PinKind::Input );
    ImGui::TextUnformatted( "> in" );
    ed::EndPin();
    ImGui::SameLine( 0.0f, 40.0f );
    ed::BeginPin( OutPin( index ), ed::PinKind::Output );
    ImGui::TextUnformatted( "out >" );
    ed::EndPin();
    ImGui::PopID();
    ed::EndNode();
}

void AnimationGraphWindow::HandleCreate()
{
    if ( ed::BeginCreate( cLinkColor, 2.0f ) )
    {
        ed::PinId a, b;
        if ( ed::QueryNewLink( &a, &b ) )
        {
            // Either way round: the out pin is the "from".
            i32 from = StateOfOutPin( a ), to = StateOfInPin( b );
            if ( from == -2 or to == -2 )
            {
                from = StateOfOutPin( b );
                to = StateOfInPin( a );
            }
            const bool valid = from != -2 and to != -2 and from != to and
                               not ( from == Transition::cAnyState and to == Transition::cReturn );
            if ( not valid )
                ed::RejectNewItem( ImVec4( 1, 0.3f, 0.3f, 1 ), 2.0f );
            else if ( ed::AcceptNewItem( cSatisfiedColor, 3.0f ) )
                AddTransition( from, to );
        }
    }
    ed::EndCreate();
}

void AnimationGraphWindow::HandleDelete()
{
    // Links first, and nodes after, each as their own pass over the draft:
    // the indices in the ids are only valid until the first erase.
    vector<i32> transitions, states;
    if ( ed::BeginDelete() )
    {
        ed::LinkId link;
        while ( ed::QueryDeletedLink( &link ) )
            if ( ed::AcceptDeletedItem() )
                transitions.push_back( (i32)( link.Get() - cLinkBase ) );
        ed::NodeId node;
        while ( ed::QueryDeletedNode( &node ) )
        {
            if ( node.Get() < cFirstStateNode )
                ed::RejectDeletedItem();
            else if ( ed::AcceptDeletedItem( false ) )
                states.push_back( (i32)( node.Get() - cFirstStateNode ) );
        }
    }
    ed::EndDelete();

    StateMachine& machine = Machine();
    std::ranges::sort( transitions, std::greater{} );
    for ( const i32 t : transitions )
        if ( t >= 0 and t < (i32)machine.mTransitions.size() )
        {
            machine.mTransitions.erase( machine.mTransitions.begin() + t );
            mSelectedTransition = -1;
            mDirty = true;
        }
    std::ranges::sort( states, std::greater{} );
    for ( const i32 s : states )
        RemoveState( s );
}

void AnimationGraphWindow::HandleContextMenus()
{
    ed::Suspend();
    static ImVec2 sClickPosition;
    if ( ed::ShowBackgroundContextMenu() )
    {
        sClickPosition = ed::ScreenToCanvas( ImGui::GetMousePos() );
        ImGui::OpenPopup( "canvas menu" );
    }
    ed::NodeId contextNode;
    if ( ed::ShowNodeContextMenu( &contextNode ) and contextNode.Get() >= cFirstStateNode )
    {
        mSelectedState = (i32)( contextNode.Get() - cFirstStateNode );
        ImGui::OpenPopup( "node menu" );
    }
    if ( ImGui::BeginPopup( "canvas menu" ) )
    {
        if ( ImGui::MenuItem( "New state" ) )
            AddState( "state", sClickPosition );
        if ( ImGui::MenuItem( "Fit to view" ) )
            ed::NavigateToContent();
        ImGui::EndPopup();
    }
    if ( ImGui::BeginPopup( "node menu" ) )
    {
        StateMachine& machine = Machine();
        if ( mSelectedState >= 0 and mSelectedState < (i32)machine.mStates.size() )
        {
            if ( ImGui::MenuItem( "Set as entry" ) )
            {
                machine.mEntry = mSelectedState;
                mDirty = true;
            }
            if ( ImGui::MenuItem( "Delete" ) )
                RemoveState( mSelectedState );
        }
        ImGui::EndPopup();
    }
    ed::Resume();
}


// ------------------------------------------------------------- sidebar --

void AnimationGraphWindow::DrawSidebar()
{
    if ( ImGui::CollapsingHeader( "Controller", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        const vector<string> joints = JointNames();
        if ( joints.empty() )
        {
            if ( ImGui::InputText( "root joint", mDraft.mRootJoint ) )
                mDirty = true;
        }
        else
        {
            vector<string> options = { "" };
            options.insert( options.end(), joints.begin(), joints.end() );
            if ( NameCombo( "root joint", mDraft.mRootJoint, options ) )
                mDirty = true;
        }
        DrawParameters();
    }
    if ( mTab >= 0 and ImGui::CollapsingHeader( "Layer", ImGuiTreeNodeFlags_DefaultOpen ) )
        DrawLayerSettings();

    ImGui::Separator();
    const StateMachine& machine = Machine();
    if ( mSelectedState >= 0 and mSelectedState < (i32)machine.mStates.size() )
        DrawStateProperties( mSelectedState );
    else if ( mSelectedTransition >= 0 and mSelectedTransition < (i32)machine.mTransitions.size() )
        DrawTransitionProperties( mSelectedTransition );
    else
        ImGui::TextDisabled( "Select a state or a transition." );

    if ( const AnimatorComponent* animator = LiveAnimator() )
    {
        ImGui::Separator();
        ImGui::TextDisabled( "live: %s", string( animator->CurrentState() ).c_str() );
    }
}

void AnimationGraphWindow::DrawParameters()
{
    ImGui::TextDisabled( "parameters" );
    i32 remove = -1;
    for ( size_t i = 0; i < mDraft.mParameters.size(); i++ )
    {
        auto& [name, parameter] = mDraft.mParameters[i];
        ImGui::PushID( (int)i );
        ImGui::SetNextItemWidth( 120.0f * mWindow.GetUIScale() );
        switch ( parameter.mType )
        {
        case Parameter::Type::Float:
            if ( ImGui::DragFloat( "##v", &parameter.mValue, 0.01f ) ) mDirty = true;
            break;
        case Parameter::Type::Bool:
        {
            bool value = parameter.mValue != 0.0f;
            if ( ImGui::Checkbox( "##v", &value ) ) { parameter.mValue = value ? 1.0f : 0.0f; mDirty = true; }
            break;
        }
        case Parameter::Type::Trigger:
            ImGui::TextDisabled( "trigger" );
            break;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted( name.c_str() );
        ImGui::SameLine();
        if ( ImGui::SmallButton( "x" ) )
            remove = (i32)i;
        ImGui::PopID();
    }
    if ( remove >= 0 )
    {
        mDraft.mParameters.erase( mDraft.mParameters.begin() + remove );
        mDirty = true;
    }
    ImGui::SetNextItemWidth( 110.0f * mWindow.GetUIScale() );
    ImGui::InputText( "##new", mNewParameter );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 80.0f * mWindow.GetUIScale() );
    ImGui::Combo( "##type", &mNewParameterType, "float\0bool\0trigger\0" );
    ImGui::SameLine();
    if ( ImGui::SmallButton( "add" ) and not mNewParameter.empty() )
    {
        Parameter parameter;
        parameter.mType = static_cast<Parameter::Type>( mNewParameterType );
        mDraft.mParameters.emplace_back( mNewParameter, parameter );
        mNewParameter.clear();
        mDirty = true;
    }
}

void AnimationGraphWindow::DrawLayerSettings()
{
    ControllerLayer& layer = mDraft.mLayers[mTab];
    string mask = Join( layer.mMask, ", " );
    if ( ImGui::InputText( "mask", mask ) )
    {
        layer.mMask = Split( mask, ',' );
        mDirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "(?)" );
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "joint names, comma separated; each with its subtree, !name takes one out" );
    bool weightFromParameter = not layer.mWeightParameter.empty();
    if ( ImGui::Checkbox( "weight from parameter", &weightFromParameter ) )
    {
        layer.mWeightParameter = weightFromParameter and not mDraft.mParameters.empty() ? mDraft.mParameters[0].first : string();
        mDirty = true;
    }
    if ( weightFromParameter )
    {
        vector<string> names;
        for ( const auto& [name, _] : mDraft.mParameters )
            names.push_back( name );
        if ( NameCombo( "weight", layer.mWeightParameter, names ) )
            mDirty = true;
    }
    else if ( ImGui::SliderFloat( "weight", &layer.mWeight, 0.0f, 1.0f ) )
    {
        mDirty = true;
    }
    if ( ImGui::Checkbox( "additive", &layer.mAdditive ) )
        mDirty = true;
    if ( ImGui::Button( "Remove layer" ) )
    {
        if ( auto context = mEditors.find( mTab ); context != mEditors.end() )
        {
            ed::DestroyEditor( context->second );
            mEditors.erase( context );
        }
        mPlaced.erase( mTab );
        mDraft.mLayers.erase( mDraft.mLayers.begin() + mTab );
        // The layers above shift down, and their editors with them.
        map<i32, ed::EditorContext*> editors;
        set<i32> placed;
        for ( auto& [tab, context] : mEditors )
        {
            editors[tab > mTab ? tab - 1 : tab] = context;
            if ( mPlaced.contains( tab ) )
                placed.insert( tab > mTab ? tab - 1 : tab );
        }
        mEditors = std::move( editors );
        mPlaced = std::move( placed );
        mTab = -1;
        mDirty = true;
    }
}

void AnimationGraphWindow::DrawStateProperties( i32 index )
{
    StateMachine& machine = Machine();
    ControllerState& state = machine.mStates[index];
    ImGui::PushID( "state" );
    ImGui::TextUnformatted( "State" );

    // Renamed through a buffer, applied on Enter: transitions hold the
    // index, but the layout and the live animator hold the name.
    if ( mRenaming != index )
    {
        mRename = state.mName;
        mRenaming = index;
    }
    if ( ImGui::InputText( "name", mRename ) and not mRename.empty() and machine.FindState( mRename ) < 0 )
    {
        mDraft.mNodePositions[PositionKey( mRename )] = mDraft.mNodePositions[PositionKey( state.mName )];
        mDraft.mNodePositions.erase( PositionKey( state.mName ) );
        state.mName = mRename;
        mDirty = true;
    }

    int kind = state.IsBlend() ? 1 : state.mClip.empty() ? 2 : 0;
    if ( ImGui::RadioButton( "clip", &kind, 0 ) or ( ImGui::SameLine(), ImGui::RadioButton( "blend", &kind, 1 ) )
         or ( ImGui::SameLine(), ImGui::RadioButton( "nothing", &kind, 2 ) ) )
    {
        if ( kind != 1 )
        {
            state.mBlend = {};
            state.mBlendParameter.clear();
        }
        if ( kind == 2 )
            state.mClip.clear();
        if ( kind == 1 and state.mBlend.Empty() )
        {
            state.mBlend.Add( state.mClip, 0.0f );
            state.mClip.clear();
            if ( state.mBlendParameter.empty() and not mDraft.mParameters.empty() )
                state.mBlendParameter = mDraft.mParameters[0].first;
        }
        mDirty = true;
    }

    const vector<string> clips = ClipNames();
    vector<string> parameters;
    for ( const auto& [name, _] : mDraft.mParameters )
        parameters.push_back( name );

    if ( kind == 0 )
    {
        if ( clips.empty() ? ImGui::InputText( "clip", state.mClip ) : NameCombo( "clip", state.mClip, clips ) )
            mDirty = true;
    }
    else if ( kind == 1 )
    {
        if ( NameCombo( "param", state.mBlendParameter, parameters ) )
            mDirty = true;
        ImGui::TextDisabled( "points" );
        i32 remove = -1;
        vector<BlendPoint> points = state.mBlend.mPoints;
        bool changed = false;
        for ( size_t i = 0; i < points.size(); i++ )
        {
            ImGui::PushID( (int)i );
            ImGui::SetNextItemWidth( 70.0f * mWindow.GetUIScale() );
            if ( ImGui::DragFloat( "##at", &points[i].mValue, 0.05f ) ) changed = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 150.0f * mWindow.GetUIScale() );
            if ( clips.empty() ? ImGui::InputText( "##clip", points[i].mClip ) : NameCombo( "##clip", points[i].mClip, clips ) ) changed = true;
            ImGui::SameLine();
            if ( ImGui::SmallButton( "x" ) ) remove = (i32)i;
            ImGui::PopID();
        }
        if ( remove >= 0 )
        {
            points.erase( points.begin() + remove );
            changed = true;
        }
        if ( ImGui::SmallButton( "add point" ) )
        {
            points.push_back( { clips.empty() ? string() : clips[0], points.empty() ? 0.0f : points.back().mValue + 1.0f } );
            changed = true;
        }
        if ( changed )
        {
            // Rebuilt through Add, which keeps them sorted by value.
            state.mBlend = {};
            for ( const BlendPoint& point : points )
                state.mBlend.Add( point.mClip, point.mValue );
            mDirty = true;
        }
    }

    if ( kind != 1 and ImGui::Checkbox( "loop", &state.mLoop ) )
        mDirty = true;
    bool speedFromParameter = not state.mSpeedParameter.empty();
    if ( ImGui::Checkbox( "speed from parameter", &speedFromParameter ) )
    {
        state.mSpeedParameter = speedFromParameter and not parameters.empty() ? parameters[0] : string();
        mDirty = true;
    }
    if ( speedFromParameter )
    {
        if ( NameCombo( "speed", state.mSpeedParameter, parameters ) )
            mDirty = true;
    }
    else if ( ImGui::DragFloat( "speed", &state.mSpeed, 0.01f, -10.0f, 10.0f ) )
    {
        mDirty = true;
    }
    if ( ImGui::Checkbox( "root motion", &state.mRootMotion ) )
        mDirty = true;
    if ( machine.mEntry != index and ImGui::Button( "Set as entry" ) )
    {
        machine.mEntry = index;
        mDirty = true;
    }

    // The clip's markers, when the state plays one clip.
    if ( kind == 0 and not state.mClip.empty() )
    {
        ImGui::Separator();
        ImGui::TextDisabled( "events of %s", state.mClip.c_str() );
        vector<ClipEvent>& markers = mDraft.mEvents[state.mClip];
        i32 remove = -1;
        bool changed = false;
        for ( size_t i = 0; i < markers.size(); i++ )
        {
            ImGui::PushID( (int)i );
            ImGui::SetNextItemWidth( 70.0f * mWindow.GetUIScale() );
            if ( ImGui::DragFloat( "##t", &markers[i].mTime, 0.005f, 0.0f, 1.0f ) ) changed = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 150.0f * mWindow.GetUIScale() );
            if ( ImGui::InputText( "##n", markers[i].mName ) ) changed = true;
            ImGui::SameLine();
            if ( ImGui::SmallButton( "x" ) ) remove = (i32)i;
            ImGui::PopID();
        }
        if ( remove >= 0 )
        {
            markers.erase( markers.begin() + remove );
            changed = true;
        }
        if ( ImGui::SmallButton( "add event" ) )
        {
            markers.push_back( { 0.5f, "event" } );
            changed = true;
        }
        if ( changed )
        {
            std::ranges::stable_sort( markers, {}, &ClipEvent::mTime );
            mDirty = true;
        }
        if ( markers.empty() )
            mDraft.mEvents.erase( state.mClip );
    }
    ImGui::PopID();
}

void AnimationGraphWindow::DrawTransitionProperties( i32 index )
{
    StateMachine& machine = Machine();
    Transition& transition = machine.mTransitions[index];
    ImGui::PushID( "transition" );
    const string from = transition.mFrom == Transition::cAnyState ? "any state" : machine.mStates[transition.mFrom].mName;
    const string to = transition.mTo == Transition::cReturn ? "return" : machine.mStates[transition.mTo].mName;
    ImGui::Text( "Transition  %s -> %s", from.c_str(), to.c_str() );

    ImGui::TextDisabled( "when (all of)" );
    i32 remove = -1;
    for ( size_t i = 0; i < transition.mConditions.size(); i++ )
    {
        ImGui::PushID( (int)i );
        string text = transition.mConditions[i].ToString();
        ImGui::SetNextItemWidth( 200.0f * mWindow.GetUIScale() );
        if ( ImGui::InputText( "##c", text, ImGuiInputTextFlags_EnterReturnsTrue ) )
        {
            try
            {
                transition.mConditions[i] = Condition::Parse( text );
                mDirty = true;
            }
            catch ( const std::exception& e )
            {
                LogError( "{}", e.what() );
            }
        }
        ImGui::SameLine();
        if ( ImGui::SmallButton( "x" ) ) remove = (i32)i;
        ImGui::PopID();
    }
    if ( remove >= 0 )
    {
        transition.mConditions.erase( transition.mConditions.begin() + remove );
        mDirty = true;
    }
    if ( ImGui::SmallButton( "add condition" ) )
    {
        Condition condition;
        condition.mParameter = mDraft.mParameters.empty() ? "param" : mDraft.mParameters[0].first;
        transition.mConditions.push_back( condition );
        mDirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "(?)" );
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "name, !name, or name <op> value with == != < <= > >=; Enter applies" );

    bool hasExit = transition.mExitTime.has_value();
    if ( ImGui::Checkbox( "exit time", &hasExit ) )
    {
        transition.mExitTime = hasExit ? std::optional<f32>( 0.9f ) : std::nullopt;
        mDirty = true;
    }
    if ( hasExit )
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 100.0f * mWindow.GetUIScale() );
        f32 exit = *transition.mExitTime;
        if ( ImGui::DragFloat( "##exit", &exit, 0.01f, 0.0f, 1.0f ) )
        {
            transition.mExitTime = exit;
            mDirty = true;
        }
    }
    if ( ImGui::DragFloat( "duration", &transition.mDuration, 0.01f, 0.0f, 5.0f, "%.2f s" ) )
        mDirty = true;
    if ( ImGui::Checkbox( "may interrupt a transition", &transition.mInterrupt ) )
        mDirty = true;
    if ( transition.mConditions.empty() and not transition.mExitTime )
        ImGui::TextColored( ImVec4( 1, 0.4f, 0.4f, 1 ), "needs a condition or an exit time" );

    // Order matters: the first that holds fires.
    if ( index > 0 and ImGui::SmallButton( "check earlier" ) )
    {
        std::swap( machine.mTransitions[index], machine.mTransitions[index - 1] );
        mSelectedTransition = index - 1;
        mDirty = true;
    }
    if ( index + 1 < (i32)machine.mTransitions.size() )
    {
        ImGui::SameLine();
        if ( ImGui::SmallButton( "check later" ) )
        {
            std::swap( machine.mTransitions[index], machine.mTransitions[index + 1] );
            mSelectedTransition = index + 1;
            mDirty = true;
        }
    }
    ImGui::PopID();
}

}
