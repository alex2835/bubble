#pragma once
#include "bubble/assets/material_asset.hpp"
#include "bubble/editing/data_document.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/render/material_layout.hpp"
#include "bubble/scripts/script_asset.hpp"

namespace bubble
{
class GraphView;

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
// their prefab; on a prefab's instance, what differs from the prefab is told
// apart, as a script's props set in the scene are. Every edit is property.set on what is selected; a drag or a
// text typed across frames is one step.
//
// A material's document shows its base or its shader, its flags and its
// shader's parameters - each the document's own, its base's or the shader's
// default, the own ones told apart and taken back from their menu -, edited
// through data.set and data.remove. An animation graph's shows its entry,
// its parameters and what is selected on its canvas - a state: its name,
// what it plays, loop, speed, root motion; a transition: its conditions,
// exit time, duration, interrupt -, renamed through graph.rename_state.
//
// A file picked in the assets panel shows instead, until the active document
// or its selection changes: its kind, why it does not import, and how it
// imports - each setting its importer declares, the .import's own told apart
// from the importer's default -, set through asset.set_import.
class Inspector
{
public:
    explicit Inspector( EditorProject& project );

    // Each frame, shown or not: an edit that began in it and has ended -
    // the mouse up, the field left - closes its step.
    void Settle();
    // Its widgets, inside an ImGui window.
    void Show();
    // The file at `path` shown from now on, until the active document or its
    // selection changes.
    void Inspect( string path );
    // Where a field's widget was drawn last, its middle in ImGui's units:
    // "name" for the entity's name, "point_light.intensity", "script.props.speed";
    // a material's "base", "double_sided", "params.roughness"; a file's
    // "import.srgb".
    opt<vec2> TryWhere( string_view field ) const;
    // Whether a field was last shown told apart: a prop the scene sets, a
    // field an instance has otherwise than its prefab.
    bool Marked( string_view field ) const { return mMarked.contains( string( field ) ); }

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

    void Material( DataDocument& document );
    void Graph( DataDocument& document );
    void GraphParameters( const Json& data );
    void GraphState( const Json& data, const string& name, GraphView& view );
    void GraphTransition( const Json& data, size_t index );
    // A row of a table: its label, then its widget's cell.
    void RowLabel( const char* label );
    void MaterialFlag( const char* label, const string& path, opt<bool> own, bool inherited, bool based );
    void MaterialParamRow( const MaterialAsset& material, span<const MaterialAsset> bases, const MaterialParam& param );
    // A data document's label, set ones told apart; their menu takes them
    // back to the base's value, or the default.
    void DataLabel( const string& label, const string& path, bool set, bool based );
    void DataSet( const string& path, Value value );
    void DataRemove( const string& path );

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
    // What the widgets asked, run after them: one step, when a step is
    // named.
    void RunPending();
    // What the panel would show of the documents: the active one, its active
    // entity and how many are selected.
    tuple<const void*, Entity, size_t> DocumentKey();
    void ImportSettings( const string& path );
    // A clip's name, chosen among the project's models' clips or typed.
    bool ClipField( const char* id, string& clip );
    // What the active entity, a node of a prefab's instance, has otherwise
    // than its prefab: fields set, components added.
    void PrefabOverrides( Document& document, Entity entity );
    bool Overridden( Name component, const string& path ) const;
    void ImportSetting( const string& path, const struct ImportSetting& setting, OptRef<const Json> ownMaybe );

    EditorProject& mProject;
    // The file picked in the assets panel, and what the documents showed
    // then.
    string mInspected;
    tuple<const void*, Entity, size_t> mInspectedOver{ nullptr, Entity::Null, 0 };
    vector<pair<string, Value::Record>> mPending;
    string mPendingStep;
    // A state's name as it is typed, and whose it is; a parameter's to come.
    string mStateName;
    string mStateNamed;
    string mNewParameter;
    string mTypedClip;
    string mParameterName;
    // The widget an edit's step is open for: 0 when none is.
    u32 mHeld = 0;
    string mName;
    Entity mNamed = Entity::Null;
    string mFilter;
    // A prop asked to go back to its file's default.
    Name mReset;
    // Where each field's widget was drawn, by component and path; those
    // told apart.
    hmap<string, vec2> mWhere;
    hset<string> mMarked;
    // The active entity's overrides of its prefab: "light.brightness", and
    // components it has that the prefab's node does not.
    hset<string> mOverridden;
    hset<string> mAddedComponents;
    // The scripts shown, loaded for their props.
    hmap<AssetId, AssetRef<ScriptAsset>> mScripts;
};
}
