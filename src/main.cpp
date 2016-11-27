#include "screen.hpp"
#include "context.hpp"
#include "asset.hpp"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>
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

	virtual void highlight(Point top_left, int width, int height) override
	{
		int x = top_left.x;
		int y = top_left.y;
		SDL_Rect fill_rect{x, y, width, height};

		SDL_Renderer* renderer = factory.get_renderer().get();
		int color_result = SDL_SetRenderDrawColor(renderer, 200, 200, 0, 150);
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

class Options
{

public:
	Options(int argc, const char* argv[])
		: m_replay_file(str_option(argc, argv, "--replay"))
	{
	}

	const char* replay_file() const { return m_replay_file; }

private:
	const char* m_replay_file;

	// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
	const char* str_option(int argc, const char* argv[], const std::string& option)
	{
		auto end = argv + argc;
	    const char** itr = std::find(argv, end, option);
	    if (itr != end && ++itr != end)
	    {
	        return *itr;
	    }
	    return nullptr;
	}

	bool bool_option(int argc, const char* argv[], const std::string& option)
	{
		auto end = argv + argc;
	    return std::find(argv, end, option) != end;
	}

};

namespace
{

/**
 * Returns the default name of the file where the replay of this session will
 * be stored. This default name is built from the current date and time.
 */
std::string make_journal_file()
{
	using clock = std::chrono::system_clock;
	auto now = clock::now();
	std::time_t time_now = clock::to_time_t(now);
	auto ltime = std::localtime(&time_now);
	std::ostringstream stream;
	stream << std::put_time(ltime, "replay/%Y-%m-%d_%H-%M.txt");
	return stream.str();
}

}

/**
 * Top-level class which owns general application resources such as the initialized SDL library
 * and offers the main loop function.
 */
class Main
{

public:

	Main(Options options)
		: m_options(std::move(options)),
		  context(),
		  game_screen(m_options.replay_file(), make_journal_file().c_str(), context),
		  keyboard(game_screen)
	{
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

		while(!game_screen.done()) {
			// draw frames as long as logic is up to date
			Uint64 now = SDL_GetPerformanceCounter();
			while(now < next_logic) {
				float fraction = 1.0f - static_cast<float>((next_logic-now) * TPS) / freq;
				SDL_assert((fraction >= 0) && (fraction <= 1));

				game_screen.draw(fraction);
				context.render();
				now = SDL_GetPerformanceCounter();
			}

			keyboard.poll();

			// run one frame of local logic
			game_screen.animate();
			game_screen.update();

			tick++;
			next_logic = t0 + (tick+1) * freq / TPS;
		}
	}

private:

	Options m_options;
	SdlContext context;
	GameScreen game_screen;
	Keyboard keyboard;

};

int main(int argc, const char* argv[])
{
	Options options(argc, argv);
	Main main(options);
	main.game_loop();
	return 0;
}
