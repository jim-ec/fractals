#pragma once

#include "glm/glm.hpp"

class Dimension
	: glm::ivec2
{

public:

	int &width;
	int &height;

	template<class... T>
	inline Dimension(T&&... args)
		: width{ x }
		, height{ y }
		, glm::ivec2{ std::forward<T>(args)... }
	{
	}

	inline Dimension operator=(const Dimension &other) {
		glm::ivec2::operator=(other);
		return *this;
	}

};
