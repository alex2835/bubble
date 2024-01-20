//
//#include "scene/components.h"
//#include <ranges>
//
//namespace Bubble
//{
//inline nlohmann::json to_json( const vec3& vec )
//{
//    return nlohmann::json{ vec.r, vec.g, vec.b };
//}
//inline vec3 from_json_vec3( const nlohmann::json& j )
//{
//    return vec3( j[0], j[1], j[2] );
//}
//static inline nlohmann::json to_json( const vec4& vec )
//{
//    return nlohmann::json{ vec.r, vec.g, vec.b, vec.a };
//}
//static inline vec4 from_json_vec4( const nlohmann::json& j )
//{
//    return vec4( j[0], j[1], j[2], j[3] );
//}
//static inline bool json_exists( const nlohmann::json& j, const string& key )
//{
//    return j.find( key ) != j.end();
//}
//
//void TagComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    mTag = j["Tag"];
//}
//
//void TagComponent::Serialize( const Loader& loader, nlohmann::json& j ) const
//{
//    j["Tag"] = mTag;
//}
//
//TagComponent::TagComponent( const string& tag )
//    : mTag( tag )
//{
//    mTag.reserve( 256 );
//}
//
//TagComponent::TagComponent()
//{
//    mTag.reserve( 256 );
//}
//
//PositionComponent::PositionComponent( const vec3& mPosition )
//    : mPosition( mPosition )
//{
//}
//
//void PositionComponent::Serialize( const Loader& loader, nlohmann::json& j ) const
//{
//    j["Position"] = to_json( mPosition );
//}
//
//void PositionComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    mPosition = from_json_vec3( j["Position"] );
//}
//
//RotationComponent::RotationComponent( const vec3& rotation )
//    : mRotation( rotation )
//{
//}
//
//void RotationComponent::Serialize( const Loader& loader, nlohmann::json& j ) const
//{
//    j["Rotation"] = to_json( mRotation );
//}
//
//void RotationComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    mRotation = from_json_vec3( j["Rotation"] );
//}
//
//ScaleComponent::ScaleComponent( const vec3& scale )
//    : mScale( scale )
//{
//}
//
//void ScaleComponent::Serialize( const Loader&, nlohmann::json& j ) const
//{
//    j["Scale"] = to_json( mScale );
//}
//
//void ScaleComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    mScale = from_json_vec3( j["Scale"] );
//}
//
//TransformComponent::TransformComponent( mat4 transform )
//    : mTransform( transform )
//{
//}
//
//void TransformComponent::Serialize( const Loader& loader, nlohmann::json& j ) const
//{
//    vector<f32> temp;
//    for ( i32 i = 0; i < 4; i++ )
//    {
//        for ( i32 j = 0; j < 4; j++ )
//        {
//            temp.push_back( mTransform[i][j] );
//        }
//    }
//    j["Transform"] = temp;
//}
//
//void TransformComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    vector<f32> temp = j["Transform"];
//    for ( i32 i = 0; i < 4; i++ )
//    {
//        for ( i32 j = 0; j < 4; j++ )
//        {
//            mTransform[i][j] = temp[i * 4 + j];
//        }
//    }
//}
//
//TransformComponent::operator const mat4& ( ) const
//{
//    return mTransform;
//}
//
//TransformComponent::operator mat4& ( )
//{
//    return mTransform;
//}
//
//void LightComponent::Serialize( const Loader& loader, nlohmann::json& j ) const
//{
//    j["Light"]["Type"] = Type;
//    j["Light"]["Brightness"] = Brightness;
//    j["Light"]["Distance"] = Distance;
//
//    j["Light"]["Constant"] = Constant;
//    j["Light"]["Linear"] = Linear;
//    j["Light"]["Quadratic"] = Quadratic;
//
//    j["Light"]["CutOff"] = CutOff;
//    j["Light"]["OuterCutOff"] = OuterCutOff;
//
//    j["Light"]["Position"] = to_json( Position );
//    j["Light"]["Color"] = to_json( Color );
//    j["Light"]["Direction"] = to_json( Direction );
//}
//
//void LightComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    Type = j["Light"]["Type"];
//    Brightness = j["Light"]["Brightness"];
//    Distance = j["Light"]["Distance"];
//
//    Constant = j["Light"]["Constant"];
//    Linear = j["Light"]["Linear"];
//    Quadratic = j["Light"]["Quadratic"];
//
//    CutOff = j["Light"]["CutOff"];
//    OuterCutOff = j["Light"]["OuterCutOff"];
//
//    Position = from_json_vec3( j["Light"]["Position"] );
//    Direction = from_json_vec3( j["Light"]["Direction"] );
//    Color = from_json_vec3( j["Light"]["Color"] );
//}
//
//ModelComponent::ModelComponent( const Ref<Model>& model )
//    : Ref<Model>( model )
//{
//}
//
//ModelComponent::operator Ref<Model>& ( )
//{
//    return *this;
//}
//
//ModelComponent::operator const Ref<Model>& ( ) const
//{
//    return *this;
//}
//
//void ModelComponent::Serialize( const Loader& loader, nlohmann::json& j ) const
//{
//    auto iterator = std::ranges::find_if( loader.mLoadedModels,
//                                          [&]( const auto& path_model )
//    {
//        return path_model.second == *this;
//    } );
//
//    const string& path = iterator->first;
//    j["Model"] = path;
//}
//
//void ModelComponent::Deserialize( const nlohmann::json& j, Loader& loader )
//{
//    *this = loader.LoadAndCacheModel( j["Model"] );
//}
//
//}