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
	 * Draws a texture to the screen at a given position.
	 * @param loc The target location in screen pixels, from the top left.
	 * @param gfx The source texture identifier as recognized by Assets
	 * @param frame The source frame seq. nr. from the sprite sheet
	 */
	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const =0;

	/**
	 * Highlights a rectangular area of the screen with an alpha-blended color.
	 */
	virtual void highlight(Point top_left, int width, int height,
	                       uint8_t r, uint8_t g, uint8_t b, uint8_t a) const =0;

};

/**
 * Basic interface for objects subject to game logic.
 * Logic objects are constructed to fit their place on
 * the stage and are not meant to be copied or moved.
 */
class ILogic
{
public:
	ILogic() noexcept =default;
	ILogic(const ILogic& ) =delete;
	ILogic(const ILogic&& ) =delete;
	virtual ~ILogic() noexcept =default;
	virtual void update(IContext& context) =0; // advance the object by one tick
};

class IHistoryObject {}; // interface go-back etc.
