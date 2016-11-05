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

		sounds.emplace_back(factory.create_sound("snd/swap.wav"));   // Snd::SWAP
		sounds.emplace_back(factory.create_sound("snd/break.wav"));  // Snd::BREAK
		sounds.emplace_back(factory.create_sound("snd/match.wav"));  // Snd::MATCH
	}

	/**
	 * Returns a Texture according to the gfx enum id.
	 */
	Texture texture(Gfx gfx, size_t frame = 0) const
	{
		return textures[static_cast<size_t>(gfx)][frame];
	}

	Sound sound(Snd snd) const
	{
		return sounds[static_cast<size_t>(snd)];
	}

private:

	std::vector< std::vector<Texture> > textures;
	std::vector< Sound > sounds;

};
