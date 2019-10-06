#include "error.hpp"
#include "globals.hpp"
#include "context.hpp"
#include "asset.hpp"
#include "sdl_helper.hpp"
#include "text.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <ctime>
#include <cstdarg>
#include <cassert>
#include <mutex>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

namespace
{

/**
 * Replace spaces with line breaks after at least the given number of characters in each line.
 */
void auto_linebreaks(std::string& str, int n);

}

void enforce_impl(bool condition, const char* condition_str, const char* func, const char* file, int line)
{
	if(!condition)
		throwx<EnforceException>(condition_str, func, file, line);
}

bool on_failure_break_into_debugger = true;

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
	std::string what;

	// put the error message in the log file
	if(const GameException* game_ex = dynamic_cast<const GameException*>(&exception)) {
		what = string_format("%s: %s", game_ex->class_name(), game_ex->what());
	} else {
		what = string_format("%s", exception.what());
	}

	Log::error("%s", what.c_str());

	// display to the user, if we have SDL available.
	Uint32 init = SDL_WasInit(SDL_INIT_EVERYTHING);

	if(init & SDL_INIT_VIDEO) {
		SDL_Renderer* renderer = &the_context.sdl->renderer();
		SDL_Event event;
		if(0 == SDL_WaitEvent(&event)) return;

		auto_linebreaks(what, 40);
		TtfText what_text(*the_context.sdl, the_context.assets->ttf_font(), what.c_str());

		do {
			// rects
			SDL_Rect outer_rect{30, 30, CANVAS_W - 60, CANVAS_H - 60};
			SDL_Rect inner_rect{60, 60, CANVAS_W - 120, CANVAS_H - 120};
			if(0 != SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE)) break;
			if(0 != SDL_RenderFillRect(renderer, &outer_rect)) break;
			if(0 != SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE)) break;
			if(0 != SDL_RenderFillRect(renderer, &inner_rect)) break;

			// text
			SDL_Texture& tex = what_text.texture();
			Uint32 format;
			int access;
			int w;
			int h;
			sdlok(SDL_QueryTexture(&tex, &format, &access, &w, &h));
			const SDL_Rect dest_rect{ 70, CANVAS_H/2 - 40, w, h };
			sdlok(SDL_RenderCopy(renderer, &tex, NULL, &dest_rect));

			// finish
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

namespace Log
{

std::pair<std::string, std::string> write_prerequisites() noexcept
{
	// construct date and time string
	const size_t NOW_BUFSIZE = 20;
	std::string now_buffer(NOW_BUFSIZE, '\0');

	const time_t now = std::time(nullptr);
	const struct tm* local_now = std::localtime(&now);

	if(nullptr == local_now) {
		now_buffer = "?-?-? ?:?:?";
	}
	else {
		const size_t strftime_size = strftime(&now_buffer[0], NOW_BUFSIZE, "%Y-%m-%d %H:%M:%S", local_now);

		if(0 >= strftime_size) {
			now_buffer = "?-?-? ?:?:?";
		}
	}

	// construct thread id string
	std::ostringstream tid_stream;
	tid_stream << std::this_thread::get_id();

	return { move(now_buffer), tid_stream.str() };
}

void write_context(const std::string message) noexcept
{
	the_context.log->write(message);
}

}


GameException::GameException(const GameException& rhs)
	: m_what(rhs.m_what), m_cause()
{
	if(rhs.m_cause)
		m_cause = rhs.m_cause->clone();
}

GameException& GameException::operator=(const GameException& rhs)
{
	m_what = rhs.m_what;

	if(rhs.m_cause)
		m_cause = rhs.m_cause->clone();

	return *this;
}

std::unique_ptr<GameException> GameException::clone() const
{
	return std::make_unique<GameException>(*this);
}

SdlException::SdlException(const char* what)
: GameExceptionCloning("%s", what ? what : SDL_GetError())
{}

EnforceException::EnforceException(const char* condition, const char* func, const char* file, int line)
	: GameExceptionCloning("Enforced condition violated in %s (%s:%d), expression: \"%s\"", func, file, line, condition)
{}

namespace
{

void auto_linebreaks(std::string& str, int n)
{
	int last_break = 0;
	for(int i = 0; i < str.length(); i++) {
		if(last_break + n <= i && std::isspace(str[i])) {
			str[i] = '\n';
			last_break = i;
		}
	}
}

}
