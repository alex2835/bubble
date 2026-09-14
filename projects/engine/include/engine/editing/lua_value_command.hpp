#pragma once
#include "engine/editing/command.hpp"
#include "engine/scene/scene.hpp"
#include "engine/types/any.hpp"
#include "engine/types/utility.hpp"
#include <functional>
#include <variant>

// Undo for the Lua tables a component owns - a script's `state`, a shader's
// uniforms. A value inside one is addressed the way a script would spell it:
// the table on the entity, then a path of keys. Never by a sol reference,
// which would keep pointing at the old table after an undo replaced it.
namespace bubble
{
using LuaKey = std::variant<int, string>;
using LuaPath = vector<LuaKey>;

string LuaPathToString( const LuaPath& path );

// Which table: the component's on this entity. `mGet` reaches it, and comes
// back empty when the component is gone - a command outlives the component
// on a deleted entity, and simply does nothing then.
struct LuaTableRoot
{
    Scene* mScene = nullptr;
    Entity mEntity;
    string mName; // "State", "Shader.Uniforms" - the step's name starts with it
    std::function<opt<Table>( Scene&, Entity )> mGet;

    opt<Table> Get() const;
};

// The table holding the last key of `path`, or empty when the path is broken.
opt<Table> LuaParentTable( const LuaTableRoot& root, const LuaPath& path );

// Write `value` at `path`; a nil value removes the key. A table value is
// attached as a deep copy so the caller's copy is never aliased by the live
// state.
void SetLuaValue( const LuaTableRoot& root, const LuaPath& path, const Any& value );

// One value set, added (old is nil) or removed (new is nil).
class SetLuaValueCommand : public ICommand
{
public:
    // Both values are deep-copied on the way in.
    SetLuaValueCommand( LuaTableRoot root, LuaPath path, const Any& oldValue, const Any& newValue );

    ~SetLuaValueCommand() override;

    string_view Name() const override { return mName; }
    void Execute() override;
    void Undo() override;

private:
    LuaTableRoot mRoot;
    LuaPath mPath;
    // Behind pointers: Any is only forward-declared here, like everywhere a
    // header holds one.
    Scope<Any> mOld;
    Scope<Any> mNew;
    string mName;
};

}
