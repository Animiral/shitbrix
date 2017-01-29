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

	Assets(SdlFactory& factory)
	{
		textures.emplace_back(std::vector<Texture>{factory.create_texture("gfx/bg.png")}); // Gfx::BACKGROUND
		auto blocks = factory.create_texture_sheet("gfx/blocks.png", BLOCK_W, BLOCK_H);
		textures.insert(textures.end(), blocks.begin(), blocks.end());                   // Gfx::BLOCK_*, Gfx::PITVIEW
		textures.emplace_back(factory.create_texture_row("gfx/cursor.png", CURSOR_W));   // Gfx::CURSOR
		textures.emplace_back(factory.create_texture_row("gfx/banner.png", BANNER_W));   // Gfx::BANNER
		textures.emplace_back(factory.create_texture_row("gfx/garbage.png", GARBAGE_W)); // Gfx::GARBAGE
		textures.emplace_back(factory.create_texture_row("gfx/bonus.png", BONUS_W));     // Gfx::BONUS

		sounds.emplace_back(factory.create_sound("snd/swap.wav"));   // Snd::SWAP
		sounds.emplace_back(factory.create_sound("snd/break.wav"));  // Snd::BREAK
		sounds.emplace_back(factory.create_sound("snd/match.wav"));  // Snd::MATCH
	}

	/**
	 * Returns a Texture according to the gfx enum id.
	 */
	Texture texture(Gfx gfx, size_t frame = 0) const
	{
		size_t gfx_index = static_cast<size_t>(gfx);
		SDL_assert(gfx_index < textures.size());
		SDL_assert(frame < textures[gfx_index].size());

		return textures[gfx_index][frame];
	}

	Sound sound(Snd snd) const
	{
		size_t snd_index = static_cast<size_t>(snd);
		SDL_assert(snd_index < sounds.size());

		return sounds[snd_index];
	}

private:

	std::vector< std::vector<Texture> > textures;
	std::vector< Sound > sounds;

};
