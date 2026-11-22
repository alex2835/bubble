#pragma once
#include "bubble/data/value.hpp"
#include "bubble/editor/editor_preferences.hpp"

namespace bubble
{
// Edit > Preferences: a window of the preferences' fields as reflection
// describes them - a section a group, a slider for a number with a range, a
// checkbox, a colour, a text kept as typed until Enter or the field is left
// -, each change taking effect at once and written to the file when the
// mouse lets go of it. The fields are edited as a Value, as a file holds
// them, and made the preferences again.
class PreferencesPanel
{
public:
    explicit PreferencesPanel( Preferences& preferences ) : mPreferences( preferences ) {}

    void Open() { mOpen = true; }
    bool IsOpen() const { return mOpen; }
    // Drawn while open, between ImGui::NewFrame and ImGui::Render.
    void Show();

private:
    // The fields of `record`, which reflection describes as `type`; `prefix`
    // their path. True when one changed.
    bool Fields( const MetaType& type, Value::Record& record, const string& prefix );
    // One value: `whole` for an integer.
    bool Leaf( const FieldInfo& info, Value& value, const string& path, bool whole );

    Preferences& mPreferences;
    bool mOpen = false;
    // Changed and not yet written: written once no field is held.
    bool mUnsaved = false;
    // Texts being typed, by their path, until they are committed.
    hmap<string, string> mDrafts;
};

// "font_size" as a person reads it: "Font size".
string PreferenceLabel( string_view name );
}
