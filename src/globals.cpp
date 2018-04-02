/**
 * globals.cpp
 * General global definitions without dependencies: implementation.
 */

#include "globals.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

Gfx operator+(Gfx gfx, int delta)
{
	return static_cast<Gfx>(static_cast<int>(gfx) + delta);
}

BlockFrame& operator++(BlockFrame& frame)
{
	return frame = static_cast<BlockFrame>(static_cast<size_t>(frame) + 1);
}

std::ostream& operator<<(std::ostream& stream, RowCol rc)
{
	return stream << "{r" << rc.r << "c" << rc.c << "}";
}

Point from_rc(RowCol rc)
{
	return Point{static_cast<float>(rc.c*BLOCK_W), static_cast<float>(rc.r*BLOCK_H)};
}

SdlException::SdlException()
: GameException(SDL_GetError())
{}

void enforce_impl(bool condition, const char* condition_str, const char* func, const char* file, int line)
{
	if(!condition)
		throw EnforceException(condition_str, func, file, line);
}

void sdlok(int result)
{
	if(0 != result)
		throw SdlException();
}

void sdlok(void* pointer)
{
	if(!pointer)
		throw SdlException();
}

void imgok(void* pointer)
{
	if(!pointer)
		throw SdlException(IMG_GetError());
}
