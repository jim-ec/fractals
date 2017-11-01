#include "Display.h"

const double Display::LOGIC_VIEWPORT_SIZE_MUL = 2;

Display::Display()
{
}


Display::~Display()
{
}

void Display::draw()
{
	// Render into buffer:
	for (int x = 0; x < mViewportSize.width; x++) {
		for (int y = 0; y < mViewportSize.height; y++) {
			double shaderX = (static_cast<double>(x) - mViewportOrigin.x) / mViewportSize.width * 2 * LOGIC_VIEWPORT_SIZE_MUL;
			double shaderY = ((mViewportSize.height - static_cast<double>(y)) - mViewportOrigin.y) / mViewportSize.height * 2 * LOGIC_VIEWPORT_SIZE_MUL;
			char c = mShader(shaderX, shaderY);
			mBuffer[y][x] = c;
		}
	}

	// Print buffer:
	for (int y = 0; y < mViewportSize.height; y++) {
		std::cout << mBuffer[y] << '\n';
	}
	std::cout << std::flush;
}
