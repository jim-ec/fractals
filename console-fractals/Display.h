#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include "Dimension.h"

class Display
{

private:
	
	Dimension mViewportSize;

	/**
	 * A row-major back buffer
	*/
	std::vector<std::string> mBuffer;
	
	std::function<char(double x, double y)> mShader;

	glm::ivec2 mViewportOrigin;

public:

	static const double LOGIC_VIEWPORT_SIZE_MUL;

	enum class Origin {
		CENTER
	};

	Display();

	~Display();

	inline void setViewportSize(const Dimension &displaySize)
	{
		mViewportSize = displaySize;
		mBuffer.resize(displaySize.height);
		for (auto &row : mBuffer) {
			row.resize(displaySize.width);
		}
	}

	inline void setShader(decltype(mShader) &&shader) {
		mShader = std::forward<decltype(mShader)>(shader);
	}

	inline void setViewportOrigin(glm::ivec2 viewportOrigin) {
		mViewportOrigin = viewportOrigin;
	}

	inline void setViewportOrigin(Origin viewportOrigin) {
		switch (viewportOrigin) {
		case Origin::CENTER:
			setViewportOrigin({ mViewportSize.width / 2, mViewportSize.height / 2 });
		}
	}

	void draw();

};
