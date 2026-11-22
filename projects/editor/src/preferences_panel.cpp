#include "bubble/editor/preferences_panel.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/text.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/matrix.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace bubble
{
string PreferenceLabel( string_view name )
{
    string label( name );
    for ( char& letter : label )
        if ( letter == '_' )
            letter = ' ';
    if ( not label.empty() and label[0] >= 'a' and label[0] <= 'z' )
        label[0] = static_cast<char>( label[0] - 'a' + 'A' );
    return label;
}

void PreferencesPanel::Show()
{
    if ( not mOpen )
        return;
    ImGui::SetNextWindowSize( ImVec2( 460, 520 ), ImGuiCond_FirstUseEver );
    if ( ImGui::Begin( "Preferences", &mOpen ) )
    {
        Reflection& reflection = mPreferences.GetReflection();
        const MetaType type = reflection.TypeOf<EditorPreferences>();
        auto valueMaybe = reflection.TryToValue( mPreferences.Ref() );
        if ( not valueMaybe )
            throw logic_error( format( "the editor's preferences: {}", valueMaybe.error() ) );
        Value::Record& record = *valueMaybe->TryAs<Value::Record>();
        bool changed = Fields( type, record, "" );
        ImGui::Spacing();
        ImGui::Separator();
        if ( ImGui::Button( "Defaults" ) )
        {
            mPreferences.Get() = EditorPreferences{};
            mDrafts.clear();
            mUnsaved = true;
        }
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Every preference as the editor comes with it" );
        if ( changed )
        {
            auto madeMaybe = reflection.TryFromValue( *valueMaybe, type );
            if ( madeMaybe )
            {
                mPreferences.Get() = madeMaybe->cast<EditorPreferences>();
                mUnsaved = true;
            }
            else
                LogError( "preferences: {}", madeMaybe.error() );
        }
    }
    ImGui::End();
    // A drag is one change, written when it ends.
    if ( mUnsaved and not ImGui::IsAnyItemActive() )
    {
        mPreferences.Save();
        mUnsaved = false;
    }
}

bool PreferencesPanel::Fields( const MetaType& type, Value::Record& record, const string& prefix )
{
    Reflection& reflection = mPreferences.GetReflection();
    bool changed = false;
    reflection.ForEachField( type, [&]( Name name, const MetaField& field ) {
        const auto found = ranges::find( record, name, &ValueField::mName );
        if ( found == record.end() )
            return;
        const FieldInfo& info = Reflection::InfoOf( field );
        if ( info.Has( FieldInfo::Hidden ) )
            return;
        const string path = prefix.empty() ? name.ToString() : format( "{}.{}", prefix, name.ToString() );
        ImGui::PushID( path.c_str() );
        if ( auto innerMaybe = found->mValue.TryAs<Value::Record>() )
        {
            // A section: its title, then its fields in two columns.
            ImGui::SeparatorText( PreferenceLabel( name.ToString() ).c_str() );
            changed |= Fields( field.type(), *innerMaybe, path );
        }
        else
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( PreferenceLabel( name.ToString() ).c_str() );
            if ( info.mTooltip and ImGui::IsItemHovered() )
                ImGui::SetTooltip( "%s", info.mTooltip );
            ImGui::SameLine( ImGui::GetFontSize() * 9.0f );
            ImGui::SetNextItemWidth( -1.0f );
            changed |= Leaf( info, found->mValue, path, field.type().is_integral() );
            if ( info.mTooltip and ImGui::IsItemHovered() )
                ImGui::SetTooltip( "%s", info.mTooltip );
        }
        ImGui::PopID();
    } );
    return changed;
}

bool PreferencesPanel::Leaf( const FieldInfo& info, Value& value, const string& path, bool whole )
{
    if ( auto onMaybe = value.TryAs<bool>() )
        return ImGui::Checkbox( "##value", &*onMaybe );
    if ( auto numberMaybe = value.TryAs<f64>() )
    {
        const bool angle = info.Has( FieldInfo::Angle );
        // Shown as degrees, kept as radians.
        f32 shown = static_cast<f32>( angle ? glm::degrees( *numberMaybe ) : *numberMaybe );
        bool changed = false;
        if ( info.mMax > info.mMin and not whole )
            changed = ImGui::SliderFloat( "##value", &shown, info.mMin, info.mMax, angle ? "%.0f deg" : "%.2f" );
        else
        {
            int number = static_cast<int>( shown );
            changed = ImGui::InputInt( "##value", &number, 0 );
            shown = static_cast<f32>(
                info.mMax > info.mMin ? clamp( number, static_cast<int>( info.mMin ), static_cast<int>( info.mMax ) )
                                      : number );
        }
        if ( changed )
            *numberMaybe = angle ? glm::radians( static_cast<f64>( shown ) ) : static_cast<f64>( shown );
        return changed;
    }
    if ( auto colorMaybe = value.TryAs<Color>() )
    {
        array<f32, 3> rgb{ colorMaybe->mR, colorMaybe->mG, colorMaybe->mB };
        if ( not ImGui::ColorEdit3( "##value", rgb.data() ) )
            return false;
        colorMaybe->mR = rgb[0];
        colorMaybe->mG = rgb[1];
        colorMaybe->mB = rgb[2];
        return true;
    }
    if ( auto textMaybe = value.TryAs<Text>() )
    {
        // Typed into a draft while the field is held: a font is tried when
        // the text is done, not at each letter.
        const auto draft = mDrafts.find( path );
        string shown = draft != mDrafts.end() ? draft->second : textMaybe->String();
        ImGui::InputText( "##value", &shown );
        if ( ImGui::IsItemActive() )
            mDrafts[path] = shown;
        else
            mDrafts.erase( path );
        if ( not ImGui::IsItemDeactivatedAfterEdit() )
            return false;
        auto committedMaybe = Text::TryFrom( shown );
        if ( not committedMaybe )
        {
            LogError( "preferences: {}: {}", path, committedMaybe.error() );
            return false;
        }
        *textMaybe = std::move( *committedMaybe );
        return true;
    }
    ImGui::TextDisabled( "%s", value.Describe().c_str() );
    return false;
}
}
