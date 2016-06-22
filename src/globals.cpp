/**
 * globals.cpp
 * General global definitions without dependencies: implementation.
 */

#include "globals.hpp"

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

/**
 * Check the condition and, if false, throw a GameException with the given message.
 */
void game_assert(bool condition, const char* what)
{
	if(!condition) {
		throw GameException(what);
	}
}
