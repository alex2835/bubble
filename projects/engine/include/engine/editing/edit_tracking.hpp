#pragma once
#include <imgui.h>
#include <any>

// The part of an undoable widget that is about ImGui and nothing else: turning
// the frames of an interaction into one (start, end) pair. Shared by the
// component field widgets and the Lua table inspector.
namespace bubble::edit_detail
{
// The interaction in progress. ImGui has at most one active item, so one
// slot is enough; the item id is checked so a stale start value is never
// paired with another widget's end.
struct EditSession
{
    ImGuiID mItem = 0;
    std::any mStart;
};
EditSession& CurrentSession();

template <typename T>
bool Differs( const T& a, const T& b )
{
    if constexpr ( requires { { a != b } -> std::convertible_to<bool>; } )
        return a != b;
    else
        return true; // no way to tell, record it
}

// Call right after the widget for a value. `before` is the value as it was
// before the widget ran this frame, `value` as it is after, `changed` what
// the widget returned. `record( from, to )` is called once per interaction:
// for a drag when the mouse is released, from the value the drag started at;
// for a checkbox or a combo pick, on the frame it changed.
template <typename T, typename Record>
void TrackEdit( bool changed, const T& before, const T& value, Record&& record )
{
    auto& session = CurrentSession();
    const ImGuiID item = ImGui::GetItemID();
    if ( ImGui::IsItemActivated() )
        session = { item, before };

    if ( ImGui::IsItemDeactivatedAfterEdit() )
    {
        // A drag ends here: the step spans from where the drag began. A
        // one-frame widget (checkbox) lands here too, with start == before.
        const bool ownSession = session.mItem == item and session.mStart.has_value();
        const T start = ownSession ? std::any_cast<T>( session.mStart ) : before;
        session = {};
        if ( Differs( start, value ) )
            record( start, value );
    }
    else if ( ImGui::IsItemDeactivated() )
    {
        session = {};
    }
    else if ( changed and not ImGui::IsItemActive() )
    {
        // Changed with no activation cycle to hang the step on - a Selectable
        // inside a BeginCombo, whose popup is the "last item" by now.
        if ( Differs( before, value ) )
            record( before, value );
    }
}

}
