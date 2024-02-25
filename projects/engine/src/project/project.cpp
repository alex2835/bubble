
#include "engine/project/project.hpp"

namespace bubble
{
const string& Project::Name()
{
    return mName;
}

const path& Project::Root()
{
    return mRoot;
}

void Project::Open( const path& rootPath )
{

}

void Project::Save()
{

}

Scene& Project::Scene()
{
    return mScene;
}

}
