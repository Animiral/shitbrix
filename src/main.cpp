#include "screen.hpp"
#include "context.hpp"
#include "asset.hpp"
#include <iostream>
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

		int mode_result = SDL_SetTextureBlendMode(fadetex.get(), SDL_BLENDMODE_BLEND);
		game_assert(0 == mode_result, SDL_GetError());
	}

	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const override
	{
		Texture texture = assets.texture(gfx, frame);
		int x = loc.x;
		int y = loc.y;
		SDL_Rect dstrect { x, y, texture->width, texture->height };

		SDL_Renderer* renderer = factory.get_renderer().get();
		SDL_Texture* tex = texture->tex.get();
		int render_result = SDL_RenderCopy(renderer, tex, nullptr, &dstrect);
		game_assert(0 == render_result, SDL_GetError());
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

	float m_fade = 1.f;
	std::unique_ptr<SDL_Texture, SdlDeleter> fadetex; // solid pixel used for fading

};

/**
 * Top-level class which owns general application resources such as the initialized SDL library
 * and offers the main loop function.
 */
class Main
{

public:

	Main() : context(), game_screen() {}

	/**
	 * Main loop.
	 * Design goals are:
	 *  - Renders as many frames as possible
	 *  - Does not fall behind on game logic
	 *  - Handles inputs and events fast
	 *  - Frequently yields CPU to other programs in need
	 * 
	 * Speed is controlled by FPS (frames per second) and TPS (logic ticks per second) in shitbrix.hpp.
	 */
	void game_loop()
	{
		start:

		Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
		Uint64 freq = SDL_GetPerformanceFrequency();
		long tick = 0; // current logic tick counter
		Uint64 next_logic = t0 + freq / TPS; // time for next logic update

		for(;;) {
			// draw frames as long as logic is up to date
			Uint64 now = SDL_GetPerformanceCounter();
			while(now < next_logic) {
				float fraction = 1.0f - static_cast<float>((next_logic-now) * TPS) / freq;
				SDL_assert((fraction >= 0) && (fraction <= 1));

				game_screen.draw(context, fraction);
				context.render();
				now = SDL_GetPerformanceCounter();
			}

			// get inputs for next logic tick
			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				switch(event.type) {
					case SDL_QUIT:
						goto quit;	
					case SDL_KEYDOWN:
						if(!event.key.repeat) {
							switch(event.key.keysym.sym) {
								case SDLK_LEFT:  game_screen.input_dir(Dir::LEFT, 0); break;
								case SDLK_RIGHT: game_screen.input_dir(Dir::RIGHT, 0); break;
								case SDLK_UP:    game_screen.input_dir(Dir::UP, 0); break;
								case SDLK_DOWN:  game_screen.input_dir(Dir::DOWN, 0); break;
								case SDLK_z:     game_screen.input_a(0); context.play(Snd::SWAP); break;
								case SDLK_KP_4:  game_screen.input_dir(Dir::LEFT, 1); break;
								case SDLK_KP_6:  game_screen.input_dir(Dir::RIGHT, 1); break;
								case SDLK_KP_8:  game_screen.input_dir(Dir::UP, 1); break;
								case SDLK_KP_5:  game_screen.input_dir(Dir::DOWN, 1); break;
								case SDLK_KP_0:  game_screen.input_a(1); context.play(Snd::SWAP); break;
								case SDLK_d:     game_screen.input_debug(0); break;
								case SDLK_h:     game_screen.input_debug(1); break;
								case SDLK_RETURN: // restart
									game_screen = GameScreen();
									goto start;
								case SDLK_ESCAPE:
									goto quit;
							}
						}
						break;
				}
			}

			// auto-move cursor when scrolling out of bounds
			game_screen.input_dir(Dir::NONE, 0);
			game_screen.input_dir(Dir::NONE, 1);

			// run one frame of local logic
			game_screen.animate();
			game_screen.update(context);

			tick++;
			next_logic = t0 + (tick+1) * freq / TPS;
		}

		quit:;
	}

private:

	SdlContext context;
	GameScreen game_screen;

};

int main()
{
	Main main;
	main.game_loop();
	return 0;
}
