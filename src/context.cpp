/**
 * Definitions for context interfaces
 */
#include "context.hpp"

/**
 * Ordering function applicable for memory-managed IScreenObjects.
 * Simple adapter using IScreenObject::operator<().
 */
bool z_less (const ScreenObject& lhs, const ScreenObject& rhs)
{
	return *lhs < *rhs;
};
