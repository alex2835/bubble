#pragma once
#include "bubble/editor/editor_project.hpp"
#include "bubble/scripts/script_asset.hpp"

namespace bubble
{
// "light_color" as a person reads it: "Light color".
string FieldLabel( string_view name );

// What a value of `type` starts as when a list grows by one: false, 0, an
// empty text, no asset; a record with each field so.
Value DefaultOf( const ValueType& type );

// The panel of the active entity of the active document: its name, then its
// components, each field a widget by its kind as reflection describes it -
// degrees for an angle, a slider in its range, a picker of assets of its
// kind, an entity's path or a row dropped from the tree - and a script's
// props as its file declares them, the ones set in the scene told apart.
// Components are added and removed from it, and reverted to or applied to
// their prefab. Every edit is property.set on what is selected; a drag or a
// text typed across frames is one step.
class Inspector
{
public:
    explicit Inspector( EditorProject& project );

    // Each frame, shown or not: an edit that began in it and has ended -
    // the mouse up, the field left - closes its step.
    void Settle();
    // Its widgets, inside an ImGui window.
    void Show();
    // Where a field's widget was drawn last, its middle in ImGui's units:
    // "name" for the entity's name, "point_light.intensity", "script.props.speed".
    opt<vec2> TryWhere( string_view field ) const;

private:
    // A field's row as it is shown: its label, its path in the component,
    // whether a prop is set in the scene rather than at its default.
    struct Row
    {
        string mLabel;
        string mPath;
        Name mComponent;
        bool mSet = false;
        bool mProp = false;
    };

    void NameRow( Document& document, Entity entity );
    void Component( Document& document, Entity entity, const ComponentTypes::Type& type );
    void ReflectedFields( const Scene& scene, Name component, const MetaAny& owner, const string& prefix );
    void ScriptProps( const Scene& scene, const Value& overrides, const AssetId& scriptId );
    void AddComponent( Document& document, Entity entity );

    // A field's row - the label, then the widget - or rows, for a list or a
    // record; true when `value` was changed.
    bool ValueRows( const Scene& scene, const Row& row, Value& value, const ValueType& type, const FieldInfo& info );
    // The label, its tooltip and its menu: back to the prefab, a prop back to
    // its default.
    void Label( const Scene& scene, const Row& row, const FieldInfo& info );
    bool Leaf( const Scene& scene, const Row& row, Value& value, ValueKind kind, const FieldInfo& info );
    void Shown( const string& field );
    bool AssetPicker( const string& path, AssetId& asset, const char* kind );
    bool EntityField( const Scene& scene, const string& path, EntityPath& entity );
    // After an editing widget: its step opened as it is taken hold of.
    void Hold( bool changed );

    void Set( Name component, const string& path, Value value );
    void Run( string_view op, Value::Record args );

    EditorProject& mProject;
    vector<pair<string, Value::Record>> mPending;
    // The widget an edit's step is open for: 0 when none is.
    u32 mHeld = 0;
    string mName;
    Entity mNamed = Entity::Null;
    string mFilter;
    // A prop asked to go back to its file's default.
    Name mReset;
    // Where each field's widget was drawn, by component and path.
    hmap<string, vec2> mWhere;
    // The scripts shown, loaded for their props.
    hmap<AssetId, AssetRef<ScriptAsset>> mScripts;
};
}
