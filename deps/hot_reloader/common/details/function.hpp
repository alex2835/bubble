

namespace hr
{

class HotReloader;

template<typename S>
class Function;

template<typename Ret, typename... Args>
class Function<Ret(Args...)>
{
using FunctionType = Ret(Args...);

public:
   Ret operator() ( Args&&... args )
   {
      return mFunction( std::forward<Args>( args )... );
   }

   Function( FunctionType* func, int version )
      : mFunction( func ),
        mLibVersion( version )
   {}

   int GetLibVersion()
   {
      return mLibVersion;
   }

   static std::string SignatureToString()
   {
      std::string func_signature;
      auto ret = typeid(Ret).name();
      auto args = std::vector<std::string>{ typeid(Args).name()... };

      func_signature += ret;
      func_signature += " (";
      int i = 0;
      for( const auto& arg : args )
         func_signature += ( i++ ? ", " : "" ) + arg;

      func_signature += ")";
      return func_signature;
   }

private:
   FunctionType* mFunction;
   int mLibVersion;
};


}