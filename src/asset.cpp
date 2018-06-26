#include "asset.hpp"
#include "error.hpp"
#include "context.hpp"
#include <SDL.h>
#include <cassert>

SDL_Texture& NoAssets::texture(Gfx gfx, size_t frame) const
{
	assert(0);
	return *static_cast<SDL_Texture*>(nullptr);
}

const Sound& NoAssets::sound(Snd snd) const
{
	assert(0);
	return *static_cast<Sound*>(nullptr);
}

FileAssets::FileAssets()
{
	const Sdl& sdl = *the_context.sdl;

	Log::info("Load assets: graphics");
	std::vector<TexturePtr> bgframe;
	bgframe.emplace_back(sdl.create_texture("gfx/bg.png"));
	textures.emplace_back(move(bgframe));                                        // Gfx::BACKGROUND

	auto blocks = sdl.create_texture_sheet("gfx/blocks.png", BLOCK_W, BLOCK_H);
	for(auto& v : blocks)
		textures.emplace_back(move(v));                                          // Gfx::BLOCK_*, Gfx::PITVIEW

	textures.emplace_back(sdl.create_texture_row("gfx/cursor.png", CURSOR_W));   // Gfx::CURSOR
	textures.emplace_back(sdl.create_texture_row("gfx/banner.png", BANNER_W));   // Gfx::BANNER

	auto garbage = sdl.create_texture_sheet("gfx/garbage.png", GARBAGE_W, GARBAGE_H);
	for(auto& v : garbage)
		textures.emplace_back(move(v));                                          // Gfx::GARBAGE_*

	textures.emplace_back(sdl.create_texture_row("gfx/bonus.png", BONUS_W));     // Gfx::BONUS

	std::vector<TexturePtr> menuframe;
	menuframe.emplace_back(sdl.create_texture("gfx/menubg.png"));
	textures.emplace_back(move(menuframe)); // Gfx::MENUBG

	Log::info("Load assets: sounds");
	sounds.emplace_back(Sound("snd/swap.wav"));   // Snd::SWAP
	sounds.emplace_back(Sound("snd/break.wav"));  // Snd::BREAK
	sounds.emplace_back(Sound("snd/match.wav"));  // Snd::MATCH
	sounds.emplace_back(Sound("snd/thump.wav"));  // Snd::LANDING
}

SDL_Texture& FileAssets::texture(Gfx gfx, size_t frame) const
{
	size_t gfx_index = static_cast<size_t>(gfx);
	enforce(gfx_index < textures.size());
	enforce(frame < textures[gfx_index].size());

	return *textures[gfx_index][frame];
}

const Sound& FileAssets::sound(Snd snd) const
{
	size_t snd_index = static_cast<size_t>(snd);
	enforce(snd_index < sounds.size());

	return sounds[snd_index];
}
