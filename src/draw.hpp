/**
 * draw.hpp
 * Routines for drawing objects on the screen.
 */
#pragma once

#include "block.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

/**
 * Draw knows how to draw various objects, how to interpret their state
 * and which textures to use. It delegates the actual, library-implementation-
 * dependent low-level draw calls to the Context object.
 */
class Draw
{

public:

	/**
	 * Construct a new Draw object bound to the given context.
	 */
	Draw(IContext& context);

	/**
	 * Returns the fraction of logic ticks that have passed since the last
	 * completed tick. Used for interpolating animations.
	 */
	float dt() const;

	/**
	 * Sets the fraction of logic ticks that have passed since the last
	 * completed tick. Used for interpolating animations.
	 */
	void set_dt(float dt);

	/**
	 * Render the background image to the screen.
	 */
	void draw_background() const;

	/**
	 * Render the given pit and all its contents to the screen.
	 */
	void draw_pit(const PitImpl& pit) const;

private:

	IContext& m_context;
	float m_dt;

};
