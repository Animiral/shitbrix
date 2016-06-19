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

class Block;
using SharedBlock = std::shared_ptr<Block>;
using WeakBlock = std::weak_ptr<Block>;

// /**
//  * The pit does not own its contained blocks (the stage does), but it remembers where blocks are and which
//  * spaces are free or blocked.
//  */
// class IPit
// {
// public:
// 	virtual Point loc() const =0; // get location of pit on canvas
// 	virtual void block(RowCol rc, WeakBlock block) =0;
// 	virtual void unblock(RowCol rc) =0;
// 	virtual WeakBlock block_at(RowCol rc) const =0; // true if the location in the pit is occupied, e.g. by a non-falling block
// };
// using SharedPit = std::shared_ptr<IPit>;
// using WeakPit = std::weak_ptr<IPit>;

using SharedAnimation = std::shared_ptr<IAnimation>;
using SharedLogic = std::shared_ptr<ILogicObject>;
using SharedTransform = std::shared_ptr<ITransform>;
