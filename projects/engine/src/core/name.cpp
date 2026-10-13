#include "bubble/core/name.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"

namespace bubble
{
namespace
{
// Read far more often than written - every message, every parse of a name
// already known - so readers share the lock.
struct NameTable
{
    shared_mutex mMutex;
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

    const auto collision = [&]( const string& known ) {
        return unexpected( format( "'{}' and '{}' have the same hash; rename one of them", text, known ) );
    };
    NameTable& table = Table();
    {
        const shared_lock lock( table.mMutex );
        const auto found = table.mTexts.find( hash );
        if ( found != table.mTexts.end() )
        {
            if ( found->second != text )
                return collision( found->second );
            return Name( hash );
        }
    }
    // Not there yet: take the lock alone. Another thread may have added the
    // name between the two locks; try_emplace finds it then.
    const lock_guard lock( table.mMutex );
    const auto [entry, added] = table.mTexts.try_emplace( hash, text );
    if ( not added and entry->second != text )
        return collision( entry->second );
    return Name( hash );
}

string Name::ToString() const
{
    if ( Empty() )
        return {};
    NameTable& table = Table();
    const shared_lock lock( table.mMutex );
    const auto found = table.mTexts.find( mHash );
    return found != table.mTexts.end() ? found->second : format( "#{:08x}", mHash );
}
}
