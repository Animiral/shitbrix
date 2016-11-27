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

	/**
	 * Draws a texture to the screen at a given position.
	 * @param loc The target location in screen pixels, from the top left.
	 * @param gfx The source texture identifier as recognized by Assets
	 * @param frame The source frame seq. nr. from the sprite sheet
	 */
	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const =0;

	/**
	 * Sets the translation offset for all future drawGfx calls.
	 * There is no transform stack. To reset, just translate(0).
	 * Does not affect clip() parameter coordinates.
	 */
	virtual void translate(Point offset) =0;

	/**
	 * Enables a clipping rectangle outside which no graphics will be drawn.
	 */
	virtual void clip(Point top_left, int width, int height) =0;

	/**
	 * Disables the clipping rectangle.
	 */
	virtual void unclip() =0;

	/**
	 * Sets a fraction by which all screen content
	 * will be mixed with pure black.
	 */
	virtual void fade(float fraction) =0;

	/**
	 * Start playback of the indicated sound as recognized by Assets.
	 */
	virtual void play(Snd snd) =0;

	/**
	 * Highlights a rectangular area of the screen with alpha-blended yellow.
	 * Used for debugging.
	 */
	virtual void highlight(Point top_left, int width, int height) =0;

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

class IHistoryObject {}; // interface go-back etc.

using Animation = std::shared_ptr<IAnimation>;
using Logic = std::shared_ptr<ILogic>;

bool z_less (const Animation& lhs, const Animation& rhs);
