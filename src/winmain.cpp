#include "game_loop.hpp"
#include "error.hpp"
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

	for (size_t i = 0; i < argc; i++) {
		int bufferSize = static_cast<int>((wcslen(wargv[i]) + 1) * sizeof(wchar_t));
		char* buffer = new char[bufferSize];
		int converted = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, buffer, bufferSize, nullptr, nullptr);
		argv[i] = buffer;
	}

	Options options(argc, &argv[0]);
	Log::init(create_file_log(options.log_path()));

	try 	{
		GameLoop loop(options);
		loop.game_loop();
	}
	catch(const std::exception& ex) {
		show_error(ex);
	}
	catch(...) {
		Log::error("Unknown exception occurred.");
	}

	for (const char* arg : argv)
		delete arg;

	return 0;
}
