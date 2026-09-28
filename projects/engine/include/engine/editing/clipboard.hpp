#pragma once
#include "engine/scene/scene.hpp"

namespace bubble
{
// The entity cut or copied, to be pasted with what is under it.
class Clipboard
{
public:
    void Cut( Entity entity )
    {
        mEntity = entity;
        mIsCut = true;
    }

    void Copy( Entity entity )
    {
        mEntity = entity;
        mIsCut = false;
    }

    void Clear()
    {
        mEntity = Entity::Null;
        mIsCut = false;
    }

    bool IsEmpty() const { return mEntity == Entity::Null; }
    bool IsCut() const { return mIsCut; }
    Entity GetEntity() const { return mEntity; }

private:
    Entity mEntity = Entity::Null;
    bool mIsCut = false;
};

} // namespace bubble
