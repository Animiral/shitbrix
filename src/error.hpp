/**
 * Error handling and logging facilities.
 *
 * Our error-handling strategy consists of three approaches.
 *
 * 1. We use the standard assert macro for never-happens conditions.
 *    In cases such as the default branch of an enum switch-statement or 
 *    for safety checks within a module, we use this shortest standard check.
 *
 * 2. We use our own enforce macro for validating values that ostensibly
 *    originate within the program, such as input parameters from other
 *    modules or even network messages. It throws a generic EnforceException.
 *
 * 3. We use our custom exception hierarchy for errors that might conceivably
 *    reach the user. This includes bad inputs, bad files, failed network
 *    connections and communication between incompatible versions of the
 *    program.
 *
 * The custom exceptions come with a short error message that
 * describes the problem.
 */
#pragma once

#include <exception>
#include <memory>
#include <string>
#include <optional>
#include <filesystem>
#include "globals.hpp"

/**
 * Evaluate the condition and throw an @c EnforceException if it is false.
 * Intended for use through the @c enforce macro.
 */
void enforce_impl(bool condition, const char* condition_str, const char* func, const char* file, int line);

// expression-to-string helper
#define STR1(x) #x
#define SB_STRINGIZE(x) STR1(x)

// align compilers
#if defined(__GNUC__) || defined(__MINGW32__)
#define SB_FUNC __PRETTY_FUNCTION__
#elif _MSC_VER
#define SB_FUNC __FUNCSIG__
#else
#define SB_FUNC __func__
#endif

/**
 * In our debug-build implementation, if a safety check fails (input contracts,
 * library return values etc), we stop the application so that the debugger may
 * inspect the state before the call stack unwinds.
 * This can interfere with unit tests. Before executing unit tests, this flag
 * must therefore be set to false.
 */
extern bool on_failure_break_into_debugger;

// prepare forward declarations for throwx, but avoid pulling in the huge windows.h
#if defined(_WIN32)
extern "C" __declspec(dllimport) void __stdcall DebugBreak();
#else
#include <csignal>
#endif

/**
 * Construct the specified exception with the given parameters and throw it.
 *
 * Before the exception is thrown, this function attempts to break into the
 * debugger, if available.
 */
template<class Except, class... Args>
[[noreturn]]
void throwx(Args&& ... params)
{
	if(on_failure_break_into_debugger) {
		// Stop and activate the debugger.
		// https://stackoverflow.com/questions/4326414/set-breakpoint-in-c-or-c-code-programmatically-for-gdb-on-linux
		// https://docs.microsoft.com/en-us/visualstudio/debugger/debugbreak-and-debugbreak
		// https://hg.mozilla.org/mozilla-central/file/98fa9c0cff7a/js/src/jsutil.cpp#l66
#if defined(_WIN32)
		//* ((int*)NULL) = 0;
		DebugBreak();
#else
		std::raise(SIGABRT);  /* To continue from here in GDB: "signal 0". */
#endif
	}

	throw Except(std::forward<Args>(params)...);
}

/**
 * Evaluate the condition and throw an @c EnforceException if it is false.
 */
#define enforce(CONDITION) enforce_impl(bool(CONDITION), SB_STRINGIZE(CONDITION), SB_FUNC, __FILE__, __LINE__)

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
 * Validate that the object created by SDL_ttf (cast to bool) is true (not nullptr).
 * If not, throw an SdlException.
 */
void ttfok(void* pointer);

#define enetok(VALUE) enetok_impl((VALUE), "Bad result: " SB_STRINGIZE(VALUE))

/**
 * Validate that the result of an ENet operation is 0 (OK). If not, throw an ENetException.
 */
void enetok_impl(int result, const char* what);

/**
 * Validate that the object created by ENet (cast to bool) is true (not nullptr).
 * If not, throw an ENetException.
 */
void enetok_impl(void* pointer, const char* what);

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
 * All implementations should be thread-safe.
 */
class Logger
{

public:

	virtual ~Logger() =default;
	virtual void write(const std::string& message) noexcept =0;

};

/**
 * Create a logging implementation that swallows all messages.
 */
std::unique_ptr<Logger> create_no_log();

/**
 * Create a logging implementation that writes to the specified file.
 */
std::unique_ptr<Logger> create_file_log(std::filesystem::path path);

/**
 * Logging convenience functions.
 * Format messages on different log levels and hands them to a the logging
 * implementation defined in the global context, e.g. to be written to a file.
 */
namespace Log
{

/**
 * Write a trace-level log message.
 * If the logger is not intialized, do nothing.
 */
void trace(const char *format, ...) noexcept;

/**
 * Write an info-level log message.
 * If the logger is not intialized, do nothing.
 */
void info(const char *format, ...) noexcept;

/**
 * Write an error-level log message.
 * If the logger is not intialized, do nothing.
 */
void error(const char *format, ...) noexcept;

/**
 * Format the given message and write it using the implementation.
 */
void write(const char* level, const char *format, va_list vlist) noexcept;

}

// ================================================
// Exception class hierarchy
// ================================================

/**
 * General exception for all types of errors that occur in the game.
 */
class GameException : public std::exception
{

public:

	/**
	 * Constructor from a root cause exception and a printf-style formatted message.
	 */
	template<typename... Args>
	explicit GameException(GameException cause, const std::string& format, Args&& ... args)
		: m_what(string_format(format, std::forward<Args>(args)...)),
		m_cause(std::make_unique<GameException>(std::move(cause)))
	{
	}

	/**
	 * Constructor from a printf-style formatted message.
	 */
	template<typename... Args>
	explicit GameException(const std::string& format, Args&& ... args)
		: m_what(string_format(format, std::forward<Args>(args)...))
	{
	}

	GameException(const GameException& rhs);
	GameException(GameException&& rhs) noexcept = default;
	GameException& operator=(const GameException& rhs);
	GameException& operator=(GameException&& rhs) noexcept = default;
	virtual ~GameException() noexcept = default;

	virtual std::unique_ptr<GameException> clone() const;
	virtual const char* class_name() const noexcept { return "GameException"; }
	virtual const char* what() const noexcept final override { return m_what.c_str(); }
	const GameException& cause() const noexcept { return *m_cause; }

private:

	std::string m_what;
	std::unique_ptr<GameException> m_cause;

};

// shortcut helpers for declaring derived exception classes
#define EXCEPTION_CONSTRUCT(Class, Base) \
	template<typename... Args> \
	explicit Class(GameException cause, const std::string& format, Args&& ... args) \
		: Base(std::move(cause), format, std::forward<Args>(args)...) \
	{} \
	template<typename... Args> \
	explicit Class(const std::string& format, Args&& ... args) \
		: Base(format, std::forward<Args>(args)...) \
	{}

#define EXCEPTION_DEFAULT(Class) \
	Class(const Class& rhs) = default; \
	Class(Class&& rhs) noexcept = default; \
	Class& operator=(const Class& rhs) = default; \
	Class& operator=(Class&& rhs) noexcept = default; \
	virtual ~Class() noexcept = default; \
	virtual const char* class_name() const noexcept override { return #Class; }

/**
 * Base exception class that implements cloning through the CRTP.
 *
 * This class cannot be instantiated on its own. A derived implementation is required.
 */
template<class ExceptionImpl>
class GameExceptionCloning : public GameException
{

public:

	virtual std::unique_ptr<GameException> clone() const override
	{
		return std::make_unique<ExceptionImpl>(static_cast<const ExceptionImpl&>(*this));
	}

protected:

	EXCEPTION_CONSTRUCT(GameExceptionCloning, GameException)
	EXCEPTION_DEFAULT(GameExceptionCloning)

};

/**
 * Invalid syntax or values encountered while reading configuration.
 */
class ConfigException : public GameExceptionCloning<ConfigException>
{

public:

	EXCEPTION_CONSTRUCT(ConfigException, GameExceptionCloning)
	EXCEPTION_DEFAULT(ConfigException)

};


/**
 * Invalid game states encountered while evaluating game logic.
 * This can point to invalid setup of pit contents.
 */
class LogicException : public GameExceptionCloning<LogicException>
{

public:

	EXCEPTION_CONSTRUCT(LogicException, GameExceptionCloning)
	EXCEPTION_DEFAULT(LogicException)

};

/**
 * Problems in reading and parsing replays.
 */
class ReplayException : public GameExceptionCloning<ReplayException>
{

public:

	EXCEPTION_CONSTRUCT(ReplayException, GameExceptionCloning)
	EXCEPTION_DEFAULT(ReplayException)

};

/**
 * Umbrella exception for error conditions that arise from use of the SDL library.
 * Their common feature is that they can not be handled and we get the error
 * message from the library.
 */
class SdlException : public GameExceptionCloning<SdlException>
{

public:

	/**
	 * Constructor from the given error message, or from @c SDL_GetError by default.
	 */
	explicit SdlException(const char* what = nullptr);

	EXCEPTION_DEFAULT(SdlException)

};

/**
 * Umbrella exception for error conditions that arise from use of the ENet library.
 * Their common feature is that they can not be handled.
 */
class ENetException : public GameExceptionCloning<ENetException>
{

public:

	/**
	 * Constructor with custom error message.
	 */
	explicit ENetException(const char* what) : GameExceptionCloning("%s", what) {}

	EXCEPTION_DEFAULT(ENetException)

};

/**
 * Exception for violated input expectations and contracts.
 */
class EnforceException : public GameExceptionCloning<EnforceException>
{

public:

	explicit EnforceException(const char* condition, const char* func, const char* file, int line);

	EXCEPTION_DEFAULT(EnforceException)

};
