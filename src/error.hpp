/**
 * Error handling and logging facilities.
 */
#pragma once

#include <exception>
#include <memory>
#include <string>

// NOTE: we use the standard assert macro for never-happens conditions.

/**
 * General exception for all types of errors that occur in the game.
 */
struct GameException : public std::exception
{
	explicit GameException(const char* what = "") : m_what(what) {}
	virtual const char* class_name() const noexcept { return "GameException"; }
	virtual const char* what() const override { return m_what; }
	const char* m_what;
};

/**
 * Invalid game states encountered while evaluating game logic.
 * This can point to invalid setup of pit contents.
 */
struct LogicException : public GameException
{
	explicit LogicException(const char* what = "") : GameException(what) {}
	virtual const char* class_name() const noexcept override { return "LogicException"; }
};

/**
 * Problems in reading and parsing replays.
 */
struct ReplayException : public GameException
{
	explicit ReplayException(const char* what = "") : GameException(what) {}
	virtual const char* class_name() const noexcept override { return "ReplayException"; }
};

/**
 * Umbrella exception for error conditions that arise from use of the SDL library.
 * Their common feature is that they can not be handled and we get the error
 * message from the library.
 */
struct SdlException : public GameException
{
	/**
	 * Constructor that takes the error message from @c SDL_GetError.
	 */
	SdlException();

	/**
	 * Constructor with custom error message.
	 */
	explicit SdlException(const char* what) : GameException(what) {}

	virtual const char* class_name() const noexcept override { return "SdlException"; }
};

/**
 * Exception for violated input expectations and contracts.
 */
struct EnforceException : public GameException
{
	explicit EnforceException(const char* condition, const char* func, const char* file, int line)
		: GameException("Enforced condition violated"), m_condition(condition), m_func(func), m_file(file), m_line(line) {}
	virtual const char* class_name() const noexcept override { return "EnforceException"; }
	const char* m_condition;
	const char* m_func;
	const char* m_file;
	int m_line;
};

/**
 * Evaluate the condition and throw an @c EnforceException if it is false.
 * Intended for use through the @c enforce macro.
 */
void enforce_impl(bool condition, const char* condition_str, const char* func, const char* file, int line);

// expression-to-string helper
#define STR1(x) #x
#define SB_STRINGIZE(x) STR1(x)

// align compilers
#ifdef __GNUC__
#elif __MINGW32__
#define SB_FUNC __PRETTY_FUNCTION__
#elif _MSC_VER
#define SB_FUNC __FUNCSIG__
#else
#define SB_FUNC __func__
#endif

/**
 * Evaluate the condition and throw an @c EnforceException if it is false.
 */
#define enforce(CONDITION) enforce_impl((CONDITION), SB_STRINGIZE(CONDITION), SB_FUNC, __FILE__, __LINE__)

/**
 * Validate that the result of an SDL operation is 0 (OK). If not, throw an SdlException.
 */
void sdlok(int result);

/**
 * Validate that the object created by SDL (cast to bool) is true (not nullptr).
 * If not, throw an SdlException.
 */
void sdlok(void* pointer);

/**
 * Validate that the object created by SDL_image (cast to bool) is true (not nullptr).
 * If not, throw an SdlException.
 */
void imgok(void* pointer);

/**
 * Display the error to the user in an appropriate way.
 * If SDL is up and running, this means that the error will be rendered to the
 * screen canvas in its own loop, interrupting the normal application flow.
 * If SDL is not available, and in any case, write an error log entry.
 */
void show_error(const std::exception& exception) noexcept;

// ================================================
// Logging
// ================================================

/**
 * Interface for the underlying logging implementation.
 */
class LogImpl
{

public:

	virtual ~LogImpl() =default;
	virtual void write(const std::string& message) noexcept =0;

};

/**
 * Create a logging implementation that writes to the specified file.
 */
std::unique_ptr<LogImpl> create_file_log(const char* path);

/**
 * Singleton main logging class.
 * Formats messages on different log levels and hands them to a logging implementation
 * e.g. to be written to a file.
 */
class Log
{

public:

	Log(const Log& ) =delete;
	~Log();

	/**
	 * Initialize the logger with the given backend.
	 */
	static void init(std::unique_ptr<LogImpl> impl);

	/**
	 * Write an info-level log message.
	 * If the logger is not intialized, do nothing.
	 */
	static void info(const char *format, ...) noexcept;

	/**
	 * Write an error-level log message.
	 * If the logger is not intialized, do nothing.
	 */
	static void error(const char *format, ...) noexcept;

	/**
	 * Format the given message and write it using the implementation.
	 */
	void write(const char* level, const char *format, va_list vlist) const noexcept;

private:

	explicit Log(std::unique_ptr<LogImpl> impl);

	std::unique_ptr<LogImpl> m_impl;

	static std::unique_ptr<Log> m_instance;

};
