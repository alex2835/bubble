
#include "details/function_registry.hpp"
#include "hot_reloader_export.hpp"

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

const LibraryMeta& HR_GetModuleInfo()
{
    return hr::FunctionRegistry::Get().GetInfo();
}
