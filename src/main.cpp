#include "block.hpp"
#include "director.hpp"
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
class SdlContext : public IVideoContext
{

private:

	std::unique_ptr<Assets> assets;
	Window screen;
	Renderer renderer;

public:

	SdlContext()
	{
		int init_result = SDL_Init(SDL_INIT_EVERYTHING);
		game_assert(0 == init_result, "SDL initialization failed.");

		if(!IMG_Init(IMG_INIT_PNG)) {
			SDL_Quit();
			throw GameException("SDL_image initialization failed.");
		}

		screen = Window(SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CANVAS_W, CANVAS_H, 0));
		game_assert(static_cast<bool>(screen), SDL_GetError());

		renderer = Renderer(SDL_CreateRenderer(screen.get(), -1, 0));
		game_assert(static_cast<bool>(renderer), SDL_GetError());

		assets = std::make_unique<Assets>(renderer);
	}

	~SdlContext()
	{
		IMG_Quit();
		SDL_Quit();
	}

	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const
	{
		TextRect tr = assets->texture(gfx, frame);
		SDL_Rect dstrect = *tr.rect;
		dstrect.x = static_cast<int>(loc.x);
		dstrect.y = static_cast<int>(loc.y);

		int render_result = SDL_RenderCopy(renderer.get(), tr.texture, NULL, &dstrect);
		game_assert(0 == render_result, SDL_GetError());
	}

	/**
	 * Puts the rendered scene on screen
	 */
	void render() const
	{
		SDL_RenderPresent(renderer.get());

		// clear for next frame
		int render_result = SDL_RenderClear(renderer.get());
		game_assert(0 == render_result, SDL_GetError());
	}
};

/**
 * Top-level class which owns general application resources such as the initialized SDL library
 * and offers the main loop function.
 */
class Main
{

public:

	Main() : context()
	{
		auto builder = StageBuilder();
		stage = builder.construct();

		left_blocks = std::make_unique<BlockDirector>(stage, builder.left_pit);
		right_blocks = std::make_unique<BlockDirector>(stage, builder.right_pit);
		left_cursor = std::make_unique<CursorDirector>(builder.left_pit, builder.left_cursor);
	}

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
				stage->draw(context, fraction);
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
								case SDLK_LEFT: left_cursor->move(Dir::LEFT); break;
								case SDLK_RIGHT: left_cursor->move(Dir::RIGHT); break;
								case SDLK_UP: left_cursor->move(Dir::UP); break;
								case SDLK_DOWN: left_cursor->move(Dir::DOWN); break;
							}
						}
						break;
				}
			}

			left_cursor->move(Dir::NONE); // auto-move cursor when scrolling out of bounds

			// run one frame of local logic
			stage->animate();
			left_blocks->update();
			right_blocks->update();
			stage->update();
			tick++;
			next_logic = t0 + (tick+1) * freq / TPS;
		}

		quit:;
	}

private:

	SdlContext context;
	Stage stage; // to be replaced by app-state object (e.g. menu, in-game etc.)
	std::unique_ptr<BlockDirector> left_blocks;
	std::unique_ptr<BlockDirector> right_blocks;
	std::unique_ptr<CursorDirector> left_cursor;

};

int main()
{
	Main main;
	main.game_loop();
	return 0;
}
