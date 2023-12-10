#pragma once 
#include <filesystem>
using namespace std::string_literals;
namespace std
{
using namespace filesystem;
}

namespace bubble
{

template <typename StringType>
StringType ReplaceAll( StringType str, const StringType& from, const StringType& to )
{
	size_t start_pos = 0;
	while ( ( start_pos = str.find( from, start_pos ) ) != StringType::npos )
	{
		str.replace( start_pos, from.length(), to );
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

inline std::string NormalizePath( const std::string& path )
{
	return ReplaceAll( path, "\\"s, "/"s );
}

inline std::string RightPartLastOf( const std::string& str, std::string_view separator )
{
	return str.substr( str.find_last_of( separator ) + 1 );
}

inline std::string MidPartLastOf( const std::string& str, std::string_view separator1, std::string_view separator2 )
{
	auto start_pos = str.find_last_of( separator1 ) + 1;
	auto end_pos = str.find_last_of( separator2 );
	if ( start_pos >= end_pos )
		return str;
	return str.substr( start_pos, end_pos - start_pos );
}

inline std::string CreateRelPath( const std::string& to, const std::string& from )
{
	return ReplaceAll( from, to, ""s );
}

}