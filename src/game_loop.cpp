#include "game_loop.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

Options::Options(int argc, const char* argv[])
: m_replay_file(str_option(argc, argv, "--replay"))
{
}

const char* Options::replay_file() const
{
	return m_replay_file;
}

// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
const char* Options::str_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	const char** itr = std::find(argv, end, option);
	if (itr != end && ++itr != end)
	{
		return *itr;
	}
	return nullptr;
}

bool Options::bool_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	return std::find(argv, end, option) != end;
}

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
	struct tm ltime;

	if (localtime_s(&ltime, &time_now)) {
		std::ostringstream stream;
		stream << std::put_time(&ltime, "replay/%Y-%m-%d_%H-%M.txt");
		return stream.str();
	}
	else {
		return "replay/default.txt";
	}
}

}

GameLoop::GameLoop(Options options)
: m_options(std::move(options)),
  m_factory(),
  m_assets(m_factory),
  m_draw(m_factory, m_assets),
  m_sound(*m_factory.get_audio(), m_assets),
  m_screen(m_options.replay_file(), make_journal_file().c_str(), m_draw, m_sound),
  m_keyboard(m_screen)
{
}

void GameLoop::game_loop()
{
	Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
	Uint64 freq = SDL_GetPerformanceFrequency();
	long tick = 0; // current logic tick counter
	Uint64 next_logic = t0 + freq / TPS; // time for next logic update

	while (!m_screen.done()) {
		// draw frames as long as logic is up to date
		Uint64 now = SDL_GetPerformanceCounter();
		while (now < next_logic) {
			float fraction = 1.0f - static_cast<float>((next_logic - now) * TPS) / freq;
			SDL_assert((fraction >= 0) && (fraction <= 1));

			m_draw.draw_all(fraction);
			now = SDL_GetPerformanceCounter();
		}

		m_keyboard.poll();

		// run one frame of local logic
		m_screen.update();

		tick++;
		next_logic = t0 + (tick + 1) * freq / TPS;
	}
}
