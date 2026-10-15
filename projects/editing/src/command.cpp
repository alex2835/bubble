#include "bubble/editing/command.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
void Command::Redo( Scene& scene )
{
    if ( auto doneMaybe = TryDo( scene ); not doneMaybe )
        throw logic_error( format( "Redo, where the first time was fine: {}", doneMaybe.error() ) );
}

bool Command::Absorb( const Command& )
{
    return false;
}
}
