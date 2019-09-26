#include "asset.hpp"
#include "error.hpp"
#include "context.hpp"
#include <cassert>
#include <SDL.h>

BitmapFont::BitmapFont(const Sdl& sdl, const char* file, wrap::Color outline_color, wrap::Color fill_color)
{
	if(nullptr == file) {
		for(int i = 0; i < 4 * 16; i++)
			m_textures.emplace_back();
		return;
	}

	SurfacePtr charset = sdl.load_surface(file, SDL_PIXELFORMAT_RGBA32);

	// we can atm only deal with the exact expected layout
	enforce(13 * 16 + 1 == charset->w);
	enforce(21 * 4 + 1 == charset->h);

	const wrap::Color placeholder_background{ 144, 144, 144, 255 };
	const wrap::Color placeholder_outline{ 255, 255, 255, 255 };
	const wrap::Color placeholder_fill{ 0, 0, 0, 255 };
	sdl.recolor(*charset, placeholder_background, wrap::Color{ 0, 0, 0, 0 });
	sdl.recolor(*charset, placeholder_outline, outline_color);
	sdl.recolor(*charset, placeholder_fill, fill_color);

	for(int y = 0; y < 4; y++)
		for(int x = 0; x < 16; x++) {
			wrap::Rect rect{ 13 * x + 1, 21 * y + 1, 12, 20 };
			m_textures.push_back(sdl.cutout_texture(*charset, rect));
		}
}

bool BitmapFont::can_print(char c) const noexcept
{
	return (size_t)c - ' ' < m_textures.size();
}

SDL_Texture& BitmapFont::char_texture(char c) const
{
	enforce(c >= ' ');
	enforce(c <= '_');

	return *m_textures[(size_t)c - ' '];
}

NoAssets::NoAssets() noexcept
	: m_bitmap_font(*the_context.sdl, nullptr, {}, {})
{}

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

const BitmapFont& NoAssets::bmp_font() const
{
	assert(0);
	return m_bitmap_font;
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
	wrap::Color outline_color{ 111, 31, 148, 255 };
	wrap::Color fill_color{ 198, 247, 242, 255 };
	m_bitmap_font = std::make_unique<BitmapFont>(sdl, "font/fixed.png", outline_color, fill_color);
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

const BitmapFont& FileAssets::bmp_font() const
{
	return *m_bitmap_font;
}
