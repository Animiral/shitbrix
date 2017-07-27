/**
 * draw.hpp
 * Routines for drawing objects on the screen.
 */
#pragma once

// TODO: use pImpl to remove SDL dependencies from draw interface.
#include "stage.hpp"
#include "gameevent.hpp"
#include "asset.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

/**
 * Draw the main menu to the screen.
 */
class DrawMenu
{

public:

	DrawMenu(const SdlFactory& factory, const Assets& assets);
	void draw() const;

private:

	const SdlFactory& m_factory;
	const Assets& m_assets;

};

/**
 * DrawGame draws gameplay-related objects to the screen.
 * It knows how to interpret various objects’ state and which textures to use.
 */
class DrawGame
{

public:

	/**
	 * Construct a new DrawGame object from the given dependencies.
	 */
	DrawGame(const SdlFactory& factory, const Assets& assets);

	/**
	 * Add the specified pit to be drawn.
	 * DrawGame always associates several player-specific objects with the pit.
	 */
	void add_pit(const Pit& pit, const Cursor& cursor,
				 const Banner& banner, const BonusIndicator& indicator);

	/**
	 * Removes all drawables known to this DrawGame object.
	 */
	void clear();

	void fade(float fraction);

	/**
	 * Render all that we know to the screen.
	 * This includes background, pits and cursors.
	 */
	void draw_all(float dt) const;

	/**
	 * Set whether or not the cursors should be displayed.
	 */
	void show_cursor(bool show);

	/**
	 * Set whether or not the banners should be displayed.
	 */
	void show_banner(bool show);

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
	struct PlayerDrawables
	{
		const Pit& pit;
		const Cursor& cursor;
		const Banner& banner;
		const BonusIndicator& indicator;
	};

	std::vector<PlayerDrawables> m_drawables;
	bool m_show_cursor;
	bool m_show_banner;
	bool m_show_pit_debug_overlay = false;
	bool m_show_pit_debug_highlight = false;

	const SdlFactory& m_factory;
	const Assets& m_assets;

	float m_fade = 1.f;
	mutable Point m_translate{0,0};
	mutable uint8_t m_alpha = 255;
	std::unique_ptr<SDL_Texture, SdlDeleter> m_fadetex; // solid pixel used for fading

	void draw_background() const;
	void draw_pit(const Pit& pit, float dt) const;
	void draw_pit_debug_overlay(const Pit& pit) const;
	void draw_block(const Block& block, float dt) const;
	void draw_garbage(const Garbage& garbage, float dt) const;
	void draw_cursor(const Cursor& cursor, float dt) const;
	void draw_banner(const Banner& banner, float dt) const;
	void draw_bonus(const BonusIndicator& bonus, float dt) const;
	void draw_highlight(Point top_left, int width, int height,
				        uint8_t r, uint8_t g, uint8_t b, uint8_t a) const;
	
	void putsprite(Point loc, Gfx gfx, size_t frame = 0) const;

	/**
	 * Puts the rendered scene on screen
	 */
	void render() const;

};
