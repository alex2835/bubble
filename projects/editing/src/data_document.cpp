#include "bubble/editing/data_document.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
bool Same( const opt<Json>& a, const opt<Json>& b )
{
    if ( a.has_value() != b.has_value() )
        return false;
    return not a or a->Write() == b->Write();
}
}

DataDocument::DataDocument( AssetPath path, Json data ) : mPath( std::move( path ) ), mData( std::move( data ) )
{
}

OptRef<DocumentView> DataDocument::TryView()
{
    return mView ? OptRef<DocumentView>( *mView ) : nullopt;
}

OptRef<const Json> DataDocument::TryAt( const FieldPath& path ) const
{
    const Json* at = &mData;
    for ( const FieldPath::Step& step : path.Steps() )
    {
        if ( const Name* name = get_if<Name>( &step ) )
        {
            const auto objectMaybe = at->TryAs<Json::Object>();
            if ( not objectMaybe )
                return nullopt;
            const auto found = ranges::find( *objectMaybe, name->ToString(), &Json::Member::mKey );
            if ( found == objectMaybe->end() )
                return nullopt;
            at = &found->mValue;
        }
        else
        {
            const size_t position = get<size_t>( step );
            const auto arrayMaybe = at->TryAs<Json::Array>();
            if ( not arrayMaybe or position < 1 or position > arrayMaybe->size() )
                return nullopt;
            at = &( *arrayMaybe )[position - 1];
        }
    }
    return *at;
}

expected<void, string> DataDocument::TryPut( const FieldPath& path, const opt<Json>& value, opt<size_t> place )
{
    const span<const FieldPath::Step> steps = path.Steps();
    if ( steps.empty() )
        return unexpected( "a path into the file is needed"s );
    Json* at = &mData;
    for ( size_t index = 0; index < steps.size(); ++index )
    {
        const bool last = index + 1 == steps.size();
        const string where = path.Prefix( index + 1 );
        if ( const Name* name = get_if<Name>( &steps[index] ) )
        {
            const auto objectMaybe = at->TryAs<Json::Object>();
            if ( not objectMaybe )
                return unexpected(
                    format( "{}: {} is not an object", where, index == 0 ? "the file" : path.Prefix( index ) ) );
            Json::Object& object = *objectMaybe;
            const string key = name->ToString();
            auto found = ranges::find( object, key, &Json::Member::mKey );
            if ( last )
            {
                if ( value and found != object.end() )
                    found->mValue = *value;
                else if ( value and place )
                    object.insert( object.begin() + static_cast<i64>( min( *place, object.size() ) ),
                                   Json::Member{ .mKey = key, .mValue = *value } );
                else if ( value )
                    object.push_back( Json::Member{ .mKey = key, .mValue = *value } );
                else if ( found != object.end() )
                    object.erase( found );
                else
                    return unexpected( format( "{}: not there", where ) );
                return {};
            }
            // On the way to a value set, a missing object is made.
            if ( found == object.end() )
            {
                if ( not value )
                    return unexpected( format( "{}: not there", where ) );
                object.push_back( Json::Member{ .mKey = key, .mValue = Json( Json::Object() ) } );
                found = object.end() - 1;
            }
            at = &found->mValue;
        }
        else
        {
            const size_t position = get<size_t>( steps[index] );
            const auto arrayMaybe = at->TryAs<Json::Array>();
            if ( not arrayMaybe )
                return unexpected( format( "{}: {} is not an array", where, path.Prefix( index ) ) );
            Json::Array& array = *arrayMaybe;
            const bool onePast = last and value and position == array.size() + 1;
            if ( position < 1 or ( position > array.size() and not onePast ) )
                return unexpected( format( "{}: the array has {}", where, array.size() ) );
            if ( last )
            {
                if ( value and place )
                    array.insert( array.begin() + static_cast<i64>( min( *place, array.size() ) ), *value );
                else if ( onePast )
                    array.push_back( *value );
                else if ( value )
                    array[position - 1] = *value;
                else
                    array.erase( array.begin() + static_cast<i64>( position - 1 ) );
                return {};
            }
            at = &array[position - 1];
        }
    }
    return {};
}

expected<void, string> DataDocument::TrySet( const FieldPath& path, Json value, string label )
{
    return TryChange( path, std::move( value ), std::move( label ) );
}

expected<void, string> DataDocument::TryRemove( const FieldPath& path, string label )
{
    return TryChange( path, nullopt, std::move( label ) );
}

expected<void, string> DataDocument::TryChange( const FieldPath& changed, opt<Json> value, string label )
{
    if ( not value and not TryAt( changed ) )
        return unexpected( format( "{}: not there", changed.ToString() ) );
    // Objects made on the way to a value set go with it when it is undone:
    // the change is the first of them made.
    FieldPath path = changed;
    if ( value )
    {
        FieldPath prefix;
        for ( const FieldPath::Step& step : changed.Steps() )
        {
            prefix = prefix.Then( step );
            if ( not TryAt( prefix ) )
            {
                path = prefix;
                break;
            }
        }
    }
    const auto beforeMaybe = TryAt( path );
    opt<Json> before = beforeMaybe ? opt<Json>( *beforeMaybe ) : nullopt;
    const opt<size_t> place = before ? TryPlaceOf( path ) : nullopt;
    // A value set to what it is is no step.
    if ( path == changed and Same( before, value ) )
        return {};
    if ( auto putMaybe = TryPut( changed, value ); not putMaybe )
        return putMaybe;
    // What a set made, objects made on the way with it. A removal leaves
    // nothing - in an array, what comes after moves into its position.
    if ( value )
    {
        const auto afterMaybe = TryAt( path );
        value = afterMaybe ? opt<Json>( *afterMaybe ) : nullopt;
    }
    ++mVersion;
    if ( InGroup() )
    {
        // The same path set again in this group carries the first set on.
        if ( mGroup.mChanges.size() > mGroupStarts.back() and mGroup.mChanges.back().mPath == path )
            mGroup.mChanges.back().mAfter = std::move( value );
        else
            mGroup.mChanges.push_back( Change{
                .mPath = path, .mBefore = std::move( before ), .mAfter = std::move( value ), .mPlace = place } );
        return {};
    }
    Push(
        Step{ .mId = mNextId++,
              .mLabel = std::move( label ),
              .mChanges = { Change{
                  .mPath = path, .mBefore = std::move( before ), .mAfter = std::move( value ), .mPlace = place } } } );
    return {};
}

opt<size_t> DataDocument::TryPlaceOf( const FieldPath& path ) const
{
    const span<const FieldPath::Step> steps = path.Steps();
    if ( const size_t* position = get_if<size_t>( &steps.back() ) )
        return *position - 1;
    FieldPath parent;
    for ( size_t index = 0; index + 1 < steps.size(); ++index )
        parent = parent.Then( steps[index] );
    const auto holderMaybe = steps.size() == 1 ? OptRef<const Json>( mData ) : TryAt( parent );
    const auto objectMaybe = holderMaybe ? holderMaybe->TryAs<Json::Object>() : nullopt;
    if ( not objectMaybe )
        return nullopt;
    const auto found = ranges::find( *objectMaybe, get<Name>( steps.back() ).ToString(), &Json::Member::mKey );
    return found != objectMaybe->end() ? opt<size_t>( static_cast<size_t>( found - objectMaybe->begin() ) ) : nullopt;
}

void DataDocument::TakeBack( const Change& change )
{
    // One the change took out comes back where it stood; one it set goes
    // back to what it was, in its place.
    static_cast<void>( TryPut( change.mPath, change.mBefore, change.mAfter ? nullopt : change.mPlace ) );
}

void DataDocument::Push( Step step )
{
    mUndo.push_back( std::move( step ) );
    mRedo.clear();
}

void DataDocument::Undo()
{
    if ( InGroup() )
        throw logic_error( "DataDocument::Undo inside a group: the group is not a step yet" );
    if ( mUndo.empty() )
        return;
    Step step = std::move( mUndo.back() );
    mUndo.pop_back();
    for ( auto change = step.mChanges.rbegin(); change != step.mChanges.rend(); ++change )
        TakeBack( *change );
    mRedo.push_back( std::move( step ) );
    ++mVersion;
}

void DataDocument::Redo()
{
    if ( InGroup() )
        throw logic_error( "DataDocument::Redo inside a group: the group is not a step yet" );
    if ( mRedo.empty() )
        return;
    Step step = std::move( mRedo.back() );
    mRedo.pop_back();
    for ( const Change& change : step.mChanges )
        static_cast<void>( TryPut( change.mPath, change.mAfter ) );
    mUndo.push_back( std::move( step ) );
    ++mVersion;
}

void DataDocument::BeginGroup( string label )
{
    if ( mGroupStarts.empty() )
        mGroup = Step{ .mId = 0, .mLabel = std::move( label ), .mChanges = {} };
    mGroupStarts.push_back( mGroup.mChanges.size() );
}

void DataDocument::EndGroup()
{
    if ( mGroupStarts.empty() )
        throw logic_error( "DataDocument::EndGroup with no group open" );
    mGroupStarts.pop_back();
    if ( not mGroupStarts.empty() or mGroup.mChanges.empty() )
        return;
    mGroup.mId = mNextId++;
    Push( std::move( mGroup ) );
    mGroup = Step();
}

void DataDocument::CancelGroup()
{
    if ( mGroupStarts.empty() )
        throw logic_error( "DataDocument::CancelGroup with no group open" );
    const size_t start = mGroupStarts.back();
    mGroupStarts.pop_back();
    while ( mGroup.mChanges.size() > start )
    {
        TakeBack( mGroup.mChanges.back() );
        mGroup.mChanges.pop_back();
        ++mVersion;
    }
}
}
