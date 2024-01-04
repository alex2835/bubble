#pragma once

#include <string_view>
#include <unordered_map>
#include <typeinfo>

#include "function_meta.hpp"

namespace hr
{

class FunctionRegistry
{
public:
   static FunctionRegistry& Get();
   const LibraryMeta& GetInfo();

   template <typename Ret, typename ...Args>
   void Registrate( std::string_view func_name  )
   {
      FunctionMeta export_function;
      export_function.mReturnType = typeid(Ret).name();
      export_function.mArgsTypes = { typeid(Args).name()... };
      mFunctions[ std::string( func_name ) ] = std::move( export_function );
   }

private:
   FunctionRegistry() = default;
   LibraryMeta mFunctions;
};

}