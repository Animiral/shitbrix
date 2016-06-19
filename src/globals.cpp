/**
 * globals.cpp
 * General global definitions without dependencies: implementation.
 */

#include "globals.hpp"

/**
 * Check the condition and, if false, throw a GameException with the given message.
 */
void game_assert(bool condition, const char* what)
{
	if(!condition) {
		throw GameException(what);
	}
}
