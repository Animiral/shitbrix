/**
 * Definitions for drawing routines.
 */
#include "draw.hpp"
#include "globals.hpp"
#include "error.hpp"
#include <string>
#include <sstream>
#include <cmath>
#include <cassert>
#include <SDL.h>

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

void SdlDraw::text(int x, int y, const char* text, SDL_Color color)
{
	std::vector<TexturePtr> line_textures;
	std::istringstream stream(text);
	std::string line;

	while(std::getline(stream, line, '\n')) {
		SurfacePtr text_surface{ TTF_RenderUTF8_Blended(&m_assets->font(), line.c_str(), color) };
		ttfok(text_surface.get());
		auto& tex = line_textures.emplace_back(SDL_CreateTextureFromSurface(m_renderer, text_surface.get()));
		sdlok(tex.get());
	}

	for(int line = 0; line < line_textures.size(); line++) {
		TexturePtr& tex = line_textures[line];
		Uint32 format;
		int access;
		int w;
		int h;
		sdlok(SDL_QueryTexture(tex.get(), &format, &access, &w, &h));
		const SDL_Rect dest_rect{ x, y + line * DEFAULT_FONT_LINEHEIGHT, w, h };
		sdlok(SDL_RenderCopy(m_renderer, tex.get(), NULL, &dest_rect));
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
