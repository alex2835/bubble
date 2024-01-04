
#include "import.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace hr;
using namespace std::chrono_literals;

void main()
{
   try
   {
      hr::HotReloader hr( "HotReloaderTestLib" );

      for( const auto& [name, func] : hr.GetLibraryMeta() )
            std::cout << "name:" << name << " signature:" << func.ToString() << std::endl;
      auto mul = hr.GetFunction<int(int, int)>( "mul" );

      while( true )
      {
         std::cout << "2 + 2 = " << std::to_string( mul(2, 2) ) << std::endl;

         try
         {
            if( hr.TryUpdate() )
            {
               std::cout << "Load new version" << std::endl;
               mul = hr.GetFunction<int(int, int)>( "mul" );
            }
         }
         catch(const std::exception& e)
         {
            std::cerr << e.what() << std::endl;
         }
         std::this_thread::sleep_for( 1s );
      }
   }
   catch(const std::exception& e)
   {
      std::cerr << e.what() << '\n';
   }
}