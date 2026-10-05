#pragma once
#include "luaubind/common.hpp"

struct lua_State;

namespace luaubind
{
class LuaState;
struct LuaResume;
struct ScriptError;
template <typename K>
class LuaTableEntry;

enum class LuaKind
{
    Nil,
    Boolean,
    Number,
    String,
    Vector,
    Table,
    Function,
    Userdata,
    Thread,
    Other,
};

// A Luau value held from C++: any kind, kept alive by the VM while this
// lives. A copy is a second hold on the same value - a table copied is the
// same table. Empty is nil. Comes from a LuaState, and must not outlive it.
class LuaValue
{
public:
    LuaValue() = default;
    // Holds the value at stack slot `index` of L: for the binding layer.
    LuaValue( lua_State* L, int index );
    ~LuaValue();
    LuaValue( const LuaValue& other );
    LuaValue& operator=( const LuaValue& other );
    LuaValue( LuaValue&& other ) noexcept;
    LuaValue& operator=( LuaValue&& other ) noexcept;

    bool IsNil() const { return mRef <= 0; }
    LuaKind Kind() const;
    bool Is( LuaKind kind ) const { return Kind() == kind; }
    // Lua's own truth: everything but nil and false.
    bool Truthy() const;
    // As T, or nothing when it is not one: As<f64>(), As<string>(),
    // As<LuaTable>().
    template <typename T>
    opt<T> As() const;
    // As a person reads it in an error: nil, 42, "jump", table.
    string Describe() const;

    // The same value: the same table, function or thread, or an equal
    // number, string or boolean.
    friend bool operator==( const LuaValue& a, const LuaValue& b );

    // Pushes it onto `L` - the state's main thread or any of its threads.
    void Push( lua_State* L ) const;
    // The state it lives in; null for nil.
    LuaState* State() const;

private:
    void Release();

    // The main thread: a coroutine the value came from may be collected.
    lua_State* mL = nullptr;
    int mRef = -1;
};

// A table. Entries read and write like a C++ map - `table["speed"] = 5`,
// `table["speed"].As<f64>()` - and nest: `table["stats"]["hp"]`. Reading is
// always explicit: nothing converts to a C++ type behind your back.
class LuaTable : public LuaValue
{
public:
    LuaTable() = default;
    // The value if it is a table; nil otherwise.
    explicit LuaTable( LuaValue value );

    template <typename K>
    LuaTableEntry<K> operator[]( K key ) const;

    // table[key], through metatables, and the table's own entry only.
    template <typename K>
    LuaValue Get( const K& key ) const;
    template <typename K>
    LuaValue RawGet( const K& key ) const;
    // A lambda as the value becomes a Luau function. A frozen table refuses
    // with std::logic_error.
    template <typename K, typename V>
    void Set( const K& key, const V& value ) const;
    template <typename K, typename V>
    void RawSet( const K& key, const V& value ) const;

    // Every entry, taken when called: the loop may change the table.
    vector<std::pair<LuaValue, LuaValue>> Pairs() const;
    // #table: the length of the array part.
    int Length() const;
    template <typename V>
    void Append( const V& value ) const;

    void SetMetatable( const LuaTable& metatable ) const;
    // Read-only from here on, for scripts and C++ alike.
    void Freeze() const;
    bool Frozen() const;
    // A shallow copy.
    LuaTable Clone() const;
};

// A function: a Luau one or a bound C++ one. Calling it runs protected and
// gives back its first result, or the error with its traceback.
class LuaFunction : public LuaValue
{
public:
    LuaFunction() = default;
    // The value if it is a function; nil otherwise.
    explicit LuaFunction( LuaValue value );

    template <typename... Args>
    expected<LuaValue, ScriptError> operator()( const Args&... args ) const;
};

// A coroutine. Resume runs it until it yields or ends.
class LuaThread : public LuaValue
{
public:
    LuaThread() = default;
    // The value if it is a coroutine; nil otherwise.
    explicit LuaThread( LuaValue value );

    // `args` go to the function at the first resume, and back from the
    // yield after that.
    template <typename... Args>
    LuaResume Resume( const Args&... args ) const;
};

// table[key], before it is read or written: what LuaTable::operator[]
// gives. Short-lived - a statement's worth.
template <typename K>
class LuaTableEntry
{
public:
    LuaTableEntry( LuaTable table, K key ) : mTable( std::move( table ) ), mKey( std::move( key ) ) {}

    template <typename V>
    const LuaTableEntry& operator=( const V& value ) const
    {
        mTable.Set( mKey, value );
        return *this;
    }

    LuaValue Value() const { return mTable.Get( mKey ); }
    operator LuaValue() const { return Value(); }
    template <typename T>
    opt<T> As() const
    {
        return Value().template As<T>();
    }
    bool IsNil() const { return Value().IsNil(); }
    LuaKind Kind() const { return Value().Kind(); }
    bool Truthy() const { return Value().Truthy(); }

    // A nested entry: table["stats"]["hp"]. A missing or non-table level
    // reads as nil and refuses writes.
    template <typename K2>
    LuaTableEntry<K2> operator[]( K2 key ) const
    {
        return LuaTableEntry<K2>( LuaTable( Value() ), std::move( key ) );
    }

private:
    LuaTable mTable;
    K mKey;
};
}
