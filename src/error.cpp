#include "error.hpp"
#include "globals.hpp"
#include "context.hpp"
#include "sdl_helper.hpp"
#include <fstream>
#include <ctime>
#include <cstdarg>
#include <cassert>
#include <mutex>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

GameException::GameException(std::string what, std::unique_ptr<GameException> cause)
	: m_what(std::move(what)), m_cause(std::move(cause))
{
}

GameException::GameException(const GameException& rhs)
	: m_what(rhs.m_what), m_cause()
{
	if(rhs.m_cause)
		m_cause = rhs.m_cause->clone();
}

std::unique_ptr<GameException> GameException::clone() const
{
	if(m_cause)
		return std::make_unique<GameException>(m_what, m_cause->clone());
	else
		return std::make_unique<GameException>(m_what);
}

ReplayException::ReplayException(std::string what, std::unique_ptr<GameException> cause)
	: GameException(std::move(what), std::move(cause))
{}

SdlException::SdlException()
: GameException(SDL_GetError())
{
}

EnforceException::EnforceException(const char* condition, const char* func, const char* file, int line)
	: GameException("Enforced condition violated"),
	  m_condition(condition), m_func(func), m_file(file), m_line(line)
{}


void enforce_impl(bool condition, const char* condition_str, const char* func, const char* file, int line)
{
	if(!condition)
		throwx<EnforceException>(condition_str, func, file, line);
}

void sdlok(int result)
{
	if(0 != result)
		throwx<SdlException>();
}

void sdlok(void* pointer)
{
	if(!pointer)
		throwx<SdlException>();
}

void imgok(void* pointer)
{
	if(!pointer)
		throwx<SdlException>(IMG_GetError());
}

void ttfok(void* pointer)
{
	if(!pointer)
		throwx<SdlException>(TTF_GetError());
}

void enetok_impl(int result, const char* what)
{
	if(0 != result)
		throwx<ENetException>(what);
}

void enetok_impl(void* pointer, const char* what)
{
	if(!pointer)
		throwx<ENetException>(what);
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
		SDL_Renderer* renderer = &the_context.sdl->renderer();
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


/**
 * Stub logger implementation.
 */
class NoLogger : public Logger
{
public:
	virtual void write(const std::string& message) noexcept override {}
};


/**
 * Log to file implementation.
 */
class FileLogger : public Logger
{

public:

	explicit FileLogger(std::filesystem::path path)
	{
		m_stream.rdbuf()->pubsetbuf(nullptr, 0); // make unbuffered
		m_stream.open(path, std::ios_base::out | std::ios_base::app);
		write("Log initialized.");
	}

	virtual void write(const std::string& message) noexcept override
	{
		try {
			// Thread safety: from here on, only one thread at a time can write.
			std::lock_guard<std::mutex> lock{m_mutex};

			// we don't care to check errors as the log is best-effort
			m_stream << message << "\n";
			m_stream.flush();
		}
		catch(const std::exception& ) {
			// We never propagate exceptions out of the Logger as it is already
			// our last-ditch reporting facility.
		}
	}

private:

	std::mutex m_mutex; //!< Only one thread at a time can write
	std::ofstream m_stream;

};

std::unique_ptr<Logger> create_no_log()
{
	return std::make_unique<NoLogger>();
}

std::unique_ptr<Logger> create_file_log(std::filesystem::path path)
{
	return std::make_unique<FileLogger>(path);
}

void Log::trace(const char *format, ...) noexcept
{
	va_list vlist;
	va_start(vlist, format);
	write("TRACE", format, vlist);
	va_end(vlist);
}

void Log::info(const char *format, ...) noexcept
{
	va_list vlist;
	va_start(vlist, format);
	write("INFO", format, vlist);
	va_end(vlist);
}

void Log::error(const char *format, ...) noexcept
{
	va_list vlist;
	va_start(vlist, format);
	write("ERROR", format, vlist);
	va_end(vlist);
}

void Log::write(const char* level, const char *format, va_list vlist) noexcept
{
	const size_t level_size = strlen(level);
	va_list vlist2;
	va_copy(vlist2, vlist);
	const int format_size = vsnprintf(NULL, 0, format, vlist2);

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

	the_context.log->write(buffer);
}
