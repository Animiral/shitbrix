/**
 * sdl_context.hpp
 * SDL-specific implementation of the Context interface,
 * which allows the game to output graphics and sound.
 */
#pragma once

#include "context.hpp"
#include "asset.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_image.h>

/**
 * The SdlContext owns the SDL setup (from SDL_Init to SDL_Quit) and the window.
 */
class SdlContext : public IContext
{

public:

	SdlContext() : factory(), assets(factory)
	{
		fadetex = std::unique_ptr<SDL_Texture, SdlDeleter>(SDL_CreateTexture(factory.get_renderer().get(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 1, 1)); // 1x1 pixel for fading
		game_assert(bool(fadetex), SDL_GetError());

		int texblend_result = SDL_SetTextureBlendMode(fadetex.get(), SDL_BLENDMODE_BLEND);
		game_assert(0 == texblend_result, SDL_GetError());

		SDL_Renderer* renderer = factory.get_renderer().get();
		int drawblend_result = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
		game_assert(0 == drawblend_result, SDL_GetError());
	}

	virtual void translate(Point offset) override
	{
		m_translate = offset;
	}

	virtual void clip(Point top_left, int width, int height) override
	{
		int x = top_left.x;
		int y = top_left.y;
		SDL_Rect clip_rect{x, y, width, height};

		SDL_Renderer* renderer = factory.get_renderer().get();
		int clip_result = SDL_RenderSetClipRect(renderer, &clip_rect);
		game_assert(0 == clip_result, SDL_GetError());
	}

	virtual void unclip() override
	{
		SDL_Renderer* renderer = factory.get_renderer().get();
		int clip_result = SDL_RenderSetClipRect(renderer, nullptr);
		game_assert(0 == clip_result, SDL_GetError());
	}

	virtual void fade(float fraction) override
	{
		m_fade = fraction;
	}

	virtual void play(Snd snd) override
	{
		Sound sound = assets.sound(snd);
		auto audio = factory.get_audio();
		audio->play(sound);
	}

	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const override
	{
		Texture texture = assets.texture(gfx, frame);
		int x = loc.x + m_translate.x;
		int y = loc.y + m_translate.y;
		SDL_Rect dstrect { x, y, texture->width, texture->height };

		SDL_Renderer* renderer = factory.get_renderer().get();
		SDL_Texture* tex = texture->tex.get();
		int render_result = SDL_RenderCopy(renderer, tex, nullptr, &dstrect);
		game_assert(0 == render_result, SDL_GetError());
	}

	virtual void highlight(Point top_left, int width, int height,
	                       uint8_t r, uint8_t g, uint8_t b, uint8_t a) const override
	{
		Point loc = top_left.offset(m_translate.x, m_translate.y);
		SDL_Rect fill_rect {
			static_cast<int>(loc.x),
			static_cast<int>(loc.y),
			width,
			height
		};

		SDL_Renderer* renderer = factory.get_renderer().get();
		int color_result = SDL_SetRenderDrawColor(renderer, r, g, b, a);
		game_assert(0 == color_result, SDL_GetError());
		int fill_result = SDL_RenderFillRect(renderer, &fill_rect);
		game_assert(0 == fill_result, SDL_GetError());
	}

	/**
	 * Puts the rendered scene on screen
	 */
	void render() const
	{
		SDL_Renderer* renderer = factory.get_renderer().get();

		if(m_fade < 1.f) {
			SDL_Rect rect_pixel{0,0,1,1};
			uint32_t fade_pixel = static_cast<uint32_t>(0xff * (1.f - m_fade));
			int tex_result = SDL_UpdateTexture(fadetex.get(), &rect_pixel, &fade_pixel, 1);
			game_assert(0 == tex_result, SDL_GetError());

			int render_result = SDL_RenderCopy(renderer, fadetex.get(), nullptr, nullptr);
			game_assert(0 == render_result, SDL_GetError());
		}

		SDL_RenderPresent(renderer);

		// clear for next frame
		int render_result = SDL_RenderClear(renderer);
		game_assert(0 == render_result, SDL_GetError());
	}

private:

	SdlFactory factory;
	Assets assets;

	Point m_translate{0,0};
	float m_fade = 1.f;
	std::unique_ptr<SDL_Texture, SdlDeleter> fadetex; // solid pixel used for fading

};
