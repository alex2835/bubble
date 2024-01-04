
#include "engine/project/project.hpp"

namespace bubble
{

const std::string& Project::Name()
{
    return mName;
}


const std::path& Project::Root()
{
    return mRoot;
}


}
