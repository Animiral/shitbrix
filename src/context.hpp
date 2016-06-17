/**
 * context.hpp
 * The context interfaces in this module are abstractions of SDL functions.
 * This module is independent of the concrete library-specific implementation.
 */
#pragma once

#include "shitbrix.hpp"

/**
 * Represents an environment for drawing stuff to the screen. Implemented with SDL.
 */
class IVideoContext
{
public:
	virtual void drawGfx(Gfx gfx, Point loc) const =0;
};
