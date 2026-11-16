# wgsl_reflect

What a WGSL module declares at module scope, read from its source: structs
with their members laid out by the uniform address space's rules, the
resources bound to groups, the functions by name, and the comment on a
member's or a binding's line for a caller's own annotations. WebGPU has no
reflection of its own, so whatever has to know a shader's layout - a bind
group, a material's uniform block, an inspector - works it out from the
source. It is not a WGSL front end: function bodies are skipped, and it
does not say whether a shader compiles.

```cpp
const wgsl_reflect::Module module = wgsl_reflect::Parse( source );
if ( const wgsl_reflect::Struct* material = module.FindStruct( "Material" ) )
    for ( const wgsl_reflect::StructMember& member : material->mMembers )
        Use( member.mName, member.mOffset, member.mType, member.mTrailingComment );
```

The standard library only. Written for the Bubble engine; MIT.
