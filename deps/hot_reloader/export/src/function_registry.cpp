
#include "details/function_registry.hpp"

namespace hr
{

FunctionRegistry& FunctionRegistry::Get()
{
   static FunctionRegistry registry;
   return registry;
}

const LibraryMeta& FunctionRegistry::GetInfo()
{
   return mFunctions;
}

}