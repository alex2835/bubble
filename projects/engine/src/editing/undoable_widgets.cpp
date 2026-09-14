#include "engine/pch/pch.hpp"
#include "engine/editing/edit_tracking.hpp"

namespace bubble::edit_detail
{
EditSession& CurrentSession()
{
    static EditSession session;
    return session;
}
}
