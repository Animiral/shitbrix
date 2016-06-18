/**
 * context.hpp
 * The context interfaces in this module are abstractions of SDL functions.
 * This module is independent of the concrete library-specific implementation.
 */
#pragma once

#include "shitbrix.hpp"
#include <memory>

/**
 * Represents an environment for drawing stuff to the screen. Implemented with SDL.
 */
class IVideoContext
{
public:
	virtual void drawGfx(Gfx gfx, Point loc) const =0;
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

class IHistoryObject {}; // interface go-back etc.

using SharedAnimation = std::shared_ptr<IAnimation>;
using SharedLogic = std::shared_ptr<ILogicObject>;
