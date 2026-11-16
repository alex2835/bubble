#include "bubble/import/script_importer.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/scripts/lua.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <Luau/Ast.h>
#include <Luau/BuiltinDefinitions.h>
#include <Luau/Frontend.h>
#include <Luau/Parser.h>
#include <Luau/ToString.h>

namespace bubble
{
namespace
{
// "scripts/player.luau:3: " for a node's place, lines from 1.
string At( string_view path, const Luau::Location& location )
{
    return format( "{}:{}: ", path, location.begin.line + 1 );
}

string StringOf( const Luau::AstExprConstantString& constant )
{
    return string( constant.value.data, constant.value.size );
}

bool IsGlobal( const Luau::AstExpr* expr, string_view name )
{
    const auto* global = expr->as<Luau::AstExprGlobal>();
    return global and string_view( global->name.value ) == name;
}

// vector.create, vector.zero: a field of the global table `vector`.
bool IsVectorField( const Luau::AstExpr* expr, string_view field )
{
    const auto* index = expr->as<Luau::AstExprIndexName>();
    return index and IsGlobal( index->expr, "vector" ) and string_view( index->index.value ) == field;
}

opt<f64> TryNumber( const Luau::AstExpr* expr )
{
    if ( const auto* number = expr->as<Luau::AstExprConstantNumber>() )
        return number->value;
    if ( const auto* unary = expr->as<Luau::AstExprUnary>(); unary and unary->op == Luau::AstExprUnary::Op::Minus )
        if ( const auto* number = unary->expr->as<Luau::AstExprConstantNumber>() )
            return -number->value;
    return nullopt;
}

// What a prop's default is: a literal the inspector can show, its type
// with it.
class PropReader
{
public:
    PropReader( string_view path, const ScriptCheck& check ) : mPath( path ), mCheck( check ) {}

    expected<vector<ScriptProp>, string> TryRead( const Luau::AstExprTable& table )
    {
        vector<ScriptProp> props;
        for ( const Luau::AstExprTable::Item& item : table.items )
        {
            const auto* key = item.key ? item.key->as<Luau::AstExprConstantString>() : nullptr;
            if ( item.kind != Luau::AstExprTable::Item::Kind::Record or not key )
                return unexpected( At( mPath, item.value->location ) + "props are named: props { speed = 40 }" );
            const string name = StringOf( *key );
            const auto nameMaybe = Name::TryFrom( name );
            if ( not nameMaybe )
                return unexpected( At( mPath, key->location ) + nameMaybe.error() );
            ScriptProp prop{ .mName = *nameMaybe, .mType = {}, .mDefault = {}, .mMin = 0, .mMax = 0, .mTooltip = {} };
            const Luau::AstExpr* value = item.value;
            if ( const auto* call = value->as<Luau::AstExprCall>(); call and IsGlobal( call->func, "prop" ) )
            {
                if ( call->args.size == 0 or call->args.size > 2 )
                    return unexpected( At( mPath, call->location ) + "prop( default, hints )" );
                if ( call->args.size == 2 )
                    if ( auto hintsMaybe = TryHints( *call->args.data[1], prop ); not hintsMaybe )
                        return unexpected( std::move( hintsMaybe.error() ) );
                value = call->args.data[0];
            }
            auto literalMaybe = TryLiteral( *value, name );
            if ( not literalMaybe )
                return unexpected( std::move( literalMaybe.error() ) );
            prop.mDefault = std::move( literalMaybe->first );
            prop.mType = std::move( literalMaybe->second );
            props.push_back( std::move( prop ) );
        }
        return props;
    }

private:
    using Literal = pair<Value, ValueType>;

    expected<void, string> TryHints( const Luau::AstExpr& expr, ScriptProp& prop )
    {
        const auto* table = expr.as<Luau::AstExprTable>();
        if ( not table )
            return unexpected( At( mPath, expr.location ) + "prop's hints are a table: { min = 0, max = 10 }" );
        for ( const Luau::AstExprTable::Item& item : table->items )
        {
            const auto* key = item.key ? item.key->as<Luau::AstExprConstantString>() : nullptr;
            const string name = key ? StringOf( *key ) : string();
            const auto numberMaybe = TryNumber( item.value );
            const auto* text = item.value->as<Luau::AstExprConstantString>();
            if ( name == "min" and numberMaybe )
                prop.mMin = static_cast<f32>( *numberMaybe );
            else if ( name == "max" and numberMaybe )
                prop.mMax = static_cast<f32>( *numberMaybe );
            else if ( name == "tooltip" and text )
                prop.mTooltip = StringOf( *text );
            else
                return unexpected( At( mPath, item.value->location ) +
                                   "prop's hints: min and max, numbers, and tooltip, a string" );
        }
        return {};
    }

    expected<Literal, string> TryLiteral( const Luau::AstExpr& expr, const string& where )
    {
        const auto refuse = [&] {
            return unexpected(
                At( mPath, expr.location ) +
                format( "props: {}: a literal the inspector can show - a number, string or boolean, "
                        "vector.create( x, y, z ), color( r, g, b ), asset( \"path\" ), entity(), a table of them",
                        where ) );
        };
        if ( const auto* boolean = expr.as<Luau::AstExprConstantBool>() )
            return Literal( Value( boolean->value ), ValueType::Of( ValueKind::Bool ) );
        if ( const auto numberMaybe = TryNumber( &expr ) )
            return Literal( Value( *numberMaybe ), ValueType::Of( ValueKind::Number ) );
        if ( const auto* text = expr.as<Luau::AstExprConstantString>() )
        {
            auto textMaybe = Text::TryFrom( StringOf( *text ) );
            if ( not textMaybe )
                return unexpected( At( mPath, expr.location ) + textMaybe.error() );
            return Literal( Value( std::move( *textMaybe ) ), ValueType::Of( ValueKind::Text ) );
        }
        if ( IsVectorField( &expr, "zero" ) or IsVectorField( &expr, "one" ) )
            return Literal( Value( vec3( IsVectorField( &expr, "one" ) ? 1.0f : 0.0f ) ),
                            ValueType::Of( ValueKind::Vec3 ) );
        if ( const auto* call = expr.as<Luau::AstExprCall>() )
            return TryCall( *call, where, refuse );
        if ( const auto* table = expr.as<Luau::AstExprTable>() )
            return TryTable( *table, nullopt, where );
        if ( const auto* assertion = expr.as<Luau::AstExprTypeAssertion>() )
        {
            auto typeMaybe = TryType( *assertion->annotation );
            if ( not typeMaybe )
                return unexpected( std::move( typeMaybe.error() ) );
            const auto* table = assertion->expr->as<Luau::AstExprTable>();
            if ( typeMaybe->mKind != ValueKind::List or not table )
                return refuse();
            return TryTable( *table, *typeMaybe->mElement, where );
        }
        return refuse();
    }

    template <typename Refuse>
    expected<Literal, string> TryCall( const Luau::AstExprCall& call, const string& where, const Refuse& refuse )
    {
        vector<f64> numbers;
        for ( const Luau::AstExpr* arg : call.args )
            if ( const auto numberMaybe = TryNumber( arg ) )
                numbers.push_back( *numberMaybe );
        const bool allNumbers = numbers.size() == call.args.size;
        if ( IsVectorField( call.func, "create" ) and allNumbers and numbers.size() == 3 )
            return Literal( Value( vec3( static_cast<f32>( numbers[0] ), static_cast<f32>( numbers[1] ),
                                         static_cast<f32>( numbers[2] ) ) ),
                            ValueType::Of( ValueKind::Vec3 ) );
        if ( IsGlobal( call.func, "color" ) and allNumbers and ( numbers.size() == 3 or numbers.size() == 4 ) )
            return Literal( Value( Color{ .mR = static_cast<f32>( numbers[0] ),
                                          .mG = static_cast<f32>( numbers[1] ),
                                          .mB = static_cast<f32>( numbers[2] ),
                                          .mA = numbers.size() == 4 ? static_cast<f32>( numbers[3] ) : 1.0f } ),
                            ValueType::Of( ValueKind::Color ) );
        if ( IsGlobal( call.func, "entity" ) and call.args.size == 0 )
            return Literal( Value( EntityPath{} ), ValueType::Of( ValueKind::Entity ) );
        if ( IsGlobal( call.func, "asset" ) and call.args.size <= 1 )
        {
            AssetId assetId;
            if ( call.args.size == 1 )
            {
                const auto* path = call.args.data[0]->as<Luau::AstExprConstantString>();
                if ( not path )
                    return refuse();
                const auto validMaybe = AssetPath::TryFrom( StringOf( *path ) );
                if ( not validMaybe )
                    return unexpected( At( mPath, path->location ) + validMaybe.error() );
                // One not there yet is the run's to report, with the line.
                if ( mCheck.mAssetOf )
                    assetId = mCheck.mAssetOf( *validMaybe );
            }
            return Literal( Value( assetId ), ValueType::Of( ValueKind::Asset ) );
        }
        (void)where;
        return refuse();
    }

    // A record of named fields, or a list of one type - the first
    // element's, or what a type assertion says.
    expected<Literal, string> TryTable( const Luau::AstExprTable& table, opt<ValueType> element, const string& where )
    {
        const bool record =
            table.items.size != 0 and table.items.data[0].kind == Luau::AstExprTable::Item::Kind::Record;
        if ( record )
        {
            Value::Record fields;
            vector<ValueTypeField> types;
            for ( const Luau::AstExprTable::Item& item : table.items )
            {
                const auto* key = item.key ? item.key->as<Luau::AstExprConstantString>() : nullptr;
                if ( item.kind != Luau::AstExprTable::Item::Kind::Record or not key )
                    return unexpected( At( mPath, item.value->location ) +
                                       format( "props: {}: a table is a record of named fields or a list", where ) );
                const auto nameMaybe = Name::TryFrom( StringOf( *key ) );
                if ( not nameMaybe )
                    return unexpected( At( mPath, key->location ) + nameMaybe.error() );
                auto fieldMaybe = TryLiteral( *item.value, where + "." + StringOf( *key ) );
                if ( not fieldMaybe )
                    return fieldMaybe;
                fields.push_back( ValueField{ *nameMaybe, std::move( fieldMaybe->first ) } );
                types.push_back( ValueTypeField{ *nameMaybe, std::move( fieldMaybe->second ) } );
            }
            return Literal( Value( std::move( fields ) ), ValueType::RecordOf( std::move( types ) ) );
        }
        if ( table.items.size == 0 and not element )
            return unexpected( At( mPath, table.location ) +
                               format( "props: {}: an empty list says what it holds: {{}} :: {{ vector }}", where ) );
        Value::List list;
        for ( size_t i = 0; i < table.items.size; ++i )
        {
            const Luau::AstExprTable::Item& item = table.items.data[i];
            const string at = format( "{}[{}]", where, i + 1 );
            if ( item.kind != Luau::AstExprTable::Item::Kind::List )
                return unexpected( At( mPath, item.value->location ) +
                                   format( "props: {}: a table is a record of named fields or a list", where ) );
            auto elementMaybe = TryLiteral( *item.value, at );
            if ( not elementMaybe )
                return elementMaybe;
            if ( not element )
                element = elementMaybe->second;
            else if ( element->Describe() != elementMaybe->second.Describe() )
                return unexpected( At( mPath, item.value->location ) +
                                   format( "props: {}: {} in a list whose first element is {}", at,
                                           elementMaybe->second.Describe(), element->Describe() ) );
            list.push_back( std::move( elementMaybe->first ) );
        }
        return Literal( Value( std::move( list ) ), ValueType::ListOf( std::move( *element ) ) );
    }

    // The type an annotation names, for an empty list's elements.
    expected<ValueType, string> TryType( const Luau::AstType& type )
    {
        if ( const auto* reference = type.as<Luau::AstTypeReference>(); reference and not reference->prefix )
        {
            static const array<pair<string_view, ValueKind>, 7> cKinds{
                pair<string_view, ValueKind>{ "number", ValueKind::Number },
                pair<string_view, ValueKind>{ "string", ValueKind::Text },
                pair<string_view, ValueKind>{ "boolean", ValueKind::Bool },
                pair<string_view, ValueKind>{ "vector", ValueKind::Vec3 },
                pair<string_view, ValueKind>{ "color", ValueKind::Color },
                pair<string_view, ValueKind>{ "asset", ValueKind::Asset },
                pair<string_view, ValueKind>{ "entity", ValueKind::Entity },
            };
            for ( const auto& [name, kind] : cKinds )
                if ( string_view( reference->name.value ) == name )
                    return ValueType::Of( kind );
        }
        if ( const auto* table = type.as<Luau::AstTypeTable>() )
        {
            if ( table->indexer and table->props.size == 0 )
            {
                auto elementMaybe = TryType( *table->indexer->resultType );
                if ( not elementMaybe )
                    return elementMaybe;
                return ValueType::ListOf( std::move( *elementMaybe ) );
            }
            vector<ValueTypeField> fields;
            for ( const Luau::AstTableProp& prop : table->props )
            {
                auto fieldMaybe = TryType( *prop.type );
                const auto nameMaybe = Name::TryFrom( prop.name.value );
                if ( not fieldMaybe or not nameMaybe )
                    return unexpected( fieldMaybe ? At( mPath, prop.location ) + nameMaybe.error()
                                                  : fieldMaybe.error() );
                fields.push_back( ValueTypeField{ *nameMaybe, std::move( *fieldMaybe ) } );
            }
            if ( not table->indexer )
                return ValueType::RecordOf( std::move( fields ) );
        }
        return unexpected( At( mPath, type.location ) +
                           "a prop's type is number, string, boolean, vector, color, asset, entity, a list { T } or a "
                           "record of them" );
    }

    string_view mPath;
    const ScriptCheck& mCheck;
};

// What import reads of a file without running it: the props table, and
// every require's path - a string literal, resolved from the file.
class SourceReader : public Luau::AstVisitor
{
public:
    SourceReader( string_view path, const ScriptCheck& check ) : mPath( path ), mCheck( check ) {}

    expected<void, string> TryRead( Luau::AstStatBlock& root, ScriptAsset& asset )
    {
        for ( Luau::AstStat* stat : root.body )
        {
            const Luau::AstExprCall* call = nullptr;
            if ( const auto* expr = stat->as<Luau::AstStatExpr>() )
                call = expr->expr->as<Luau::AstExprCall>();
            else if ( const auto* local = stat->as<Luau::AstStatLocal>(); local and local->values.size == 1 )
                call = local->values.data[0]->as<Luau::AstExprCall>();
            if ( not call or not IsGlobal( call->func, "props" ) )
                continue;
            const auto* table = call->args.size == 1 ? call->args.data[0]->as<Luau::AstExprTable>() : nullptr;
            if ( not table )
                return unexpected( At( mPath, call->location ) +
                                   "props takes a table literal: the inspector reads it without running the file" );
            auto propsMaybe = PropReader( mPath, mCheck ).TryRead( *table );
            if ( not propsMaybe )
                return unexpected( std::move( propsMaybe.error() ) );
            asset.mProps = std::move( *propsMaybe );
        }
        root.visit( this );
        if ( mError )
            return unexpected( std::move( *mError ) );
        asset.mRequires = std::move( mRequires );
        return {};
    }

    bool visit( Luau::AstExprCall* call ) override
    {
        if ( mError or not IsGlobal( call->func, "require" ) )
            return true;
        // Unchecked, a require that names nothing is the run's to report,
        // at its line; checked, it is the import's.
        const bool checked = not mCheck.mDeclarations.empty();
        const auto* path = call->args.size == 1 ? call->args.data[0]->as<Luau::AstExprConstantString>() : nullptr;
        if ( not path )
        {
            if ( checked )
                mError = At( mPath, call->location ) + "require takes a path as a string literal: require( \"./x\" )";
            return true;
        }
        auto resolvedMaybe = TryResolveRequire( mPath, StringOf( *path ), mCheck.mAliases );
        if ( not resolvedMaybe )
        {
            if ( checked )
                mError = At( mPath, path->location ) +
                         format( "require( \"{}\" ): {}", StringOf( *path ), resolvedMaybe.error() );
        }
        else if ( ranges::find( mRequires, *resolvedMaybe ) == mRequires.end() )
            mRequires.push_back( std::move( *resolvedMaybe ) );
        return true;
    }

private:
    string_view mPath;
    const ScriptCheck& mCheck;
    vector<string> mRequires;
    opt<string> mError;
};

// What the analyzer does not see: `any` written, callbacks with
// parameters unannotated.
class TypingRules : public Luau::AstVisitor
{
public:
    explicit TypingRules( string_view path ) : mPath( path ) {}

    expected<void, string> TryCheck( Luau::AstStatBlock& root )
    {
        for ( Luau::AstStat* stat : root.body )
        {
            const auto* function = stat->as<Luau::AstStatFunction>();
            const auto* global = function ? function->name->as<Luau::AstExprGlobal>() : nullptr;
            if ( not global or
                 ranges::find( cScriptCallbacks, string_view( global->name.value ) ) == cScriptCallbacks.end() )
                continue;
            for ( const Luau::AstLocal* arg : function->func->args )
                if ( not arg->annotation )
                    return unexpected( At( mPath, arg->location ) +
                                       format( "{}: annotate its parameters - {}( self: Self, dt: number ), "
                                               "with type Self = typeof( P ) & {{ entity: entity }}",
                                               global->name.value, global->name.value ) );
        }
        root.visit( this );
        if ( mError )
            return unexpected( std::move( *mError ) );
        return {};
    }

    bool visit( Luau::AstTypeReference* type ) override
    {
        if ( not mError and not type->prefix and string_view( type->name.value ) == "any" )
            mError = At( mPath, type->location ) + "any: write the type the value has - scripts are typed";
        return true;
    }

private:
    string_view mPath;
    opt<string> mError;
};

// The project's files as the analyzer reads them: the one importing from
// its source, the others from the project; requires resolved as the
// runtime resolves them.
class ProjectSources final : public Luau::FileResolver
{
public:
    ProjectSources( const ScriptCheck& check, string_view path, string_view source )
        : mCheck( check ),
          mPath( path ),
          mSource( source )
    {
    }

    std::optional<Luau::SourceCode> readSource( const Luau::ModuleName& name ) override
    {
        if ( name == mPath )
            return Luau::SourceCode{ string( mSource ), Luau::SourceCode::Module };
        const auto pathMaybe = AssetPath::TryFrom( name );
        if ( not pathMaybe or not mCheck.mRead )
            return std::nullopt;
        auto textMaybe = mCheck.mRead( *pathMaybe );
        if ( not textMaybe )
            return std::nullopt;
        return Luau::SourceCode{ std::move( *textMaybe ), Luau::SourceCode::Module };
    }

    std::optional<Luau::ModuleInfo> resolveModule( const Luau::ModuleInfo* context,
                                                   Luau::AstExpr* expr,
                                                   const Luau::TypeCheckLimits& ) override
    {
        const auto* path = expr->as<Luau::AstExprConstantString>();
        if ( not context or not path )
            return std::nullopt;
        auto resolvedMaybe = TryResolveRequire( context->name, StringOf( *path ), mCheck.mAliases );
        if ( not resolvedMaybe )
            return std::nullopt;
        return Luau::ModuleInfo{ std::move( *resolvedMaybe ), false };
    }

private:
    const ScriptCheck& mCheck;
    string mPath;
    string_view mSource;
};

// Strict, whatever a file says.
class StrictConfig final : public Luau::ConfigResolver
{
public:
    StrictConfig() { mConfig.mode = Luau::Mode::Strict; }
    const Luau::Config& getConfig( const Luau::ModuleName&, const Luau::TypeCheckLimits& ) const override
    {
        return mConfig;
    }

private:
    Luau::Config mConfig;
};

expected<void, string> TryCheckTypes( const ScriptCheck& check, string_view path, string_view source )
{
    ProjectSources sources( check, path, source );
    StrictConfig config;
    Luau::Frontend frontend( &sources, &config );
    Luau::registerBuiltinGlobals( frontend, frontend.globals );
    const Luau::LoadDefinitionFileResult loaded = frontend.loadDefinitionFile(
        frontend.globals, frontend.globals.globalScope, check.mDeclarations, "@bubble", false, false );
    if ( not loaded.success )
    {
        // The engine wrote them: a mistake there is the engine's.
        string why;
        for ( const Luau::ParseError& error : loaded.parseResult.errors )
            why += format( "\n{}: {}", error.getLocation().begin.line + 1, error.getMessage() );
        if ( loaded.module )
            for ( const Luau::TypeError& error : loaded.module->errors )
                why += format( "\n{}: {}", error.location.begin.line + 1, Luau::toString( error ) );
        throw logic_error( "bubble.d.luau does not load:" + why );
    }
    Luau::freeze( frontend.globals.globalTypes );

    const Luau::CheckResult result = frontend.check( string( path ) );
    string errors;
    for ( const Luau::TypeError& error : result.errors )
        // A library's own errors are its import's to report.
        if ( error.moduleName == path )
            errors += ( errors.empty() ? "" : "\n" ) + At( path, error.location ) + Luau::toString( error );
    if ( not errors.empty() )
        return unexpected( std::move( errors ) );
    return {};
}

expected<string, string> TryImportScript( const ScriptCheck& check, string_view source, const AssetPath& path )
{
    const string where = path.String();
    // progress changes as the game runs: read each time, never once at load.
    auto bytecodeMaybe =
        TryCompileScript( source, CompileOptions{ .mOptimization = 1, .mMutableGlobals = { "progress" } } );
    if ( not bytecodeMaybe )
        // Luau's message starts with the line: ":3: Expected ...".
        return unexpected( where + bytecodeMaybe.error() );

    Luau::Allocator allocator;
    Luau::AstNameTable names( allocator );
    Luau::ParseOptions options;
    options.captureComments = true;
    const Luau::ParseResult parsed = Luau::Parser::parse( source.data(), source.size(), names, allocator, options );
    if ( not parsed.errors.empty() or not parsed.root )
        return unexpected( where + ": does not parse" );

    ScriptAsset asset{ .mBytecode = std::move( *bytecodeMaybe ), .mProps = {}, .mRequires = {} };
    if ( auto readMaybe = SourceReader( where, check ).TryRead( *parsed.root, asset ); not readMaybe )
        return unexpected( std::move( readMaybe.error() ) );

    if ( not check.mDeclarations.empty() )
    {
        for ( const Luau::HotComment& comment : parsed.hotcomments )
            if ( comment.header and ( comment.content == "nonstrict" or comment.content == "nocheck" ) )
                return unexpected( At( where, comment.location ) +
                                   format( "--!{}: every script is checked strictly", comment.content ) );
        if ( auto rulesMaybe = TypingRules( where ).TryCheck( *parsed.root ); not rulesMaybe )
            return unexpected( std::move( rulesMaybe.error() ) );
        if ( auto typedMaybe = TryCheckTypes( check, where, source ); not typedMaybe )
            return unexpected( std::move( typedMaybe.error() ) );
    }
    return WriteScriptAsset( asset );
}
}

Importer ScriptImporter( ScriptCheck check )
{
    return Importer{
        .mExtension = ".luau",
        .mKind = string( cScriptKind ),
        .mVersion = 2,
        .mImport = [check = std::move( check )]( string_view source, const Json&, const AssetPath& path )
            -> expected<string, string> { return TryImportScript( check, source, path ); },
    };
}

expected<ScriptCheck, string> TryProjectScriptCheck( App& app,
                                                     const ProjectFiles& files,
                                                     const AssetCatalog& catalog,
                                                     const ScriptLabels& labels )
{
    ScriptCheck check;
    check.mDeclarations = WriteScriptDeclarations( app, labels );
    if ( const auto luaurcMaybe = files.TryRead( *AssetPath::TryFrom( ".luaurc" ) ) )
    {
        auto aliasesMaybe = TryReadLuaurc( *luaurcMaybe );
        if ( not aliasesMaybe )
            return unexpected( std::move( aliasesMaybe.error() ) );
        check.mAliases = std::move( *aliasesMaybe );
    }
    check.mRead = [&files]( const AssetPath& path ) { return files.TryRead( path ); };
    check.mAssetOf = [&catalog]( const AssetPath& path ) { return catalog.TryIdOf( path ).value_or( AssetId{} ); };
    return check;
}

expected<hmap<string, string>, string> TryReadLuaurc( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( ".luaurc: " + jsonMaybe.error() );
    hmap<string, string> aliases;
    const auto found = jsonMaybe->TryFind( "aliases" );
    if ( not found )
        return aliases;
    const auto objectMaybe = found->TryAs<Json::Object>();
    if ( not objectMaybe )
        return unexpected( format( ".luaurc: {}: aliases are an object of names and directories", found->Where() ) );
    for ( const Json::Member& member : *objectMaybe )
    {
        const auto directoryMaybe = member.mValue.TryAs<string>();
        if ( not directoryMaybe )
            return unexpected(
                format( ".luaurc: {}: alias {}: a directory, as a string", member.mValue.Where(), member.mKey ) );
        // Luau's own resolves them from the .luaurc; ours is at the root.
        string directory = *directoryMaybe;
        while ( directory.starts_with( "./" ) )
            directory.erase( 0, 2 );
        while ( directory.ends_with( '/' ) )
            directory.pop_back();
        aliases[member.mKey] = std::move( directory );
    }
    return aliases;
}
}
