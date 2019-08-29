#define NOMINMAX

#include "game_loop.hpp"
#include "game.hpp"
#include "configuration.hpp"
#include "error.hpp"
#include "context.hpp"

namespace
{

/**
 * Cross-platform main function.
 */
void game_main(int argc, const char* argv[]) noexcept;

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
		delete[] arg;

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

		configure_context(configuration);

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

}
