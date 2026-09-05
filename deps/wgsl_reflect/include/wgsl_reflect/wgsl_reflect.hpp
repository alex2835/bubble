#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Reflection for WGSL module-scope declarations.
//
// WebGPU exposes no reflection of its own: wgpuShaderModuleGetCompilationInfo
// returns diagnostics and nothing else, Dawn's Tint inspector is not reachable
// through webgpu.h, and wgpu-native does not surface naga's. Anything that needs
// to know what a shader declares - to build a bind group, to lay out a uniform
// buffer, to drive an editor - has to work it out from the source.
//
// This parses only what is needed for that: structs, and the module-scope `var`
// declarations that carry @group/@binding. Function bodies, expressions and
// control flow are skipped wholesale. It is not a WGSL front end and will not
// tell you whether a shader compiles.
//
// Layout follows the WGSL uniform address space rules, so offsets computed here
// match what the GPU expects for a `var<uniform>` block.
namespace wgsl_reflect
{

enum class ScalarType
{
    Bool,
    I32,
    U32,
    F32,
    F16,
};

enum class TypeKind
{
    Unknown,
    Scalar,
    Vector,
    Matrix,
    Array,
    Struct,
    Texture,
    Sampler,
};

struct Type
{
    TypeKind mKind = TypeKind::Unknown;

    // Scalar, Vector, Matrix: the component type.
    ScalarType mScalar = ScalarType::F32;
    // Vector: 2, 3 or 4. Matrix: rows.
    uint32_t mRows = 0;
    // Matrix: columns.
    uint32_t mColumns = 0;

    // Struct: the struct's name. Array: the element type's spelling.
    std::string mName;
    // Array: element count. Zero means runtime-sized.
    uint32_t mArrayCount = 0;

    // Exactly as written in the source.
    std::string mSpelling;

    bool IsScalar() const { return mKind == TypeKind::Scalar; }
    bool IsHostShareable() const;
};


struct StructMember
{
    std::string mName;
    Type mType;

    // Byte offset and size in the uniform address space, with @align and @size
    // taken into account.
    uint32_t mOffset = 0;
    uint32_t mSize = 0;
    uint32_t mAlign = 0;

    std::optional<uint32_t> mExplicitAlign;
    std::optional<uint32_t> mExplicitSize;

    // Whatever followed the member on its own line, comment markers stripped.
    // Callers use this for their own annotations - the engine reads a
    // `@default(...)` out of it, since WGSL forbids an initializer on a uniform
    // and there is otherwise nowhere to put one.
    std::string mTrailingComment;
};


struct Struct
{
    std::string mName;
    std::vector<StructMember> mMembers;
    // Size and alignment in the uniform address space. Alignment is rounded up
    // to 16 there, which is why this can differ from the storage space.
    uint32_t mSize = 0;
    uint32_t mAlign = 0;

    const StructMember* FindMember( std::string_view name ) const;
};


enum class AddressSpace
{
    None,      // a handle: texture or sampler
    Uniform,
    Storage,
    Workgroup,
    Private,
};

struct Binding
{
    uint32_t mGroup = 0;
    uint32_t mBinding = 0;
    std::string mName;
    AddressSpace mAddressSpace = AddressSpace::None;
    Type mType;
};


struct Module
{
    std::vector<Struct> mStructs;
    std::vector<Binding> mBindings;
    // Things that could not be parsed. Not fatal - the rest of the module is
    // still reported - but a caller that cares about completeness should check.
    std::vector<std::string> mErrors;

    const Struct* FindStruct( std::string_view name ) const;
    const Binding* FindBinding( std::string_view name ) const;
    const Binding* FindBindingAt( uint32_t group, uint32_t binding ) const;
};


// Parses module-scope declarations out of WGSL source.
Module Parse( std::string_view source );

// Uniform address space layout, exposed for callers that build a type by hand.
uint32_t AlignOf( const Type& type, const Module& module );
uint32_t SizeOf( const Type& type, const Module& module );

std::string_view ToString( ScalarType type );
std::string_view ToString( TypeKind kind );
std::string_view ToString( AddressSpace space );

}
