// Copy, paste and duplicate: a copy outlives what it was copied from and goes
// into any document, is named apart from what is there, and the absolute
// links inside it lead into the copy. Each paste is one step of the history.
#include "bubble/editing/edits.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    f32 mBrightness = 1;
};

struct Follow
{
    EntityPath mTarget;
    vector<EntityPath> mWaypoints;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

EntityPath Link( string_view path )
{
    return EntityPath{ Make( path ) };
}

FieldPath Path( string_view text )
{
    auto pathMaybe = FieldPath::TryParse( text );
    REQUIRE_MESSAGE( pathMaybe, pathMaybe.error() );
    return *pathMaybe;
}

void Require( const expected<void, string>& doneMaybe )
{
    REQUIRE_MESSAGE( doneMaybe, doneMaybe.error() );
}

template <typename T>
T Require( expected<T, string> doneMaybe )
{
    REQUIRE_MESSAGE( doneMaybe, doneMaybe.error() );
    return std::move( *doneMaybe );
}

struct Types
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };

    Types()
    {
        mReflection.Type<Light>( "light" ).Field<&Light::mBrightness>( "brightness" );
        mReflection.Type<Follow>( "follow" )
            .Field<&Follow::mTarget>( "target" )
            .Field<&Follow::mWaypoints>( "waypoints" );
        mComponentTypes.Add<Light>();
        mComponentTypes.Add<Follow>();
    }
};

// /lamp; /props/chair with a light, and /props/chair/leg; /props/table.
struct Fixture : Types
{
    Document mDocument{ mComponentTypes };
    Clipboard mClipboard;
    Entity mLamp = Create( Root(), "lamp" );
    Entity mProps = Create( Root(), "props" );
    Entity mChair = Create( mProps, "chair" );
    Entity mLeg = Create( mChair, "leg" );
    Entity mTable = Create( mProps, "table" );

    Fixture()
    {
        Require( TryAddComponent( mDocument, mChair, "light"_n ) );
        Require( TrySetField( mDocument, mChair, "light"_n, Path( "brightness" ), MetaAny( 3.0f ) ) );
    }

    const Scene& GetScene() const { return mDocument.GetScene(); }
    Entity Root() const { return GetScene().Root(); }

    Entity Create( Entity parent, string_view name )
    {
        return Require( TryCreateEntity( mDocument, parent, Make( name ) ) );
    }

    void Follows( Entity holder, string_view target, vector<string_view> waypoints = {} )
    {
        Require( TryAddComponent( mDocument, holder, "follow"_n ) );
        Value::List list;
        for ( const string_view waypoint : waypoints )
            list.push_back( Link( waypoint ) );
        Require( TrySetField( mDocument, holder, "follow"_n, Path( "target" ), MetaAny( Link( target ) ) ) );
        Require( TrySetField( mDocument, holder, "follow"_n, Path( "waypoints" ), MetaAny( Value( list ) ) ) );
    }

    string PathText( Entity entity ) const { return PathOf( GetScene(), entity ).mPath.String(); }

    vector<string> Children( Entity entity ) const
    {
        vector<string> names;
        for ( const Entity child : ChildrenOf( GetScene(), entity ) )
            names.push_back( NameOf( GetScene(), child ).String() );
        return names;
    }

    vector<string> Waypoints( Entity holder ) const
    {
        vector<string> texts;
        for ( const EntityPath& waypoint : GetScene().Get<Follow>( holder ).mWaypoints )
            texts.push_back( waypoint.mPath.String() );
        return texts;
    }
};
}

TEST_CASE( "A copy pasted beside itself is named apart, and undo and redo take it away and bring the same back" )
{
    Fixture f;
    Require( f.mClipboard.TryCopy( f.mDocument, vector<Entity>{ f.mChair } ) );
    const vector<Entity> pasted = Require( TryPasteEntities( f.mDocument, f.mClipboard, f.mProps ) );
    REQUIRE( pasted.size() == 1 );
    const Entity copy = pasted.front();
    CHECK( f.PathText( copy ) == "/props/chair_2" );
    CHECK( f.Children( copy ) == vector<string>{ "leg" } );
    CHECK( f.GetScene().Get<Light>( copy ).mBrightness == 3.0f );
    CHECK( f.mDocument.GetHistory().UndoLabel() == "paste" );

    f.mDocument.GetHistory().Undo();
    CHECK_FALSE( f.GetScene().Alive( copy ) );
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "table" } );
    f.mDocument.GetHistory().Redo();
    CHECK( f.PathText( copy ) == "/props/chair_2" );
    CHECK( f.Children( copy ) == vector<string>{ "leg" } );
    CHECK( f.GetScene().Get<Light>( copy ).mBrightness == 3.0f );
}

TEST_CASE( "A copy outlives what it was copied from and pastes into another document" )
{
    Fixture f;
    Require( f.mClipboard.TryCopy( f.mDocument, vector<Entity>{ f.mChair, f.mTable } ) );
    Require( TryDestroyEntity( f.mDocument, f.mProps ) );

    Document other( f.mComponentTypes );
    const vector<Entity> pasted = Require( TryPasteEntities( other, f.mClipboard, other.GetScene().Root() ) );
    REQUIRE( pasted.size() == 2 );
    CHECK( PathOf( other.GetScene(), pasted[0] ).mPath.View() == "/chair" );
    CHECK( PathOf( other.GetScene(), pasted[1] ).mPath.View() == "/table" );
    CHECK( other.GetScene().Get<Light>( pasted[0] ).mBrightness == 3.0f );
    // The other document's history is its own.
    CHECK_FALSE( f.mDocument.GetHistory().UndoLabel() == "paste" );
}

TEST_CASE( "A paste at a place puts what it pastes there, in the clipboard's order" )
{
    Fixture f;
    Require( f.mClipboard.TryCopy( f.mDocument, vector<Entity>{ f.mTable, f.mLamp } ) );
    Require( TryPasteEntities( f.mDocument, f.mClipboard, f.mProps, 0 ) );
    CHECK( f.Children( f.mProps ) == vector<string>{ "table_2", "lamp", "chair", "table" } );
}

TEST_CASE( "Absolute links inside a copy lead into the copy, and every other link stays as it was" )
{
    Fixture f;
    f.Follows( f.mChair, "/props/chair/leg", { "/lamp", "leg", "/props/chair", "/props/chairs" } );
    const vector<Entity> copies = Require( TryDuplicateEntities( f.mDocument, vector<Entity>{ f.mChair } ) );
    REQUIRE( copies.size() == 1 );
    const Entity copy = copies.front();
    CHECK( f.GetScene().Get<Follow>( copy ).mTarget.mPath.View() == "/props/chair_2/leg" );
    CHECK( f.Waypoints( copy ) == vector<string>{ "/lamp", "leg", "/props/chair_2", "/props/chairs" } );
    // The original is untouched.
    CHECK( f.GetScene().Get<Follow>( f.mChair ).mTarget.mPath.View() == "/props/chair/leg" );
}

TEST_CASE( "Duplicates go right after their originals, and links among them lead among the copies" )
{
    Fixture f;
    f.Follows( f.mTable, "/props/chair" );
    const vector<Entity> copies = Require( TryDuplicateEntities( f.mDocument, vector<Entity>{ f.mChair, f.mTable } ) );
    REQUIRE( copies.size() == 2 );
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "chair_2", "table", "table_2" } );
    CHECK( f.GetScene().Get<Follow>( copies[1] ).mTarget.mPath.View() == "/props/chair_2" );
    CHECK( f.mDocument.GetHistory().UndoLabel() == "duplicate" );

    f.mDocument.GetHistory().Undo();
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "table" } );
    f.mDocument.GetHistory().Redo();
    CHECK( f.Children( f.mProps ) == vector<string>{ "chair", "chair_2", "table", "table_2" } );
    CHECK( f.PathText( copies[1] ) == "/props/table_2" );
}

TEST_CASE( "Copy and paste refuse what cannot be, and a failed copy keeps what was copied" )
{
    Fixture f;
    CHECK( TryPasteEntities( f.mDocument, f.mClipboard, f.mProps ).error() ==
           "nothing to paste: the clipboard is empty" );
    Require( f.mClipboard.TryCopy( f.mDocument, vector<Entity>{ f.mLamp } ) );
    CHECK( f.mClipboard.TryCopy( f.mDocument, vector<Entity>{ f.Root() } ).error() ==
           "the root is not copied: it is the scene; copy what is under it" );
    CHECK( f.mClipboard.All().size() == 1 );
    CHECK( f.mClipboard.All().front().mFrom.mPath.View() == "/lamp" );

    CHECK( TryDuplicateEntities( f.mDocument, vector<Entity>{ f.Root() } ).error() ==
           "the root is not duplicated: it is the scene" );
    Require( TryDestroyEntity( f.mDocument, f.mTable ) );
    CHECK( TryPasteEntities( f.mDocument, f.mClipboard, f.mTable )
               .error()
               .starts_with( "not an entity of the document" ) );
}
