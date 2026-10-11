#include "bubble/scene/uid.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
opt<Uid> Uid::Parse( string_view text )
{
    if ( text.size() != 16 )
        return nullopt;
    const auto value = TryParse<u64>( text, 16 );
    if ( not value or *value == 0 )
        return nullopt;
    return Uid{ *value };
}

string Uid::ToString() const
{
    return format( "{:016x}", mValue );
}
}
