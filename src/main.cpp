#include "screen.hpp"
#include "sdl_context.hpp"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>

/**
 * A screen transition which draws a larger and larger portion of the screen
 * contents from the successor screen.
 */
class SwipeTransition : public Transition
{

public:

	GridTransition(std::unique_ptr<IScreen> predecessor, std::unique_ptr<IScreen> successor, SdlFactory& factory)
	: Transition(std::move(predecessor), std::move(successor)),
	  m_fake_context(factory)
	{}

private:

	/**
	 * The purpose of this fake context is to make the client of the context
	 * (the screen) draw itself to a texture instead of the screen.
	 */
	class CaptureContext : public IContext
	{

	public:

		CaptureContext(SdlFactory& factory) : m_factory(factory)
		{
			m_capture.reset(SDL_CreateTexture(
				factory.get_renderer().get(),
				SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, CANVAS_W, CANVAS_H
				));
			 // in draw(), create texture, render both screens onto it
			// and interpolate from there
		}

	private:

		SdlFactory& m_factory;
		std::unique_ptr<SDL_Texture, SdlDeleter> m_capture;

	}

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
	  m_context(),
	  m_rndgen(),
	  m_input_mixer(m_options.replay_file()),
	  m_replay_outstream(make_journal_file().c_str()),
	  m_journal(m_replay_outstream),
	  m_screen(nullptr),
	  m_keyboard()
	{
		// If there is no replay, get a seed from random device
		if(!m_options.replay_file()) {
			std::random_device rdev;
			auto rng_seed = rdev();
			m_rndgen = std::make_shared<std::mt19937>(rng_seed);
			std::ostringstream stream;
			stream << rng_seed;
			m_journal << ReplayEvent::make_set("rng_seed", stream.str());
		}
		else {
			m_rndgen = std::make_shared<std::mt19937>();
		}

		m_screen = make_splash_screen();
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
		SDL_assert(m_screen);

		Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
		Uint64 freq = SDL_GetPerformanceFrequency();
		long tick = 0; // current logic tick counter
		Uint64 next_logic = t0 + freq / TPS; // time for next logic update

		do {
			while(!m_screen->done()) {
				// draw frames as long as logic is up to date
				Uint64 now = SDL_GetPerformanceCounter();
				while(now < next_logic) {
					float fraction = 1.0f - static_cast<float>((next_logic-now) * TPS) / freq;
					SDL_assert((fraction >= 0) && (fraction <= 1));

					m_screen->draw(fraction);
					m_context.render();
					now = SDL_GetPerformanceCounter();
				}

				m_keyboard.poll();

				// run one frame of local logic
				m_screen->update();

				tick++;
				next_logic = t0 + (tick+1) * freq / TPS;
			}

			// choose next screen
			if(Transition* transition = dynamic_cast<Transition*>(m_screen.get())) {
				m_screen = transition->successor();
			}
			else if(dynamic_cast<SplashScreen*>(m_screen.get())) {
				auto game_screen = make_game_screen();
				m_screen = make_transition(std::move(game_screen));
			}
			else if(GameScreen* game = dynamic_cast<GameScreen*>(m_screen.get())) {
				if(game->is_quit()) {
					m_screen = nullptr;
				}
				else {
					auto game_screen = make_game_screen();
					m_screen = make_transition(std::move(game_screen));
				}
			}
			else {
				SDL_assert(false);
				m_screen = nullptr;
			}
		}
		while(m_screen);
	}

private:

	Options m_options;

	SdlContext m_context;
	RndGen m_rndgen;
	GameInputMixer m_input_mixer;
	std::ofstream m_replay_outstream;
	Journal m_journal;

	std::unique_ptr<IScreen> m_screen; //!< active screen

	Keyboard m_keyboard;

	void unplug_all()
	{
		m_input_mixer.set_game_sink(nullptr);
		m_input_mixer.set_replay_sink(nullptr);
		m_keyboard.set_game_sink(nullptr);
	}

	std::unique_ptr<SplashScreen> make_splash_screen()
	{
		unplug_all();

		auto screen = std::make_unique<SplashScreen>();
		screen->set_context(m_context);
		return screen;
	}

	std::unique_ptr<GameScreen> make_game_screen()
	{
		unplug_all();

		auto screen = std::make_unique<GameScreen>();

		// connect all the components
		m_input_mixer.set_replay_sink(screen.get());
		m_keyboard.set_game_sink(screen.get());
		screen->set_context(m_context);
		screen->set_rndgen(m_rndgen);
		screen->set_input(m_input_mixer);
		screen->set_journal(m_journal);

		return screen;
	}

	std::unique_ptr<Transition> make_transition(std::unique_ptr<IScreen> successor)
	{
		auto screen = std::make_unique<Transition>(std::move(m_screen), std::move(successor));
		screen->set_context(m_context);
		return screen;
	}

};

int main(int argc, const char* argv[])
{
	Options options(argc, argv);
	Main main(options);
	main.game_loop();
	return 0;
}
