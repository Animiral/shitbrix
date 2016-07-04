/**
 * Definitions for context interfaces
 */
#include "context.hpp"

/**
 * Ordering function applicable for memory-managed IAnimations.
 * Simple adapter using IAnimation::operator<().
 */
bool z_less (const Animation& lhs, const Animation& rhs)
{
	return *lhs < *rhs;
};
