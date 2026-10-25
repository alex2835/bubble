#include "links.hpp"
#include "bubble/scene/hierarchy.hpp"

namespace bubble
{
namespace
{
class LinkFinder
{
public:
    LinkFinder( const Scene& scene, const ComponentTypes& componentTypes, vector<EntityLink>& links )
        : mScene( scene ),
          mComponentTypes( componentTypes ),
          mLinks( links )
    {
    }

    void FindIn( Entity entity )
    {
        for ( const ComponentTypes::Type& type : mComponentTypes.All() )
        {
            // Most components hold no link: their type says so, and they are
            // not read at all.
            if ( not MayHold( type.mValueType, ValueKind::Entity ) )
                continue;
            const MetaAny component = type.mGet( mScene, entity );
            if ( not component )
                continue;
            // One that cannot be read cannot be saved either, and saving
            // says why.
            const auto valueMaybe = mComponentTypes.GetReflection().TryToValue( component );
            if ( valueMaybe )
                FindIn( *valueMaybe, entity, type.mName, FieldPath() );
        }
        for ( const Entity child : ChildrenOf( mScene, entity ) )
            FindIn( child );
    }

private:
    void FindIn( const Value& value, Entity holder, Name component, const FieldPath& path )
    {
        if ( const auto linkMaybe = value.TryAs<EntityPath>() )
        {
            const auto targetMaybe = TryResolve( mScene, *linkMaybe, holder );
            mLinks.push_back( EntityLink{ .mHolder = holder,
                                          .mComponent = component,
                                          .mPath = path,
                                          .mLink = *linkMaybe,
                                          .mTarget = targetMaybe ? *targetMaybe : Entity::Null } );
        }
        else if ( const auto listMaybe = value.TryAs<Value::List>() )
        {
            for ( size_t i = 0; i < listMaybe->size(); ++i )
                FindIn( ( *listMaybe )[i], holder, component, path.Then( i + 1 ) );
        }
        else if ( const auto recordMaybe = value.TryAs<Value::Record>() )
        {
            for ( const ValueField& field : *recordMaybe )
                FindIn( field.mValue, holder, component, path.Then( field.mName ) );
        }
    }

    const Scene& mScene;
    const ComponentTypes& mComponentTypes;
    vector<EntityLink>& mLinks;
};
}

vector<EntityLink> FindLinks( const Scene& scene, const ComponentTypes& componentTypes )
{
    vector<EntityLink> links;
    LinkFinder( scene, componentTypes, links ).FindIn( scene.Root() );
    return links;
}
}
