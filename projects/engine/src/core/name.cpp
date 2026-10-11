#include "bubble/core/name.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"

namespace bubble
{
namespace
{
struct NameTable
{
    mutex mMutex;
    hmap<u32, string> mTexts;
};

NameTable& Table()
{
    static NameTable table;
    return table;
}
}

expected<Name, string> Name::From( string_view text )
{
    if ( not detail::NameValid( text ) )
        return unexpected( format( "'{}' is not a name: a name is ASCII snake_case - a lowercase letter, then "
                                   "lowercase letters, digits and _",
                                   text ) );
    const u32 hash = detail::NameHash( text );
    if ( hash == 0 )
        return unexpected( format( "'{}' hashes to 0, the empty name's hash; rename it", text ) );

    NameTable& table = Table();
    const lock_guard lock( table.mMutex );
    const auto [entry, added] = table.mTexts.try_emplace( hash, text );
    if ( not added and entry->second != text )
        return unexpected( format( "'{}' and '{}' have the same hash; rename one of them", text, entry->second ) );
    return Name( hash );
}

string Name::ToString() const
{
    if ( Empty() )
        return {};
    NameTable& table = Table();
    const lock_guard lock( table.mMutex );
    const auto found = table.mTexts.find( mHash );
    return found != table.mTexts.end() ? found->second : format( "#{:08x}", mHash );
}
}
