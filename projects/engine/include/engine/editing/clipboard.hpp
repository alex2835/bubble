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
        mEntity = INVALID_ENTITY;
        mIsCut = false;
    }

    bool IsEmpty() const { return mEntity == INVALID_ENTITY; }
    bool IsCut() const { return mIsCut; }
    Entity GetEntity() const { return mEntity; }

private:
    Entity mEntity = INVALID_ENTITY;
    bool mIsCut = false;
};

} // namespace bubble
