#pragma once 
#include <vector>
#include <string>
#include <unordered_map>

namespace hr
{

struct FunctionMeta
{
   std::string mReturnType;
   std::vector<std::string> mArgsTypes;

   std::string ToString() const
   {
      std::string func_signature;
      func_signature += mReturnType + " (";

      int i = 0;
      for( const auto& arg : mArgsTypes )
         func_signature += ( i++ ? ", " : "" ) + arg;
         
      func_signature += ")";
      return func_signature;
   }
};

}

typedef std::unordered_map<std::string, hr::FunctionMeta> LibraryMeta;
