#include "error.hpp"
#include "globals.hpp"
#include "sdl_helper.hpp"
#include <fstream>
#include <ctime>
#include <cassert>
#include <mutex>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

SdlException::SdlException()
: GameException(SDL_GetError())
{
}

void enforce_impl(bool condition, const char* condition_str, const char* func, const char* file, int line)
{
	if(!condition)
		throw EnforceException(condition_str, func, file, line);
}

void sdlok(int result)
{
	if(0 != result)
		throw SdlException();
}

void sdlok(void* pointer)
{
	if(!pointer)
		throw SdlException();
}

void imgok(void* pointer)
{
	if(!pointer)
		throw SdlException(IMG_GetError());
}

void enetok_impl(int result, const char* what)
{
	if(0 != result)
		throw ENetException(what);
}

void enetok_impl(void* pointer, const char* what)
{
	if(!pointer)
		throw ENetException(what);
}

void show_error(const std::exception& exception) noexcept
{
	// put the error message in the log file
	if(const LogicException* logic_ex = dynamic_cast<const LogicException*>(&exception)) {
		Log::error("%s: %s", logic_ex->class_name(), logic_ex->what());
	} else
	if(const ReplayException* replay_ex = dynamic_cast<const ReplayException*>(&exception)) {
		Log::error("%s: %s", replay_ex->class_name(), replay_ex->what());
	} else
	if(const SdlException* sdl_ex = dynamic_cast<const SdlException*>(&exception)) {
		Log::error("%s: %s", sdl_ex->class_name(), sdl_ex->what());
	} else
	if(const EnforceException* enforce_ex = dynamic_cast<const EnforceException*>(&exception)) {
		Log::error("%s: %s in %s (%s:%d), expression: \"%s\"",
		           enforce_ex->class_name(), enforce_ex->what(), enforce_ex->m_func,
		           enforce_ex->m_file, enforce_ex->m_line, enforce_ex->m_condition);
	} else
	if(const GameException* game_ex = dynamic_cast<const GameException*>(&exception)) {
		Log::error("%s: %s", game_ex->class_name(), game_ex->what());
	} else {
		Log::error("%s", exception.what());
	}

	// display to the user, if we have SDL available.
	Uint32 init = SDL_WasInit(SDL_INIT_EVERYTHING);

	if(init & SDL_INIT_VIDEO) {
		SDL_Renderer* renderer = &Sdl::instance().renderer();
		SDL_Event event;
		if(0 == SDL_WaitEvent(&event)) return;

		do {
			// NOTE: there is no text support in the application yet,
			//       so instead of the actual error message, we display a red rectangle instead.
			SDL_Rect outer_rect{30, 30, CANVAS_W - 60,CANVAS_H - 60};
			SDL_Rect inner_rect{60, 60, CANVAS_W - 120,CANVAS_H - 120};
			if(0 != SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE)) break;
			if(0 != SDL_RenderFillRect(renderer, &outer_rect)) break;
			if(0 != SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE)) break;
			if(0 != SDL_RenderFillRect(renderer, &inner_rect)) break;
			SDL_RenderPresent(renderer);

			if(SDL_QUIT == event.type ||
				(SDL_KEYDOWN == event.type &&
				(SDLK_ESCAPE == event.key.keysym.sym || SDLK_RETURN == event.key.keysym.sym))) {
				break;
			}
		} while(SDL_WaitEvent(&event));
	}
}


Log::~Log() =default;

/**
 * Log to file implementation.
 */
class LogFile : public LogImpl
{

public:

	explicit LogFile(const char* path)
	{
		m_stream.rdbuf()->pubsetbuf(nullptr, 0); // make unbuffered
		m_stream.open(path, std::ios_base::out | std::ios_base::app);
	}

	virtual void write(const std::string& message) noexcept override
	{
		// we don't care to check errors as the log is best-effort
		m_stream << message << "\n";
		m_stream.flush();
	}

private:

	std::ofstream m_stream;

};

std::unique_ptr<LogImpl> create_file_log(const char* path)
{
	return std::make_unique<LogFile>(path);
}

void Log::init(std::unique_ptr<LogImpl> impl)
{
	m_instance.reset(new Log(move(impl)));
	info("Log initialized.");
}

void Log::trace(const char *format, ...) noexcept
{
	if(m_instance) {
		va_list vlist;
		va_start(vlist, format);
		m_instance->write("TRACE", format, vlist);
		va_end(vlist);
	}
}

void Log::info(const char *format, ...) noexcept
{
	if(m_instance) {
		va_list vlist;
		va_start(vlist, format);
		m_instance->write("INFO", format, vlist);
		va_end(vlist);
	}
}

void Log::error(const char *format, ...) noexcept
{
	if(m_instance) {
		va_list vlist;
		va_start(vlist, format);
		m_instance->write("ERROR", format, vlist);
		va_end(vlist);
	}
}

void Log::write(const char* level, const char *format, va_list vlist) const noexcept
{
	const size_t level_size = strlen(level);
	const int format_size = vsnprintf(NULL, 0, format, vlist);

	if(format_size < 0)
		return;

	const time_t now = std::time(nullptr);
	const struct tm* local_now = std::localtime(&now);

	if(nullptr == local_now)
		return;

	const size_t NOW_BUFSIZE = 20;
	std::string now_buffer(NOW_BUFSIZE, '\0');
	const size_t strftime_size = strftime(&now_buffer[0], NOW_BUFSIZE, "%Y-%m-%d %H:%M:%S", local_now);

	if(0 >= strftime_size)
		return;

	std::string buffer(strftime_size + level_size + format_size + 5, '\0');
	const int sprintf_result = sprintf(&buffer[0], "%s [%s] ", now_buffer.c_str(), level);

	if(sprintf_result < 0)
		return;

	const int vsprintf_result = vsprintf(&buffer[strftime_size + level_size + 4], format, vlist);

	if(vsprintf_result < 0)
		return;

	buffer.resize(buffer.size() - 1); // drop trailing '\0' from vsprintf

	// Thread safety: from here on, only one thread at a time can write.
	static std::mutex write_mutex;
	std::lock_guard<std::mutex> lock(write_mutex);

	m_impl->write(buffer);
}

Log::Log(std::unique_ptr<LogImpl> impl)
: m_impl(move(impl))
{
	assert(m_impl);
}

std::unique_ptr<Log> Log::m_instance;
