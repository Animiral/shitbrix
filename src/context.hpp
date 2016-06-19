/**
 * context.hpp
 * The context interfaces in this module are abstractions of SDL functions.
 * This module is independent of the concrete library-specific implementation.
 */
#pragma once

#include "globals.hpp"
#include <memory>

/**
 * Represents an environment for drawing stuff to the screen. Implemented with SDL.
 */
class IVideoContext
{
public:
	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const =0;
};

/**
 * Basic interface that specifies that an object can be drawn to the screen.
 */
class IScreenObject
{
	public: virtual void draw(const IVideoContext& context, float dt) =0; // dt: fraction of current display frame time elapsed
};

/**
 * Basic interface for animated objects
 */
class IAnimation : public IScreenObject
{
	public: virtual void animate() =0; // Called once per frame to update animation
};

/**
 * Basic interface for objects subject to game logic
 */
class ILogicObject
{
	public: virtual void update() =0; // advance the object by one tick
};

/**
 * Transforms point coordinates.
 * E.g. the scrolling pit translates its block’s coordinates ever upwards.
 * Optionally, a display frame fraction is included to enable smooth drawing.
 */
class ITransform
{
	public: virtual Point transform(Point point, float dt=0.f) const =0;
};

class IHistoryObject {}; // interface go-back etc.

using Animation = std::shared_ptr<IAnimation>;
using Logic = std::shared_ptr<ILogicObject>;
using Transform = std::shared_ptr<ITransform>;
