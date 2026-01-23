#pragma once
#include "engine/project/project_tree.hpp"

namespace bubble
{

class Clipboard
{
public:
    void Cut( Ref<ProjectTreeNode> node )
    {
        mNode = node;
        mIsCut = true;
    }

    void Copy( Ref<ProjectTreeNode> node )
    {
        mNode = node;
        mIsCut = false;
    }

    void Clear()
    {
        mNode = nullptr;
        mIsCut = false;
    }

    bool IsEmpty() const
    {
        return mNode == nullptr;
    }

    bool IsCut() const
    {
        return mIsCut;
    }

    Ref<ProjectTreeNode> GetNode() const
    {
        return mNode;
    }

private:
    Ref<ProjectTreeNode> mNode;
    bool mIsCut = false;
};

} // namespace bubble
