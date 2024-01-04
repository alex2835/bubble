#pragma once

#define DYNALO_EXPORT_SYMBOLS
#include <dynalo/symbol_helper.hpp>

#include <boost/preprocessor/variadic.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/tuple.hpp>
#include <boost/preprocessor/variadic.hpp>

#include "details/function_registry.hpp"

namespace hr
{

template< typename Ret, typename ...Args>
class Registrator
{
public:
   Registrator( std::string_view function_name )
   { 
      FunctionRegistry::Get().Registrate<Ret, Args...>( function_name );
   }
};

}

#ifdef _MSC_VER // Microsoft compilers
#   define __GET_ARG_COUNT(...)  INTERNAL_EXPAND_ARGS_PRIVATE(INTERNAL_ARGS_AUGMENTER(__VA_ARGS__))
#   define INTERNAL_ARGS_AUGMENTER(...) unused, __VA_ARGS__
#   define INTERNAL_EXPAND(x) x
#   define INTERNAL_EXPAND_ARGS_PRIVATE(...) INTERNAL_EXPAND(INTERNAL_GET_ARG_COUNT_PRIVATE(__VA_ARGS__, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#   define INTERNAL_GET_ARG_COUNT_PRIVATE(_1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10_, _11_, _12_, _13_, _14_, _15_, _16_, _17_, _18_, _19_, _20_, _21_, _22_, _23_, _24_, _25_, _26_, _27_, _28_, _29_, _30_, _31_, _32_, _33_, _34_, _35_, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, count, ...) count
#else // Non-Microsoft compilers
#   define __GET_ARG_COUNT(...) INTERNAL_GET_ARG_COUNT_PRIVATE(0, ## __VA_ARGS__, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#   define INTERNAL_GET_ARG_COUNT_PRIVATE(_0, _1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10_, _11_, _12_, _13_, _14_, _15_, _16_, _17_, _18_, _19_, _20_, _21_, _22_, _23_, _24_, _25_, _26_, _27_, _28_, _29_, _30_, _31_, _32_, _33_, _34_, _35_, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, _70, count, ...) count
#endif

#define __ARG16(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define __HAS_COMMA(...) __ARG16(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define __TRIGGER_PARENTHESIS_(...) ,
#define __PASTE5(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4
#define __IS_EMPTY_CASE_0001 ,
#define __IS_EMPTY(_0, _1, _2, _3) __HAS_COMMA(__PASTE5(__IS_EMPTY_CASE_, _0, _1, _2, _3))

#define TUPLE_IS_EMPTY(...) \
    __IS_EMPTY( \
        /* test if there is just one argument, eventually an empty one */ \
        __HAS_COMMA(__VA_ARGS__), \
        /* test if _TRIGGER_PARENTHESIS_ together with the argument adds a comma */ \
        __HAS_COMMA(__TRIGGER_PARENTHESIS_ __VA_ARGS__),                 \
        /* test if the argument together with a parenthesis adds a comma */ \
        __HAS_COMMA(__VA_ARGS__ (/*empty*/)), \
        /* test if placing it between _TRIGGER_PARENTHESIS_ and the parenthesis adds a comma */ \
        __HAS_COMMA(__TRIGGER_PARENTHESIS_ __VA_ARGS__ (/*empty*/)) \
    )

#define __GEN_EMPTY_ARGS(...) BOOST_PP_EMPTY()

#define __GEN_NONEMPTY_ARGS_CB(unused, data, idx, elem) \
    BOOST_PP_COMMA_IF(idx) elem arg##idx

#define __GEN_NONEMPTY_ARGS(seq) \
    BOOST_PP_SEQ_FOR_EACH_I( \
         __GEN_NONEMPTY_ARGS_CB \
        ,~ \
        ,seq \
    )


#define __GEN_NONEMPTY_FUNCTION_ARGS_CB(unused, data, idx, elem) \
    BOOST_PP_COMMA_IF(idx) arg##idx

#define __GEN_NONEMPTY_FUNCTION_ARGS(seq) \
    BOOST_PP_SEQ_FOR_EACH_I( \
         __GEN_NONEMPTY_FUNCTION_ARGS_CB \
        ,~ \
        ,seq \
    )


#define __GEN_REGISTRATOR_ARGS(ret, ...) \
   BOOST_PP_IF( TUPLE_IS_EMPTY(__VA_ARGS__), \
               ret, \
               ret BOOST_PP_COMMA __VA_ARGS__ )



#define HR_REGISTER_FUNC(ret, name, ...)                             \
    DYNALO_EXPORT ret DYNALO_CALL BOOST_PP_CAT( HR_, name )(           \
        BOOST_PP_IF(                                                   \
             __GET_ARG_COUNT(__VA_ARGS__)                              \
            ,__GEN_NONEMPTY_ARGS                                       \
            ,__GEN_EMPTY_ARGS                                          \
        )(BOOST_PP_TUPLE_TO_SEQ((__VA_ARGS__)))                        \
    ) {                                                                \
      return name ( BOOST_PP_IF(                                       \
             __GET_ARG_COUNT(__VA_ARGS__)                              \
            , __GEN_NONEMPTY_FUNCTION_ARGS                             \
            ,__GEN_EMPTY_ARGS                                          \
        )(BOOST_PP_TUPLE_TO_SEQ((__VA_ARGS__))) );                     \
    }                                                                  \
hr::Registrator<ret, __VA_ARGS__> BOOST_PP_CAT( registrator_, name )( std::string_view( BOOST_PP_CAT( "HR_", #name ) ) );