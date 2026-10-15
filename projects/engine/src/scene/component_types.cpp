#include "bubble/scene/component_types.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
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

void ComponentTypes::AddType( const MetaType& type, Type made )
{
    if ( not type )
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
