#include "asset.hpp"
#include "error.hpp"
#include "context.hpp"
#include <cassert>
#include <SDL.h>

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

TTF_Font& NoAssets::ttf_font() const
{
	assert(0);
	return *static_cast<TTF_Font*>(nullptr);
}

SDL_Surface& NoAssets::charset() const
{
	assert(0);
	return *static_cast<SDL_Surface*>(nullptr);
}

FileAssets::FileAssets(const Sdl& sdl)
{
	Log::info("Load assets: graphics");
	std::vector<TexturePtr> bgframe;
	bgframe.emplace_back(sdl.create_texture("gfx/bg.png"));
	m_textures.emplace_back(move(bgframe));                                        // Gfx::BACKGROUND

	auto blocks = sdl.create_texture_sheet("gfx/blocks.png", BLOCK_W, BLOCK_H);
	for(auto& v : blocks)
		m_textures.emplace_back(move(v));                                          // Gfx::BLOCK_*, Gfx::PITVIEW

	m_textures.emplace_back(sdl.create_texture_row("gfx/cursor.png", CURSOR_W));   // Gfx::CURSOR
	m_textures.emplace_back(sdl.create_texture_row("gfx/banner.png", BANNER_W));   // Gfx::BANNER

	auto garbage = sdl.create_texture_sheet("gfx/garbage.png", GARBAGE_W, GARBAGE_H);
	for(auto& v : garbage)
		m_textures.emplace_back(move(v));                                          // Gfx::GARBAGE_*

	m_textures.emplace_back(sdl.create_texture_row("gfx/bonus.png", BONUS_W));     // Gfx::BONUS

	std::vector<TexturePtr> menuframe;
	menuframe.emplace_back(sdl.create_texture("gfx/menubg.png"));
	m_textures.emplace_back(move(menuframe)); // Gfx::MENUBG

	Log::info("Load assets: sounds");
	m_sounds.emplace_back(Sound("snd/swap.wav"));   // Snd::SWAP
	m_sounds.emplace_back(Sound("snd/break.wav"));  // Snd::BREAK
	m_sounds.emplace_back(Sound("snd/match.wav"));  // Snd::MATCH
	m_sounds.emplace_back(Sound("snd/thump.wav"));  // Snd::LANDING

	m_ttf_font = sdl.open_font("font/default.ttf", DEFAULT_FONT_SIZE);
	m_charset = sdl.load_surface("font/fixed.png", SDL_PIXELFORMAT_RGBA32);
}

SDL_Texture& FileAssets::texture(Gfx gfx, size_t frame) const
{
	size_t gfx_index = static_cast<size_t>(gfx);
	enforce(gfx_index < m_textures.size());
	enforce(frame < m_textures[gfx_index].size());

	return *m_textures[gfx_index][frame];
}

const Sound& FileAssets::sound(Snd snd) const
{
	size_t snd_index = static_cast<size_t>(snd);
	enforce(snd_index < m_sounds.size());

	return m_sounds[snd_index];
}

TTF_Font& FileAssets::ttf_font() const
{
	return *m_ttf_font;
}

SDL_Surface& FileAssets::charset() const
{
	return *m_charset;
}
