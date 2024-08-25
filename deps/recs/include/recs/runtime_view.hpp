#pragma once
#include <stdexcept>
#include "recs/impex.hpp"
#include "recs/utils.hpp"
#include "recs/entity.hpp"

namespace recs
{
class RECS_EXPORT RuntimeView
{
public:
	size_t Size()
	{
		return mEntities.size();
	}
	
	std::vector<Entity>& GetEntities()
	{
		return mEntities;
	}
	
	auto begin()
	{
		return mEntities.begin();
	}
	auto end()
	{
		return mEntities.end();
	}

private:
    RuntimeView( std::vector<Entity>&& entities )
        : mEntities( entities )
    {}

	std::vector<Entity> mEntities;
	friend class Registry;
};

}