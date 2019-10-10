#include "visualdemo.hpp"
#include "draw.hpp"
#include <set>
#include <cassert>

/**
 * A practical demo of the draw module.
 *
 * This demo serves as a test for the module instead of unit tests.
 * Rather than compare renderings to a "golden screenshot" reference,
 * this test passes if it looks right.
 *
 * When run through the VisualDemo application, the screen should show
 * a lineup of graphics assets used in the game. The different backgrounds
 * are mixed by an animated sliding window, which demonstrates the proper
 * working of the clipping functions. In addition, different-colored rectangles
 * demonstrate primitive drawing.
 */
void VisualDemo::draw_demo()
{
	Log::info("Visual Demo: draw demo start.");

	auto draw = std::make_unique<SdlDraw>(the_context.sdl->renderer(), *the_context.assets);
	auto canvas1 = draw->create_canvas();
	auto canvas2 = draw->create_canvas();

	// loop
	Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
	Uint64 freq = SDL_GetPerformanceFrequency();
	long tick = 0; // current logic tick counter
	Uint64 next_logic = t0 + freq / TPS; // time for next logic update
	bool exit = false;

	while (!exit) {
		// draw frames as long as logic is up to date
		Uint64 now = SDL_GetPerformanceCounter();
		while (now < next_logic) {
			float fraction = 1.0f - static_cast<float>((next_logic - now) * TPS) / freq;
			assert(fraction >= 0);
			assert(fraction <= 1);

			// DRAW STUFF

			// A. normal background screen
			canvas1->use_as_target();

			// A 1. background
			draw->gfx(0, 0, Gfx::BACKGROUND);

			// A 2. some gfx
			draw->gfx(50, 50, Gfx::BANNER);
			draw->gfx(200, 50, Gfx::BLOCK_BLUE, static_cast<size_t>(BlockFrame::PREVIEW));
			draw->gfx(250, 50, Gfx::BONUS, static_cast<size_t>(BonusFrame::CHAIN));

			// A 3. primitives
			draw->rect({ 450, 50, 40, 40 }, { 255, 0, 0, 80 }); // slight red
			draw->rect({ 500, 50, 40, 40 }, { 0, 255, 0, 160 }); // strong green
			draw->rect({ 550, 50, 40, 40 }, { 0, 0, 255, 255 }); // solid blue
			draw->highlight({ 450, 100, 40, 40 }, { 255, 0, 0, 80  }); // slight red
			draw->highlight({ 500, 100, 40, 40 }, { 0, 255, 0, 160 }); // strong green
			draw->highlight({ 550, 100, 40, 40 }, { 0, 0, 255, 255 }); // solid blue

			// B. menu background screen
			canvas2->use_as_target();

			// B 1. background
			draw->gfx(0, 0, Gfx::MENUBG);

			// B 2. some gfx
			draw->gfx(350, 250, Gfx::BANNER);
			draw->gfx(500, 250, Gfx::BLOCK_BLUE, static_cast<size_t>(BlockFrame::PREVIEW));
			draw->gfx(550, 250, Gfx::BONUS, static_cast<size_t>(BonusFrame::CHAIN));

			// B 3. primitives
			draw->rect({ 50, 250, 40, 40  }, { 255, 0, 0, 80  }); // slight red
			draw->rect({ 100, 250, 40, 40 }, { 0, 255, 0, 160 }); // strong green
			draw->rect({ 150, 250, 40, 40 }, { 0, 0, 255, 255 }); // solid blue
			draw->highlight({ 50, 300, 40, 40  }, { 255, 0, 0, 80  }); // slight red
			draw->highlight({ 100, 300, 40, 40 }, { 0, 255, 0, 160 }); // strong green
			draw->highlight({ 150, 300, 40, 40 }, { 0, 0, 255, 255 }); // solid blue

			// sliding window animation
			draw->reset_target();

			int border12 = tick % CANVAS_W;
			int border21 = (tick + CANVAS_W / 2) % CANVAS_W;
			draw->clip({ border12 - CANVAS_W, 0, CANVAS_W / 2, CANVAS_H }); // left side
			canvas1->draw();
			draw->clip({ border12, 0, CANVAS_W / 2, CANVAS_H }); // right side
			canvas1->draw();
			draw->clip({ border21 - CANVAS_W, 0, CANVAS_W / 2, CANVAS_H }); // left side
			canvas2->draw();
			draw->clip({ border21, 0, CANVAS_W / 2, CANVAS_H }); // right side
			canvas2->draw();
			draw->unclip();

			draw->render();

			// CONTINUE WITH TIMEKEEPING
			now = SDL_GetPerformanceCounter();

			// yield CPU if we have the time
			if(now < next_logic) {
				Uint64 wait = (next_logic - now) * 1000L / freq; // in ms
				assert(wait <= std::numeric_limits<Uint32>::max());
				SDL_Delay(static_cast<Uint32>(wait));
				now = SDL_GetPerformanceCounter();
			}
		}

		// get different sources of input
		SDL_Event event;

		while(SDL_PollEvent(&event)) {
			switch(event.type) {

			case SDL_QUIT: // overrides all other inputs
				exit = true;
				break;

			case SDL_KEYUP:
			case SDL_KEYDOWN:
				if(std::set<int>{SDLK_RETURN, SDLK_SPACE, SDLK_ESCAPE}.count(event.key.keysym.sym) > 0)
					exit = true;
				break;

			default:
				break;
			}
		}

		tick++;
		next_logic = t0 + (tick + 1) * freq / TPS;
	}

	Log::info("Visual Demo: draw demo exit.");
}
