/**
 * Definitions for drawing routines.
 */
#include "draw.hpp"
#include "globals.hpp"
#include "error.hpp"
#include <string>
#include <sstream>
#include <cmath>
#include <cctype>
#include <cassert>
#include <SDL.h>
#include <SDL_ttf.h>

const uint8_t ALPHA_OPAQUE = SDL_ALPHA_OPAQUE;

ICanvas::~ICanvas() = default;

IDraw::~IDraw() = default;

void IDraw::gfx(Point loc, Gfx gfx, size_t frame, uint8_t a)
{
	this->gfx(static_cast<int>(loc.x), static_cast<int>(loc.y), gfx, frame, a);
}


SdlCanvas::SdlCanvas(TexturePtr texture, SDL_Renderer& renderer)
	: m_texture(move(texture)), m_renderer(&renderer)
{
	enforce(nullptr != m_texture);
}

void SdlCanvas::use_as_target()
{
	sdlok(SDL_SetRenderTarget(m_renderer, m_texture.get()));
}

void SdlCanvas::draw()
{
	sdlok(SDL_RenderCopy(m_renderer, m_texture.get(), NULL, NULL));
}


SdlDraw::SdlDraw(SDL_Renderer& renderer, const Assets& assets)
	: m_renderer(&renderer), m_assets(&assets)
{
	enforce(nullptr != m_renderer);
}

void SdlDraw::gfx(int x, int y, Gfx gfx, size_t frame, uint8_t a)
{
	SDL_Texture* texture = &m_assets->texture(gfx, frame);
	SDL_Rect dstrect{x, y, 0, 0};
	sdlok(SDL_QueryTexture(texture, NULL, NULL, &dstrect.w, &dstrect.h));
	sdlok(SDL_SetTextureAlphaMod(texture, a));
	sdlok(SDL_RenderCopy(m_renderer, texture, NULL, &dstrect));
}

void SdlDraw::rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	SDL_Rect fill_rect{x, y, w, h};
	sdlok(SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND));
	sdlok(SDL_SetRenderDrawColor(m_renderer, r, g, b, a));
	sdlok(SDL_RenderFillRect(m_renderer, &fill_rect));
}

void SdlDraw::highlight(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	SDL_Rect fill_rect{x, y, w, h};
	sdlok(SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_ADD));
	sdlok(SDL_SetRenderDrawColor(m_renderer, r, g, b, a));
	sdlok(SDL_RenderFillRect(m_renderer, &fill_rect));
}

void SdlDraw::text(int x, int y, const TtfText& text)
{
	SDL_Texture& tex = text.texture();
	Uint32 format;
	int access;
	int w;
	int h;
	sdlok(SDL_QueryTexture(&tex, &format, &access, &w, &h));
	const SDL_Rect dest_rect{ x, y, w, h };
	sdlok(SDL_RenderCopy(m_renderer, &tex, NULL, &dest_rect));
}

void SdlDraw::text_fixed(int x, int y, const BitmapFont& font, const char* text)
{
	std::istringstream stream(text);
	int linenr = 0;
	std::string line;

	while(std::getline(stream, line, '\n')) {
		for(int i = 0; i < line.size(); i++) {
			char c = static_cast<char>(std::toupper(line[i]));

			if(!font.can_print(c))
				c = '?';

			SDL_Texture& tex = font.char_texture(c);
			Uint32 format;
			int access;
			int w;
			int h;
			sdlok(SDL_QueryTexture(&tex, &format, &access, &w, &h));
			const SDL_Rect dest_rect{ x + i * BITMAP_FONT_ADVANCE, y + linenr * BITMAP_FONT_LINEHEIGHT, w, h };
			sdlok(SDL_RenderCopy(m_renderer, &tex, NULL, &dest_rect));
		}

		linenr++;
	}
}

void SdlDraw::clip(int x, int y, int w, int h)
{
	SDL_Rect clip_rect{x, y, w, h};
	sdlok(SDL_RenderSetClipRect(m_renderer, &clip_rect));
}

void SdlDraw::unclip()
{
	sdlok(SDL_RenderSetClipRect(m_renderer, NULL));
}

std::unique_ptr<ICanvas> SdlDraw::create_canvas()
{
	return std::make_unique<SdlCanvas>(the_context.sdl->create_target_texture(), *m_renderer);
}

void SdlDraw::reset_target()
{
	sdlok(SDL_SetRenderTarget(m_renderer, NULL));
}

void SdlDraw::render()
{
	SDL_RenderPresent(m_renderer);

	// clear for next frame
	sdlok(SDL_RenderClear(m_renderer));
}
