#include "game_loop.hpp"
#include "configuration.hpp"
#include "error.hpp"
#include "context.hpp"
#include "audio.hpp"

namespace
{

/**
 * Cross-platform main function.
 */
void game_main(int argc, const char* argv[]) noexcept;

/**
 * Instantiate the members of the global context based on the configuration.
 */
void configure_context(Configuration configuration);

}

#ifdef _WIN32

#include <windows.h>
#include <vector>
#include <cassert>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd)
{
	LPWSTR command_line = GetCommandLineW();
	int argc = 0;
	LPWSTR* wargv = CommandLineToArgvW(command_line, &argc);
	assert(wargv);

	std::vector<const char*> argv(argc);

	for (int i = 0; i < argc; i++) {
		int bufferSize = static_cast<int>((wcslen(wargv[i]) + 1) * sizeof(wchar_t));
		char* buffer = new char[bufferSize];
		int converted = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, buffer, bufferSize, nullptr, nullptr);
		argv[i] = buffer;
	}

	game_main(argc, &argv[0]);

	for (const char* arg : argv)
		delete arg;

	return 0;
}

#else

int main(int argc, const char* argv[])
{
	game_main(argc, argv);
	return 0;
}

#endif

namespace
{

void game_main(int argc, const char* argv[]) noexcept
{
	try {
		Configuration configuration;
		const std::filesystem::path CONFIG_PATH{std::string(APP_NAME) + ".conf"};
		if(std::filesystem::is_regular_file(CONFIG_PATH)) {
			configuration.read_from_file(CONFIG_PATH);
		}
		configuration.read_from_args(argc, argv);

		configure_context(std::move(configuration));

		GameLoop loop;
		loop.game_loop();
	}
	catch(const std::exception& ex) {
		if(the_context.log)
			show_error(ex);
		else
			std::exit(1);
	}
	catch(...) {
		if(the_context.log)
			Log::error("Unknown exception occurred.");
		else
			std::exit(1);
	}
}

void configure_context(Configuration configuration)
{
	the_context.configuration.reset(new Configuration(std::move(configuration)));

	const bool is_server_only = NetworkMode::SERVER == the_context.configuration->network_mode;
	Uint32 sdl_flags = is_server_only ? SDL_INIT_TIMER | SDL_INIT_EVENTS
	                                  : SDL_INIT_EVERYTHING;

	the_context.sdl.reset(new Sdl(sdl_flags));
	the_context.log = create_file_log(the_context.configuration->log_path);

	if(is_server_only) {
		the_context.assets.reset(new NoAssets);
		the_context.audio.reset(new NoAudio);
	}
	else {
		the_context.assets.reset(new FileAssets);
		the_context.audio.reset(new SdlAudio(the_context.sdl->audio()));
	}
}

}
