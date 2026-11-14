#include "bubble/editing/document.hpp"

namespace bubble
{
Document::Document( const ComponentTypes& componentTypes, Scene scene )
    : mComponentTypes( componentTypes ),
      mScene( std::move( scene ) ),
      mHistory( mScene ),
      mSelection( mScene )
{
}

OptRef<const AssetPath> Document::TryPath() const
{
    return mPathMaybe ? OptRef<const AssetPath>( *mPathMaybe ) : nullopt;
}
}
