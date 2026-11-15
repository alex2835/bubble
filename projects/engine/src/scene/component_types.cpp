#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
template <typename T>
void ComponentTypes::AddInternal( string_view name )
{
    Type type = Make<T>();
    type.mName = *Name::TryFrom( name );
    mInternal.push_back( std::move( type ) );
}

ComponentTypes::ComponentTypes( Reflection& reflection ) : mReflection( reflection )
{
    AddInternal<PrefabInstance>( "prefab_instance" );
    AddInternal<PrefabLink>( "prefab_link" );
}

OptRef<const ComponentTypes::Type> ComponentTypes::TryFindAny( Name name ) const
{
    if ( const auto typeMaybe = TryFind( name ) )
        return typeMaybe;
    for ( const Type& type : mInternal )
        if ( type.mName == name )
            return type;
    return nullopt;
}

OptRef<const ComponentTypes::Type> ComponentTypes::TryFind( Name name ) const
{
    for ( const Type& type : mTypes )
        if ( type.mName == name )
            return type;
    return nullopt;
}

string ComponentTypes::Names() const
{
    string names;
    for ( const Type& type : mTypes )
        names += ( names.empty() ? "" : ", " ) + type.mName.ToString();
    return names;
}

expected<void, string> ComponentTypes::TrySetField( Scene& scene,
                                                    Entity entity,
                                                    const Type& type,
                                                    const FieldPath& path,
                                                    MetaAny value ) const
{
    MetaAny component = type.mGetWritable( scene, entity );
    if ( not component )
        return unexpected( format( "{} has no {}", Describe( scene, entity ), type.mName.ToString() ) );
    if ( auto setMaybe = mReflection.TrySetField( component, path, std::move( value ) ); not setMaybe )
        return setMaybe;
    if ( const Name* field = path.Steps().empty() ? nullptr : get_if<Name>( &path.Steps().front() ) )
        scene.NotifyFieldChanged( entity, type.mName, *field );
    return {};
}

void ComponentTypes::AddType( const MetaType& type, Type made )
{
    if ( not type or not mReflection.TryFind( Name::FromHash( type.id() ) ) )
        throw logic_error( "ComponentTypes::Add of a type reflection has not described" );
    const Name name = Name::FromHash( type.id() );
    if ( TryFind( name ) )
        throw logic_error( format( "ComponentTypes::Add of {} twice", name.ToString() ) );
    auto valueTypeMaybe = mReflection.TryValueTypeOf( type );
    if ( not valueTypeMaybe )
        throw logic_error( format( "ComponentTypes::Add of {}: {}", name.ToString(), valueTypeMaybe.error() ) );
    made.mName = name;
    made.mType = type;
    made.mValueType = std::move( *valueTypeMaybe );
    mTypes.push_back( std::move( made ) );
}
}
