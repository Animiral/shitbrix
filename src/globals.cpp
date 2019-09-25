#include "globals.hpp"
#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include "error.hpp"

Gfx operator+(Gfx gfx, int delta)
{
	return static_cast<Gfx>(static_cast<int>(gfx) + delta);
}

BlockFrame& operator++(BlockFrame& frame)
{
	return frame = static_cast<BlockFrame>(static_cast<size_t>(frame) + 1);
}

namespace
{

const char* gamebutton_string[] =
{"none", "left", "right", "up", "down", "swap", "raise"};

}

const char* game_button_to_string(GameButton button) noexcept
{
	const size_t button_index = static_cast<size_t>(button);
	assert(button_index < std::size(gamebutton_string));
	return gamebutton_string[button_index];
}

GameButton string_to_game_button(const std::string& button_string)
{
	const auto button_found = std::find(gamebutton_string, std::end(gamebutton_string), button_string);
	const size_t button_index = std::distance(gamebutton_string, button_found);

	if(std::size(gamebutton_string) <= button_index)
		throw GameException("Invalid game button string: \"" + button_string + "\"");

	return static_cast<GameButton>(button_index);
}

const char* button_action_to_string(ButtonAction action) noexcept
{
	switch(action) {
		case ButtonAction::UP: return "release";
		case ButtonAction::DOWN: return "press";
		default: assert(false); return nullptr;
	}
}

ButtonAction string_to_button_action(const std::string& action_string)
{
	if("release" == action_string) return ButtonAction::UP;
	else if("press" == action_string) return ButtonAction::DOWN;
	else throw GameException("Invalid button action string: \"" + action_string + "\"");
}

namespace
{

const char* color_string[] =
{ "fake", "blue", "red", "yellow", "green", "purple", "orange" };

}

std::string color_to_string(Color color) noexcept
{
	const size_t color_index = static_cast<size_t>(color);
	assert(color_index < std::size(color_string));
	return color_string[color_index];
}

Color string_to_color(const std::string& source)
{
	const auto color_found = std::find(color_string, std::end(color_string), source);
	const size_t color_index = std::distance(color_string, color_found);

	if(std::size(color_string) <= color_index)
		throw GameException("Invalid color string: \"" + source + "\"");

	return static_cast<Color>(color_index);
}

std::string GameMeta::to_string() const
{
	std::ostringstream ss;
	ss << players << " " << seed << " " << winner;
	return ss.str();
}

GameMeta GameMeta::from_string(std::string meta_string)
{
	std::istringstream tokenizer(meta_string);
	int players;
	unsigned seed;
	int winner;

	tokenizer >> players >> seed >> winner;
	if(!tokenizer)
		throw GameException("Invalid GameMeta string: \"" + meta_string + "\"");

	return GameMeta{players, seed, winner};
}

Point from_rc(RowCol rc)
{
	return Point{static_cast<float>(rc.c*BLOCK_W), static_cast<float>(rc.r*BLOCK_H)};
}

std::ostream& operator<<(std::ostream& stream, RowCol rc)
{
	return stream << "{r" << rc.r << "c" << rc.c << "}";
}

#ifdef __linux__

#include <sys/prctl.h>
#include <cstring>

void set_thread_name(const char* thread_name)
{
	int result = prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(thread_name), 0, 0, 0);

	if(0 != result) {
		throw GameException(std::strerror(result));
	}
}

#elif defined(_WIN32) || defined(_WIN64)

// This magic sauce code was taken from:
// https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code

#include <windows.h>

#pragma pack(push,8)
struct THREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
};
#pragma pack(pop)

void set_thread_name(const char* thread_name)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = thread_name;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma warning(pop)
}

#else

void set_thread_name(const char* )
{
	// not implemented for this platform
}

#endif // platform switches
