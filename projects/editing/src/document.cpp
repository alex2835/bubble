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

OptRef<PrefabNodes> Document::TryPrefabNodes()
{
    return mPrefabNodesMaybe ? OptRef<PrefabNodes>( *mPrefabNodesMaybe ) : nullopt;
}

OptRef<const PrefabNodes> Document::TryPrefabNodes() const
{
    return mPrefabNodesMaybe ? OptRef<const PrefabNodes>( *mPrefabNodesMaybe ) : nullopt;
}

void Document::ForgetHistory()
{
    const bool dirty = IsDirty();
    mHistory.Clear();
    // No position of an empty history is the saved one when it was dirty.
    mSavedAt = dirty ? ~u64( 0 ) : mHistory.Position();
}

OptRef<const AssetPath> Document::TryPath() const
{
    return mPathMaybe ? OptRef<const AssetPath>( *mPathMaybe ) : nullopt;
}

OptRef<DocumentView> Document::TryView()
{
    return mView ? OptRef<DocumentView>( *mView ) : nullopt;
}
}
