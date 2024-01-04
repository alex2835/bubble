
#include "hot_reloader_export.hpp"


DYNALO_EXPORT LibraryMeta DYNALO_CALL HR_GetModuleInfo()
{
   return hr::FunctionRegistry::Get().GetInfo();
}
