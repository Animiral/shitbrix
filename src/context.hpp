/**
 * context.hpp
 * The context interfaces in this module are abstractions of SDL functions.
 * This module is independent of the concrete library-specific implementation.
 */
#pragma once

#include "globals.hpp"
#include <memory>

/**
 * Abstracts the underlying library functions. Implemented with SDL.
 */
class IContext
{
public:
	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const =0;
	virtual void clip(Point top_left, int width, int height) =0;
	virtual void unclip() =0;
	virtual void fade(float fraction) =0;
	virtual void play(Snd snd) =0;
};

/**
 * Basic interface that specifies that an object can be drawn to the screen.
 */
class IAnimation
{
public:
	IAnimation(int z_order) : z_order(z_order) {}
	virtual void draw(IContext& context, float dt) =0; // dt: fraction of current display frame time elapsed
	virtual void animate() {} // Called once per frame to update animation
	bool operator<(const IAnimation& rhs) const { return z_order < rhs.z_order; }
private:
	int z_order; // Specifies drawing order. Every subclass must set this value.
};

/**
 * Basic interface for objects subject to game logic
 */
class ILogic
{
	public: virtual void update(IContext& context) =0; // advance the object by one tick
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
using Logic = std::shared_ptr<ILogic>;
using Transform = std::shared_ptr<ITransform>;

bool z_less (const Animation& lhs, const Animation& rhs);
