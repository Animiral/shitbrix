#include "screen.hpp"
#include "context.hpp"
#include "asset.hpp"
#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_image.h>

const char* APP_NAME = "shitbrix";

/**
 * The SdlContext owns the SDL setup (from SDL_Init to SDL_Quit) and the window.
 */
class SdlContext : public IContext
{

public:

	SdlContext()
	{
		int init_result = SDL_Init(SDL_INIT_EVERYTHING);
		game_assert(0 == init_result, SDL_GetError());

		if(!IMG_Init(IMG_INIT_PNG)) {
			SDL_Quit();
			throw GameException(IMG_GetError());
		}

		screen = Window(SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CANVAS_W, CANVAS_H, 0));
		game_assert(static_cast<bool>(screen), SDL_GetError());

		renderer = Renderer(SDL_CreateRenderer(screen.get(), -1, 0));
		game_assert(static_cast<bool>(renderer), SDL_GetError());

		assets = std::make_unique<Assets>(renderer);
		fadetex = Texture(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 1, 1)); // 1x1 pixel for fading
		game_assert(static_cast<bool>(fadetex), SDL_GetError());

		audio = std::make_unique<Audio>();

		int mode_result = SDL_SetTextureBlendMode(fadetex.get(), SDL_BLENDMODE_BLEND);
		game_assert(0 == mode_result, SDL_GetError());
	}

	~SdlContext()
	{
		IMG_Quit();
		SDL_Quit();
	}

	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const override
	{
		TextRect tr = assets->texture(gfx, frame);
		SDL_Rect dstrect = *tr.rect;
		dstrect.x = static_cast<int>(loc.x);
		dstrect.y = static_cast<int>(loc.y);

		int render_result = SDL_RenderCopy(renderer.get(), tr.texture, nullptr, &dstrect);
		game_assert(0 == render_result, SDL_GetError());
	}

	virtual void clip(Point top_left, int width, int height) override
	{
		int x = static_cast<int>(top_left.x);
		int y = static_cast<int>(top_left.y);
		SDL_Rect clip_rect{x, y, width, height};

		int clip_result = SDL_RenderSetClipRect(renderer.get(), &clip_rect);
		game_assert(0 == clip_result, SDL_GetError());
	}

	virtual void unclip() override
	{
		int clip_result = SDL_RenderSetClipRect(renderer.get(), nullptr);
		game_assert(0 == clip_result, SDL_GetError());
	}

	virtual void fade(float fraction) override
	{
		m_fade = fraction;
	}

	virtual void play(Snd snd) override
	{
		Sound sound = assets->sound(snd);
		audio->play(sound);
	}

	/**
	 * Puts the rendered scene on screen
	 */
	void render() const
	{
		if(m_fade < 1.f) {
			SDL_Rect rect_pixel{0,0,1,1};
			uint32_t fade_pixel = static_cast<uint32_t>(0xff * (1.f - m_fade));
			int tex_result = SDL_UpdateTexture(fadetex.get(), &rect_pixel, &fade_pixel, 1);
			game_assert(0 == tex_result, SDL_GetError());

			int render_result = SDL_RenderCopy(renderer.get(), fadetex.get(), nullptr, nullptr);
			game_assert(0 == render_result, SDL_GetError());
		}

		SDL_RenderPresent(renderer.get());

		// clear for next frame
		int render_result = SDL_RenderClear(renderer.get());
		game_assert(0 == render_result, SDL_GetError());
	}

private:

	Window screen;
	Renderer renderer;
	std::unique_ptr<Assets> assets;
	std::unique_ptr<Audio> audio;

	float m_fade = 1.f;
	Texture fadetex; // solid pixel used for fading

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
