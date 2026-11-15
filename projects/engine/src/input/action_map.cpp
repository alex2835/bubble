#include "bubble/input/action_map.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
ActionMap& ActionMap::Bind( string_view action, ActionInput input, f32 scale )
{
    const auto nameMaybe = Name::TryFrom( action );
    if ( not nameMaybe )
        throw logic_error( format( "ActionMap::Bind: {}", nameMaybe.error() ) );
    auto [found, added] = mIndices.try_emplace( *nameMaybe, static_cast<u32>( mActions.size() ) );
    if ( added )
        mActions.push_back( Action{ .mName = *nameMaybe, .mBindings = {} } );
    mActions[found->second].mBindings.push_back( ActionBinding{ .mInput = input, .mScale = scale } );
    return *this;
}

opt<u32> ActionMap::TryIndexOf( Name action ) const
{
    const auto found = mIndices.find( action );
    if ( found == mIndices.end() )
        return nullopt;
    return found->second;
}

string ActionMap::Names() const
{
    string names;
    for ( const Action& action : mActions )
    {
        if ( not names.empty() )
            names += ", ";
        names += action.mName.ToString();
    }
    return names;
}
}
