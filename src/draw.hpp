/**
 * draw.hpp
 * Routines for drawing objects on the screen.
 */
#pragma once

#include "block.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

/**
 * DrawGame draws gameplay-related objects to the screen.
 * It knows how to interpret various objects’ state and which textures to use.
 * It delegates the actual, library-implementation-
 * dependent low-level draw calls to the Context object.
 */
class DrawGame
{

public:

	/**
	 * Construct a new DrawGame object bound to the given context.
	 */
	DrawGame(IContext& context);

	/**
	 * Add the specified pit to be drawn.
	 * DrawGame always associates a cursor with the pit.
	 */
	void add_pit(const PitImpl& pit, const CursorImpl& cursor);

	/**
	 * Removes all drawables known to this DrawGame object.
	 */
	void clear();

	/**
	 * Returns the fraction of logic ticks that have passed since the last
	 * completed tick. Used for interpolating animations.
	 */
	float dt() const;

	/**
	 * Sets the fraction of logic ticks that have passed since the last
	 * completed tick. Used for interpolating animations.
	 */
	void set_dt(float dt) const;

	/**
	 * Render all that we know to the screen.
	 * This includes background, pits and cursors.
	 */
	void draw_all() const;

	/**
	 * Set whether or not the cursors should be displayed.
	 */
	void show_cursors(bool show);

	/**
	 * Show or hide the debug info on the pits.
	 */
	void toggle_pit_debug_overlay();

	/**
	 * Show or hide the debug highlight on the pits.
	 */
	void toggle_pit_debug_highlight();


	// Animation contants
	static constexpr float BLOCK_BOUNCE_H = 10.f; // height of a block’s bouncing animation when it lands
	static constexpr int CURSOR_FRAME_TIME = 4; // how many sceen frames to display one cursor frame
	static constexpr int CURSOR_FRAMES = 4; // number of available cursor frames

private:

	/**
	 * Helper type for draw compound.
	 * We always draw a pit with a cursor in it.
	 */
	struct PitCursor
	{
		const PitImpl& pit;
		const CursorImpl& cursor;
	};

	IContext& m_context;
	std::vector<PitCursor> m_drawables;
	mutable float m_dt;
	bool m_show_cursors;
	bool m_show_pit_debug_overlay;
	bool m_show_pit_debug_highlight;

};
