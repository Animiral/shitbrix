/**
 * asset.hpp
 * Functions for loading, destroying and identifying the game assets.
 */
#pragma once

#include "globals.hpp"
#include "sdl_helper.hpp"
#include <memory>
#include <vector>

/**
 * Storage class which owns all the assets.
 */
class Assets
{

public:

	Assets()
	{
		const Sdl& sdl = Sdl::instance();

		std::vector<TexturePtr> bgframe;
		bgframe.emplace_back(sdl.create_texture("gfx/bg.png"));
		textures.emplace_back(move(bgframe));                                        // Gfx::BACKGROUND

		auto blocks = sdl.create_texture_sheet("gfx/blocks.png", BLOCK_W, BLOCK_H);
		for(auto& v : blocks)
			textures.emplace_back(move(v));                                          // Gfx::BLOCK_*, Gfx::PITVIEW

		textures.emplace_back(sdl.create_texture_row("gfx/cursor.png", CURSOR_W));   // Gfx::CURSOR
		textures.emplace_back(sdl.create_texture_row("gfx/banner.png", BANNER_W));   // Gfx::BANNER
		textures.emplace_back(sdl.create_texture_row("gfx/garbage.png", GARBAGE_W)); // Gfx::GARBAGE
		textures.emplace_back(sdl.create_texture_row("gfx/bonus.png", BONUS_W));     // Gfx::BONUS

		std::vector<TexturePtr> menuframe;
		menuframe.emplace_back(sdl.create_texture("gfx/menubg.png"));
		textures.emplace_back(move(menuframe)); // Gfx::MENUBG

		sounds.emplace_back(Sound("snd/swap.wav"));   // Snd::SWAP
		sounds.emplace_back(Sound("snd/break.wav"));  // Snd::BREAK
		sounds.emplace_back(Sound("snd/match.wav"));  // Snd::MATCH
		sounds.emplace_back(Sound("snd/thump.wav"));  // Snd::LANDING
	}

	/**
	 * Returns a Texture according to the gfx enum id.
	 */
	SDL_Texture& texture(Gfx gfx, size_t frame = 0) const
	{
		size_t gfx_index = static_cast<size_t>(gfx);
		SDL_assert(gfx_index < textures.size());
		SDL_assert(frame < textures[gfx_index].size());

		return *textures[gfx_index][frame];
	}

	const Sound& sound(Snd snd) const
	{
		size_t snd_index = static_cast<size_t>(snd);
		SDL_assert(snd_index < sounds.size());

		return sounds[snd_index];
	}

private:

	std::vector< std::vector<TexturePtr> > textures;
	std::vector< Sound > sounds;

};
