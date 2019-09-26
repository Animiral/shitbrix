#include "text.hpp"
#include "error.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <sstream>

TtfText::TtfText(const Sdl& sdl, TTF_Font& font, const char* text, wrap::Color color)
{
	// split text by line - SDL_ttf renders every line to a surface
	std::vector<SurfacePtr> line_surfaces;
	std::istringstream stream(text);
	std::string line;

	while(std::getline(stream, line, '\n')) {
		SDL_Color sdl_color{ color.r, color.g, color.b, color.a };
		SurfacePtr& surf = line_surfaces.emplace_back(TTF_RenderUTF8_Blended(&font, line.c_str(), sdl_color));
		ttfok(surf.get());
	}

	// find out the final extents of the prepared text rendering
	int block_w = 0;
	int block_h = (int)line_surfaces.size() * DEFAULT_FONT_LINEHEIGHT;
	for(int i = 0; i < line_surfaces.size(); i++) {
		block_w = std::max(block_w, line_surfaces[i]->w);
	}

	// software-blit all lines into one big surface
	SurfacePtr surface = sdl.create_surface(block_w, block_h);
	int y = 0;
	for(int i = 0; i < line_surfaces.size(); i++) {
		SDL_Surface* src = line_surfaces[i].get();
		SDL_Rect dstrect{0, i * DEFAULT_FONT_LINEHEIGHT, src->w, src->h};
		sdlok(SDL_BlitSurface(src, NULL, surface.get(), &dstrect));
	}

	// finalize texture for use
	m_texture.reset(SDL_CreateTextureFromSurface(&sdl.renderer(), surface.get()));
	sdlok(m_texture.get());
}

BitmapFont::BitmapFont(const Sdl& sdl, SDL_Surface& charset, wrap::Color outline_color, wrap::Color fill_color)
{
	// we can atm only deal with the exact expected layout
	enforce(13 * 16 + 1 == charset.w);
	enforce(21 * 4 + 1 == charset.h);

	SurfacePtr colored_charset = sdl.create_surface(charset.w, charset.h);
	sdlok(SDL_BlitSurface(&charset, NULL, colored_charset.get(), NULL));
	const wrap::Color placeholder_background{ 144, 144, 144, 255 };
	const wrap::Color placeholder_outline{ 255, 255, 255, 255 };
	const wrap::Color placeholder_fill{ 0, 0, 0, 255 };
	sdl.recolor(*colored_charset, placeholder_background, wrap::Color{ 0, 0, 0, 0 });
	sdl.recolor(*colored_charset, placeholder_outline, outline_color);
	sdl.recolor(*colored_charset, placeholder_fill, fill_color);

	for(int y = 0; y < 4; y++)
		for(int x = 0; x < 16; x++) {
			wrap::Rect rect{ 13 * x + 1, 21 * y + 1, 12, 20 };
			m_textures.push_back(sdl.cutout_texture(*colored_charset, rect));
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
