#include "engine/pch/pch.hpp"
#include "engine/editing/ui/interaction.hpp"

namespace bubble::edit_detail
{
EditSession& CurrentSession()
{
    static EditSession session;
    return session;
}
}
