#pragma once

#include <string>
#include <unordered_map>
#include <type_traits>
#include <filesystem>
#include <thread>
#include <memory>

#include <dynalo/dynalo.hpp>
#include "details/function.hpp"
#include "details/function_meta.hpp"

namespace hr
{
namespace fs = std::filesystem;
using namespace std::chrono_literals;
typedef void (*FunctionPointer)(void);
typedef std::unordered_map<std::string, FunctionPointer> FunctionsCache;

const fs::path CACHE_DIR = "HR_CACHE";
const auto RELOAD_DELAY = 100ms;
const auto CHECK_DELAY = 1000ms;


class HotReloader
{
public:
   /*
    *  @throw 1) If dll can't be loaded
    *         2) If Ssignature is different to previous version
    */
   HotReloader( fs::path path );
   ~HotReloader();

   /*
    * @brief Check if new version of lib is available and try load it 
    * @return return true if new version was loaded
    * @throw std::exception (Copy lib error, Loading error, Signatures changes error)
    */
   bool TryUpdate();


   /*
    * @brief Give a function wrapper by name
    * @throw If no function with such signature 
    */
   template <typename FunctionSignature>
   FunctionSignature* GetFunctionRaw( std::string name )
   {
      name = "HR_" + name;
      if( !mLibraryMeta.count( name ) )
         throw std::runtime_error( "No such function: " + name );

      const auto expected = Function<FunctionSignature>::SignatureToString();
      const auto& existed = mLibraryMeta[name].ToString();
      if( expected != existed )
         throw std::runtime_error( "Signature of function: " + name + " is different"
                                   "\nExpected: " + expected +
                                   "\nLibrary provides: " + existed );

      if( mFunctionCache.count( name ) )
         return reinterpret_cast<FunctionSignature*>( mFunctionCache[name] );
      else
      {
         auto func = GetActiveLibrary().get_function<FunctionSignature>( name );
         mFunctionCache[name] = reinterpret_cast<FunctionPointer>( func );
         return func;
      }
   }

   /*
    * @brief Give a function wrapper by name
    * @throw If no function with such signature
    */
   template <typename FunctionSignature>
   Function<FunctionSignature> GetFunction( std::string name )
   {
      return Function<FunctionSignature>( GetFunctionRaw<FunctionSignature>( name ), GetLibraryVersion() );
   }

   /**
    * @brief Information about all functions
    *        provided by this lib 
    * @return unordered_map<Name, Function>
    *         where Function{ Ret, std::vector<Arg> } 
    */
   const LibraryMeta& GetLibraryMeta()
   {
      return mLibraryMeta;
   }


private:
   fs::path GetInputPath();
   fs::path GetOutputPath();
   fs::path GetCacheDir();
   int GetLibraryVersion();
   dynalo::library& GetActiveLibrary();
   const LibraryMeta& ExtractLibraryMeta( dynalo::library& lib );
   void UpdateMeta( dynalo::library& lib );

   bool mNeedUpdate = true;
   bool mStopUpdateChecker = false;
   std::thread mUpdateChecker;
   fs::file_time_type mLastUpdateTime;

   std::vector<dynalo::library> mLibraryVersions;
   std::string mLibraryName;
   fs::path mLibraryPath;

   // Library functions and their types
   LibraryMeta mLibraryMeta;
   // Function pointers cache
   FunctionsCache mFunctionCache;

};

}