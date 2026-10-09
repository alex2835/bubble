#include "bubble/scripts/script_runtime.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/utility.hpp"

// Script code can create and destroy instances while it runs, so nothing
// here keeps a reference to an entry across a call into Luau: before the
// call the handle is looked up, after it the handle is looked up again.
namespace bubble
{
namespace
{
// "a, b, c", or "none".
string Join( const vector<string>& names )
{
    string list;
    for ( const string& name : names )
        list += ( list.empty() ? "" : ", " ) + name;
    return list.empty() ? "none" : list;
}

// "a number", "a table": a kind as an error message says it.
string_view KindName( LuaKind kind )
{
    switch ( kind )
    {
        case LuaKind::Nil: return "nil";
        case LuaKind::Boolean: return "a boolean";
        case LuaKind::Number: return "a number";
        case LuaKind::String: return "a string";
        case LuaKind::Vector: return "a vector";
        case LuaKind::Table: return "a table";
        case LuaKind::Function: return "a function";
        case LuaKind::Userdata: return "a userdata";
        case LuaKind::Thread: return "a thread";
        case LuaKind::Other: break;
    }
    return "a value";
}
}

// ---- construction --------------------------------------------------------

ScriptRuntime::CurrentScope::CurrentScope( ScriptRuntime& runtime, ScriptInstanceHandle instance )
    : mRuntime( runtime ),
      mPrevious( runtime.mCurrent )
{
    mRuntime.mCurrent = instance;
}

ScriptRuntime::CurrentScope::~CurrentScope()
{
    mRuntime.mCurrent = mPrevious;
}

ScriptRuntime::ScriptRuntime( LuaState& lua, AssetRegistry& assets, vector<string> callbacks )
    : mLua( lua ),
      mAssets( assets ),
      mCallbacks( std::move( callbacks ) )
{
    if ( lua.Sealed() )
        throw logic_error( "ScriptRuntime adds globals: build it before the state is sealed" );
    mPathOf = mLua.NewWeakKeyTable();
    mPropsOf = mLua.NewWeakKeyTable();
    mLocalsOf = mLua.NewWeakKeyTable();
    mLibraryOf = mLua.NewWeakKeyTable();
    mRequiresOf = mLua.NewWeakKeyTable();

    mOpenEnvironment = mLua.NewTable();
    mOpenEnvironment["__index"] = mLua.Globals();
    mOpenEnvironment.Freeze();
    mStrictEnvironment = mLua.NewTable();
    mStrictEnvironment["__index"] = mLua.Globals();
    mStrictEnvironment["__newindex"] = []( const LuaValue&, const LuaValue& key ) {
        throw LuaError( format( "assignment to undeclared global '{}': declare it at the top of the file, "
                                "or keep it in self",
                                key.As<string>().value_or( key.Describe() ) ) );
    };
    mStrictEnvironment.Freeze();

    RegisterGlobals();
    mListener = mAssets.OnChanged( [this]( const AssetEntryBase& entry ) { Changed( entry ); } );
}

ScriptRuntime::~ScriptRuntime()
{
    mAssets.RemoveListener( mListener );
}

void ScriptRuntime::RegisterGlobals()
{
    LuaTable globals = mLua.Globals();

    globals["props"] = [this]( const LuaTable& props ) {
        const LuaTable file = mLua.CallerEnvironment();
        if ( mLibraryOf[file].Truthy() )
            throw LuaError( "props belong to entity scripts; a library shares through what it returns" );
        if ( not mPropsOf[file].IsNil() )
            throw LuaError( "props is declared once, at the top of the file" );
        mPropsOf[file] = props;
    };

    globals["locals"] = [this]( const LuaTable& locals ) {
        const LuaTable file = mLua.CallerEnvironment();
        if ( mLibraryOf[file].Truthy() )
            throw LuaError( "locals belong to entity scripts; a library keeps its state in its own locals" );
        if ( not mLocalsOf[file].IsNil() )
            throw LuaError( "locals is declared once, at the top of the file" );
        mLocalsOf[file] = locals;
    };

    // The hints are for the inspector and are read on import; at run time
    // a prop is its default.
    globals["prop"] = []( const LuaValue& value, const LuaValue& hints ) {
        if ( not hints.IsNil() and not hints.Is( LuaKind::Table ) )
            throw LuaError( "prop( default, hints ): the hints are a table" );
        return value;
    };

    globals["require"] = [this]( const string& request ) {
        auto value = Require( mLua.CallerEnvironment(), request );
        if ( not value )
            throw LuaError( value.error() );
        return *value;
    };

    globals["start"] = [this]( const LuaFunction& fn, const LuaRest& args ) { Start( Current( "start" ), fn, args ); };

    globals["wait"] = [this]( opt<f64> seconds ) {
        if ( not mLua.Yieldable() )
            throw LuaError( "wait works inside start( fn ), not in a callback" );
        return LuaYield{ mLua.Value( seconds.value_or( 0.0 ) ) };
    };

    globals["wait_until"] = [this]( const LuaFunction& condition ) {
        if ( not mLua.Yieldable() )
            throw LuaError( "wait_until works inside start( fn ), not in a callback" );
        return LuaYield{ condition };
    };

    globals["on"] = [this]( const string& event, const LuaFunction& fn ) {
        const ScriptInstanceHandle instance = Current( "on" );
        mEvents[event].push_back( Subscriber{ instance, fn } );
        mInstances.Get( instance )->mEvents.insert( event );
    };

    globals["emit"] = [this]( const string& event, const LuaRest& args ) { EmitWith( event, args ); };
}

ScriptInstanceHandle ScriptRuntime::Current( const char* what ) const
{
    if ( not mInstances.Alive( mCurrent ) )
        throw LuaError( format( "{} works in an entity's callbacks, not at the top of a file", what ) );
    return mCurrent;
}

expected<void, string> ScriptRuntime::SetAlias( string_view name, string_view directory )
{
    const bool named = not name.empty() and ranges::all_of( name, []( char c ) {
        return ( c >= 'a' and c <= 'z' ) or ( c >= '0' and c <= '9' ) or c == '_' or c == '-';
    } );
    if ( not named )
        return unexpected( format( "alias '{}': lowercase letters, digits, _ and - only", name ) );
    string path;
    if ( not directory.empty() )
    {
        auto valid = AssetPath::From( directory );
        if ( not valid )
            return unexpected( format( "alias @{}: {}", name, valid.error() ) );
        path = valid->String();
    }
    mAliases[string( name )] = std::move( path );
    return {};
}

// ---- files -----------------------------------------------------------------

LuaTable ScriptRuntime::NewFileEnvironment( string_view path )
{
    // The file's own globals, reading through to the engine's; what props,
    // locals and require need to know of the file, by its environment.
    LuaTable env = mLua.NewTable();
    env.SetMetatable( mOpenEnvironment );
    mPathOf[env] = path;
    return env;
}

expected<LuaValue, ScriptError> ScriptRuntime::RunFile( string_view path, const AssetRef<ScriptAsset>& asset,
                                                        const LuaTable& env )
{
    if ( not mLua.Sealed() )
        return unexpected( ScriptError{ "scripts load once the Luau state is sealed", {} } );
    if ( not asset.Ready() )
        return unexpected( ScriptError{ format( "{} is not loaded", path ), {} } );

    auto chunk = mLua.Load( path, asset.Get()->mBytecode, env );
    if ( not chunk )
        return unexpected( ScriptError{ std::move( chunk.error() ), {} } );
    mRunning.emplace_back( path );
    auto ran = ( *chunk )();
    mRunning.pop_back();
    // From here on a new global is a mistake.
    if ( ran )
        env.SetMetatable( mStrictEnvironment );
    return ran;
}

LuaTable ScriptRuntime::PassportOf( LuaTable& passport, const LuaTable& env )
{
    if ( auto kept = passport[env].As<LuaTable>() )
        return *kept;
    // For props and locals an empty table kept also makes a later
    // declaration an error.
    LuaTable none = mLua.NewTable();
    passport[env] = none;
    return none;
}

expected<ScriptRuntime::ScriptFile, ScriptError> ScriptRuntime::RunScript( string_view path,
                                                                           const AssetRef<ScriptAsset>& asset )
{
    const auto fail = [&]( string message ) -> expected<ScriptFile, ScriptError> {
        return unexpected( ScriptError{ format( "{}: {}", path, message ), {} } );
    };
    ScriptFile file;
    file.mEnvironment = NewFileEnvironment( path );
    const LuaTable env = file.mEnvironment;
    if ( auto ran = RunFile( path, asset, env ); not ran )
        return unexpected( std::move( ran.error() ) );

    file.mProps = PassportOf( mPropsOf, env );
    if ( auto checked = CheckProps( file.mProps ); not checked )
        return fail( checked.error() );
    file.mLocals = PassportOf( mLocalsOf, env );
    if ( auto checked = CheckLocals( file.mLocals, file.mProps ); not checked )
        return fail( checked.error() );

    for ( const string& name : mCallbacks )
    {
        const LuaValue callback = env.RawGet( name );
        if ( not callback.IsNil() and not callback.Is( LuaKind::Function ) )
            return fail( format( "{} is {}, not a function", name, KindName( callback.Kind() ) ) );
        file.mCallbacks.emplace_back( callback );
    }

    // on_updte would never run; say so instead of staying silent.
    for ( const auto& [key, value] : env.Pairs() )
    {
        const auto name = key.As<string>();
        if ( name and name->starts_with( "on_" ) and value.Is( LuaKind::Function ) and
             ranges::find( mCallbacks, *name ) == mCallbacks.end() )
            LogWarning( "{}: {} is not a callback the engine calls ({})", path, *name, Join( mCallbacks ) );
    }
    return file;
}

expected<ScriptRuntime::LibraryFile, ScriptError> ScriptRuntime::RunLibrary( string_view path,
                                                                             const AssetRef<ScriptAsset>& asset )
{
    const auto fail = [&]( string message ) -> expected<LibraryFile, ScriptError> {
        return unexpected( ScriptError{ format( "{}: {}", path, message ), {} } );
    };
    LibraryFile file;
    file.mEnvironment = NewFileEnvironment( path );
    const LuaTable env = file.mEnvironment;
    // Marked before it runs, so props and locals refuse it at their line.
    mLibraryOf[env] = true;
    auto ran = RunFile( path, asset, env );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );

    if ( ran->IsNil() )
        return fail( "a library returns what it shares - end it with return" );
    for ( const string& name : mCallbacks )
        if ( not env.RawGet( name ).IsNil() )
            return fail( format( "{} is a callback of entity scripts; a library only returns what it shares", name ) );
    file.mResult = std::move( *ran );
    return file;
}

expected<void, string> ScriptRuntime::CheckProps( const LuaTable& props )
{
    for ( const auto& [key, value] : props.Pairs() )
    {
        const auto name = key.As<string>();
        if ( not name )
            return unexpected( "props are named, " + key.Describe() + " is not a name" );
        if ( *name == "entity" )
            return unexpected( "a prop cannot be called 'entity': self.entity is the engine's"s );
        if ( auto encoded = mLua.Encode( value ); not encoded )
            return unexpected( format( "prop {}: {}", *name, encoded.error() ) );
    }
    return {};
}

// Locals are not saved, so any value will do; what matters is that self
// has one meaning for each name.
expected<void, string> ScriptRuntime::CheckLocals( const LuaTable& locals, const LuaTable& props )
{
    for ( const auto& [key, value] : locals.Pairs() )
    {
        const auto name = key.As<string>();
        if ( not name )
            return unexpected( "locals are named, " + key.Describe() + " is not a name" );
        if ( *name == "entity" )
            return unexpected( "a local cannot be called 'entity': self.entity is the engine's"s );
        if ( not props.RawGet( key ).IsNil() )
            return unexpected( format( "{} is both a prop and a local; self has one of each name", *name ) );
    }
    return {};
}

expected<void, string> ScriptRuntime::CopyInto( const LuaTable& self, const LuaTable& from, bool onlyMissing )
{
    for ( const auto& [key, value] : from.Pairs() )
    {
        if ( onlyMissing and not self.RawGet( key ).IsNil() )
            continue;
        auto copy = mLua.DeepCopy( value );
        if ( not copy )
            return unexpected( std::move( copy.error() ) );
        self.RawSet( key, *copy );
    }
    return {};
}

// ---- require ---------------------------------------------------------------

expected<string, string> ScriptRuntime::Resolve( string_view from, string_view request ) const
{
    string joined;
    if ( request.starts_with( "./" ) or request.starts_with( "../" ) )
    {
        const size_t slash = from.rfind( '/' );
        const string_view directory = slash == string_view::npos ? string_view() : from.substr( 0, slash );
        joined = string( directory ) + "/" + string( request );
    }
    else if ( request.starts_with( '@' ) )
    {
        const size_t slash = request.find( '/' );
        const string_view alias = request.substr( 1, slash == string_view::npos ? string_view::npos : slash - 1 );
        const auto found = mAliases.find( alias );
        if ( found == mAliases.end() )
        {
            vector<string> known;
            for ( const auto& entry : mAliases )
                known.push_back( "@" + entry.first );
            ranges::sort( known );
            return unexpected( format( "no alias @{} (aliases: {})", alias, Join( known ) ) );
        }
        joined = found->second + ( slash == string_view::npos ? "" : string( request.substr( slash ) ) );
    }
    else
        return unexpected( "a path starts with ./, ../ or @alias"s );

    // "." steps go, ".." takes one back.
    vector<string_view> steps;
    const string_view all = joined;
    for ( size_t at = 0; at <= all.size(); )
    {
        const size_t end = min( all.find( '/', at ), all.size() );
        const string_view step = all.substr( at, end - at );
        at = end + 1;
        if ( step.empty() or step == "." )
            continue;
        if ( step == ".." )
        {
            if ( steps.empty() )
                return unexpected( "the path leads out of the project"s );
            steps.pop_back();
        }
        else
            steps.push_back( step );
    }
    string path;
    for ( const string_view step : steps )
        path += ( path.empty() ? "" : "/" ) + string( step );
    path += ".luau";
    if ( auto valid = AssetPath::From( path ); not valid )
        return unexpected( valid.error() );
    return path;
}

expected<LuaValue, string> ScriptRuntime::Require( const LuaTable& file, const string& request )
{
    const auto from = mPathOf[file].As<string>();
    if ( not from )
        return unexpected( "require works in script files"s );
    auto path = Resolve( *from, request );
    if ( not path )
        return unexpected( format( "require( '{}' ): {}", request, path.error() ) );
    auto found = mLibraries.find( *path );
    if ( found == mLibraries.end() )
    {
        // First require of this file in the world: the registry has it
        // loaded, or the world was started without it.
        AssetRef<ScriptAsset> asset = mAssets.Find<ScriptAsset>( *AssetPath::From( *path ) );
        if ( not asset.Ready() )
            return unexpected( format( "require( '{}' ): no library {} is loaded", request, *path ) );
        found = mLibraries.emplace( *path, Library{ std::move( asset ), {}, {} } ).first;
    }
    // Recorded by this run of the file: a run that fails takes it away.
    PassportOf( mRequiresOf, file ).RawSet( *path, true );
    if ( not found->second.mResult.IsNil() )
        return found->second.mResult;

    if ( const auto running = ranges::find( mRunning, *path ); running != mRunning.end() )
    {
        string chain;
        for ( auto file = running; file != mRunning.end(); ++file )
            chain += *file + " -> ";
        return unexpected( "require cycle: " + chain + *path );
    }

    // A copy of the handle keeps the bytecode while it runs.
    const AssetRef<ScriptAsset> asset = found->second.mAsset;
    auto ran = RunLibrary( *path, asset );
    if ( not ran )
    {
        const ScriptError& error = ran.error();
        return unexpected( error.mTraceback.empty() ? error.mMessage : error.mMessage + "\n" + error.mTraceback );
    }
    Library& library = mLibraries.find( *path )->second;
    library.mResult = std::move( ran->mResult );
    library.mEnvironment = std::move( ran->mEnvironment );
    return library.mResult;
}

// ---- scripts ---------------------------------------------------------------

expected<ScriptHandle, ScriptError> ScriptRuntime::Load( string_view path )
{
    if ( const auto known = mScriptsByPath.find( path ); known != mScriptsByPath.end() )
        return known->second;
    auto valid = AssetPath::From( path );
    if ( not valid )
        return unexpected( ScriptError{ format( "{}: {}", path, valid.error() ), {} } );
    AssetRef<ScriptAsset> asset = mAssets.Find<ScriptAsset>( *valid );

    auto ran = RunScript( valid->View(), asset );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );
    const ScriptHandle handle =
        mScripts.Add( Script{ valid->String(), std::move( asset ), std::move( ran->mEnvironment ),
                              std::move( ran->mProps ), std::move( ran->mLocals ), std::move( ran->mCallbacks ) } );
    mScriptsByPath[valid->String()] = handle;
    return handle;
}

expected<void, ScriptError> ScriptRuntime::Rerun( ScriptHandle handle )
{
    // Copies held through the run: it may load and unload scripts.
    const string chunk = mScripts.Get( handle )->mChunk;
    const AssetRef<ScriptAsset> asset = mScripts.Get( handle )->mAsset;
    auto ran = RunScript( chunk, asset );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );
    auto script = mScripts.Get( handle );
    if ( not script )
        return unexpected( ScriptError{ chunk + " was unloaded while it ran", {} } );
    script->mEnvironment = std::move( ran->mEnvironment );
    script->mProps = std::move( ran->mProps );
    script->mLocals = std::move( ran->mLocals );
    script->mCallbacks = std::move( ran->mCallbacks );

    // Instances keep self, gain the new props and locals, and come back on.
    const LuaTable props = script->mProps;
    const LuaTable locals = script->mLocals;
    for ( const ScriptInstanceHandle instanceHandle : mInstances.Handles() )
    {
        auto instance = mInstances.Get( instanceHandle );
        if ( instance->mScript != handle )
            continue;
        // Props copy as Create copied them, a failure there would have failed
        // the run; a copy fails only on tables nested past the limit, and
        // then the instance goes on without the new value.
        (void)CopyInto( instance->mSelf, props, true );
        (void)CopyInto( instance->mSelf, locals, true );
        instance->mEnabled = true;
    }
    return {};
}

bool ScriptRuntime::Requires( const LuaTable& env, const hset<string>& libraries )
{
    // A library dropped after a change has no environment until it runs
    // again, and is to run again anyway.
    if ( env.IsNil() )
        return false;
    const auto required = mRequiresOf[env].As<LuaTable>();
    if ( not required )
        return false;
    return ranges::any_of( required->Pairs(), [&]( const auto& entry ) {
        return libraries.contains( entry.first.template As<string>().value_or( "" ) );
    } );
}

void ScriptRuntime::Changed( const AssetEntryBase& entry )
{
    const string& path = entry.Path().String();
    // Everything that holds a value of the old file: the library itself,
    // the libraries whose code required it, theirs in turn.
    hset<string> changed;
    if ( mLibraries.contains( path ) )
        changed.insert( path );
    for ( bool grew = not changed.empty(); grew; )
    {
        grew = false;
        for ( const auto& [file, library] : mLibraries )
            if ( not changed.contains( file ) and Requires( library.mEnvironment, changed ) )
                grew = changed.insert( file ).second;
    }
    for ( const string& library : changed )
    {
        Library& dropped = mLibraries.find( library )->second;
        dropped.mResult = {};
        dropped.mEnvironment = {};
    }

    // Then the scripts: the changed one, and those that required any of
    // the changed libraries. A failure is logged and leaves the old code.
    for ( const ScriptHandle handle : mScripts.Handles() )
    {
        // Another script's run may have unloaded this one.
        const auto script = mScripts.Get( handle );
        if ( not script or ( script->mChunk != path and not Requires( script->mEnvironment, changed ) ) )
            continue;
        if ( auto rerun = Rerun( handle ); not rerun )
            LogError( "{}\n{}", rerun.error().mMessage, rerun.error().mTraceback );
    }
}

void ScriptRuntime::Unload( ScriptHandle handle )
{
    if ( const auto script = mScripts.Get( handle ) )
        mScriptsByPath.erase( script->mChunk );
    for ( const ScriptInstanceHandle instance : mInstances.Handles() )
        if ( mInstances.Get( instance )->mScript == handle )
            Destroy( instance );
    mScripts.Remove( handle );
}

bool ScriptRuntime::Has( ScriptHandle handle, u32 callback ) const
{
    const auto script = mScripts.Get( handle );
    return script and callback < script->mCallbacks.size() and not script->mCallbacks[callback].IsNil();
}

LuaTable ScriptRuntime::Props( ScriptHandle handle ) const
{
    const auto script = mScripts.Get( handle );
    return script ? script->mProps : LuaTable();
}

// ---- instances -------------------------------------------------------------

expected<ScriptInstanceHandle, string> ScriptRuntime::Create( ScriptHandle scriptHandle, string label,
                                                              const LuaTable& overrides )
{
    const auto script = mScripts.Get( scriptHandle );
    if ( not script )
        return unexpected( label + ": its script was unloaded" );
    const LuaTable props = script->mProps;
    const LuaTable locals = script->mLocals;
    const string chunk = script->mChunk;

    // Each instance gets its own copy: a table among the props or locals is
    // not shared between entities.
    LuaTable self = mLua.NewTable();
    if ( auto copied = CopyInto( self, props, false ); not copied )
        return unexpected( label + ": " + copied.error() );
    if ( auto copied = CopyInto( self, locals, false ); not copied )
        return unexpected( label + ": " + copied.error() );

    for ( const auto& [key, value] : overrides.Pairs() )
    {
        const LuaValue fallback = props.RawGet( key );
        if ( not locals.RawGet( key ).IsNil() )
            return unexpected( format( "{}: {} is a local of {}: it starts the same in every instance, the scene "
                                       "cannot set it",
                                       label, key.Describe(), chunk ) );
        if ( fallback.IsNil() or not key.Is( LuaKind::String ) )
        {
            vector<string> names;
            for ( const auto& entry : props.Pairs() )
                names.push_back( entry.first.As<string>().value_or( "?" ) );
            ranges::sort( names );
            return unexpected(
                format( "{}: {} is not a prop of {} (props: {})", label, key.Describe(), chunk, Join( names ) ) );
        }
        if ( fallback.Kind() != value.Kind() )
            return unexpected( format( "{}: prop {} is {} in {}, the override is {}", label, *key.As<string>(),
                                       KindName( fallback.Kind() ), chunk, KindName( value.Kind() ) ) );
        auto copy = mLua.DeepCopy( value );
        if ( not copy )
            return unexpected( label + ": " + copy.error() );
        self.RawSet( key, *copy );
    }

    return mInstances.Add( Instance{ scriptHandle, std::move( label ), std::move( self ), {}, {}, true } );
}

void ScriptRuntime::Destroy( ScriptInstanceHandle handle )
{
    const auto instance = mInstances.Get( handle );
    if ( not instance )
        return;
    for ( const string& event : instance->mEvents )
        erase_if( mEvents[event], [&]( const Subscriber& subscriber ) { return subscriber.mInstance == handle; } );
    // Its coroutines and self go with it; a callback of its own still
    // running keeps them on the stack until it returns.
    mInstances.Remove( handle );
}

bool ScriptRuntime::Alive( ScriptInstanceHandle handle ) const
{
    return mInstances.Alive( handle );
}

bool ScriptRuntime::Enabled( ScriptInstanceHandle handle ) const
{
    const auto instance = mInstances.Get( handle );
    return instance and instance->mEnabled;
}

LuaTable ScriptRuntime::Self( ScriptInstanceHandle handle ) const
{
    const auto instance = mInstances.Get( handle );
    return instance ? instance->mSelf : LuaTable();
}

void ScriptRuntime::Fail( ScriptInstanceHandle handle, const ScriptError& error )
{
    const auto instance = mInstances.Get( handle );
    if ( not instance )
        return;
    LogError( "{}: {}\n{}", instance->mLabel, error.mMessage, error.mTraceback );
    instance->mEnabled = false;
    // Its coroutines stop with it; a changed file starts it over.
    instance->mTasks.clear();
}

bool ScriptRuntime::CallWith( ScriptInstanceHandle handle, u32 callback, const LuaRest& args )
{
    const auto instance = mInstances.Get( handle );
    if ( not instance )
        return false;
    if ( not instance->mEnabled or not Has( instance->mScript, callback ) )
        return true;
    const LuaFunction fn = mScripts.Get( instance->mScript )->mCallbacks[callback];
    const LuaTable self = instance->mSelf;

    expected<LuaValue, ScriptError> called;
    {
        const CurrentScope scope( *this, handle );
        called = fn( self, args );
    }
    if ( not called )
    {
        Fail( handle, called.error() );
        return false;
    }
    return true;
}

// ---- events ----------------------------------------------------------------

void ScriptRuntime::EmitWith( string_view event, const LuaRest& args )
{
    const auto found = mEvents.find( event );
    if ( found == mEvents.end() )
        return;
    // A copy: subscribing or leaving during the calls changes the list,
    // not this pass over it.
    const vector<Subscriber> subscribers = found->second;
    for ( const Subscriber& subscriber : subscribers )
    {
        if ( not Enabled( subscriber.mInstance ) )
            continue;
        expected<LuaValue, ScriptError> called;
        {
            const CurrentScope scope( *this, subscriber.mInstance );
            called = subscriber.mFunction( Self( subscriber.mInstance ), args );
        }
        if ( not called )
            Fail( subscriber.mInstance, called.error() );
    }
}

// ---- coroutines ------------------------------------------------------------

void ScriptRuntime::Start( ScriptInstanceHandle handle, const LuaFunction& fn, const LuaRest& args )
{
    const LuaThread thread = mLua.NewThread( fn );
    mInstances.Get( handle )->mTasks.push_back( Task{ thread, {}, false } );
    // To its first wait, at once.
    Resume( handle, thread, args );
}

void ScriptRuntime::Resume( ScriptInstanceHandle handle, const LuaThread& thread, const LuaRest& args )
{
    LuaResume resumed;
    {
        const CurrentScope scope( *this, handle );
        resumed = thread.Resume( args );
    }
    if ( resumed.mStatus == LuaResume::Status::Failed )
    {
        Fail( handle, resumed.mError );
        return;
    }
    // Found again by its thread: the coroutine may have started others,
    // or destroyed its own instance.
    const auto instance = mInstances.Get( handle );
    if ( not instance )
        return;
    for ( Task& task : instance->mTasks )
        if ( task.mThread == thread )
        {
            task.mWait = resumed.mWait;
            task.mDone = resumed.mStatus == LuaResume::Status::Finished;
        }
}

void ScriptRuntime::TickInstance( ScriptInstanceHandle handle, f32 dt )
{
    if ( not Enabled( handle ) )
        return;
    // Coroutines started during this tick have run already.
    const size_t count = mInstances.Get( handle )->mTasks.size();
    for ( size_t i = 0; i < count and Enabled( handle ); ++i )
    {
        const auto instance = mInstances.Get( handle );
        if ( i >= instance->mTasks.size() )
            break;
        const Task task = instance->mTasks[i];
        if ( task.mDone )
            continue;

        bool ready = true;
        if ( const auto seconds = task.mWait.As<f64>() )
        {
            const f64 left = *seconds - dt;
            ready = left <= 0;
            if ( not ready )
                instance->mTasks[i].mWait = mLua.Value( left );
        }
        else if ( const auto condition = task.mWait.As<LuaFunction>() )
        {
            expected<LuaValue, ScriptError> polled;
            {
                const CurrentScope scope( *this, handle );
                polled = ( *condition )();
            }
            if ( not polled )
            {
                Fail( handle, polled.error() );
                break;
            }
            ready = polled->Truthy();
        }
        if ( ready )
            Resume( handle, task.mThread, {} );
    }
    if ( const auto instance = mInstances.Get( handle ) )
        erase_if( instance->mTasks, []( const Task& task ) { return task.mDone; } );
}

void ScriptRuntime::Tick( f32 dt )
{
    BUBBLE_PROFILE_ZONE();
    // An instance made during the tick waits for the next one.
    for ( const ScriptInstanceHandle handle : mInstances.Handles() )
        TickInstance( handle, dt );
}
}
