#include "block.hpp"
#include "context.hpp"
#include "asset.hpp"
#include <iostream>
#include <SDL2/SDL.h>
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

	virtual void drawGfx(Gfx gfx, Point loc) const
	{
		TextRect tr = assets->texture(gfx);
		SDL_Rect dstrect = *tr.rect;
		dstrect.x = loc.x;
		dstrect.y = loc.y;

		int render_result = SDL_RenderCopy(renderer.get(), tr.texture, NULL, &dstrect);
		game_assert(0 == render_result, SDL_GetError());

		std::cerr << "draw gfx " << static_cast<int>(gfx) << " at point x=" << loc.x << ", y=" << loc.y << "\n";
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

int main()
{
	SdlContext context;
	StageBuilder builder;
	Stage stage = builder.construct();

	bool quit = false;
	while(!quit) {
		stage.draw(context, 0.f);
		context.render();
		SDL_Event event;
		if(SDL_WaitEvent(&event)) {
			if(event.type == SDL_QUIT) {
				quit = true;
			}
		}
		else {
			SDL_assert_release(false);
		}
	}

	return 0;
}
