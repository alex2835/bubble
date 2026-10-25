#pragma once
#include "bubble/core/name.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"
#include "bubble/types/variant.hpp"

namespace bubble
{
// A way into an object's fields - "shadow.bias", "points[2].value" - parsed
// once, so a command that sets the same field every frame does not read the
// text again. A step is a field's name or a position in a list; positions
// count from 1, as in Luau and in Check's messages.
class FieldPath
{
public:
    // A field's name, or a position in a list from 1.
    using Step = variant<Name, size_t>;

    static expected<FieldPath, string> TryParse( string_view text );

    span<const Step> Steps() const { return mSteps; }
    // This path and one step more: how a walk over an object names what it
    // finds.
    FieldPath Then( Step step ) const;
    // The first `count` steps as text: what a message says got that far.
    string Prefix( size_t count ) const;
    string ToString() const { return Prefix( mSteps.size() ); }

    friend bool operator==( const FieldPath&, const FieldPath& ) = default;

private:
    vector<Step> mSteps;
};
}
