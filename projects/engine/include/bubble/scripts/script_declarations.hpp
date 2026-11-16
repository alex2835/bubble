#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/string.hpp"

namespace bubble
{
class App;

// Names a project declares in its data that scripts pass as strings: the
// input's actions, later buses and parameters. Declared as types of their
// own - "jump" | "move_x" - so a typo is a type error, not a surprise in
// the game.
struct ScriptLabels
{
    vector<string> mActions;
};

// bubble.d.luau: what a world's scripts see of the engine, as a Luau
// definition file - for the analyzer at import and luau-lsp in an editor.
// Each component a class with its fields, the entity with a field for each
// component, color and asset, the services by name, input, and the
// runtime's globals. Made from App's descriptions, so it says what the
// engine has, no more.
string WriteScriptDeclarations( App& app, const ScriptLabels& labels = {} );
}
