/**
 * Error handling and logging facilities.
 */
#pragma once

#include <exception>
#include <memory>
#include <string>
#include <optional>
#include <filesystem>

// NOTE: we use the standard assert macro for never-happens conditions.

/**
 * General exception for all types of errors that occur in the game.
 */
struct GameException : public std::exception
{
	/**
	 * Construct the @c GameException from a message and an optional root cause.
	 */
	explicit GameException(std::string what, std::unique_ptr<GameException> cause = {});
	GameException(const GameException& rhs);
	GameException(GameException&& rhs) = default;

	virtual std::unique_ptr<GameException> clone() const;
	virtual const char* class_name() const noexcept { return "GameException"; }
	virtual const char* what() const noexcept override { return m_what.c_str(); }

	const std::string m_what;
	std::unique_ptr<GameException> m_cause;
};

/**
 * Invalid syntax or values encountered while reading configuration.
 */
struct ConfigException : public GameException
{
	explicit ConfigException(std::string what = "") : GameException(std::move(what)) {}
	ConfigException(const ConfigException& rhs) = default;
	ConfigException(ConfigException&& rhs) = default;

	virtual const char* class_name() const noexcept override { return "ConfigException"; }
	virtual std::unique_ptr<GameException> clone() const override { return std::make_unique<ConfigException>(*this); }
};


/**
 * Invalid game states encountered while evaluating game logic.
 * This can point to invalid setup of pit contents.
 */
struct LogicException : public GameException
{
	explicit LogicException(std::string what = "") : GameException(std::move(what)) {}
	LogicException(const LogicException& rhs) = default;
	LogicException(LogicException&& rhs) = default;

	virtual const char* class_name() const noexcept override { return "LogicException"; }
	virtual std::unique_ptr<GameException> clone() const override { return std::make_unique<LogicException>(*this); }
};

/**
 * Problems in reading and parsing replays.
 */
struct ReplayException : public GameException
{
	explicit ReplayException(std::string what = "", std::unique_ptr<GameException> cause = {});
	ReplayException(const ReplayException& rhs) = default;
	ReplayException(ReplayException&& rhs) = default;

	virtual const char* class_name() const noexcept override { return "ReplayException"; }
	virtual std::unique_ptr<GameException> clone() const override { return std::make_unique<ReplayException>(*this); }
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
	SdlException(const SdlException& rhs) = default;
	SdlException(SdlException&& rhs) = default;

	virtual const char* class_name() const noexcept override { return "SdlException"; }
	virtual std::unique_ptr<GameException> clone() const override { return std::make_unique<SdlException>(*this); }
};

/**
 * Umbrella exception for error conditions that arise from use of the ENet library.
 * Their common feature is that they can not be handled.
 */
struct ENetException : public GameException
{
	/**
	 * Constructor with custom error message.
	 */
	explicit ENetException(const char* what) : GameException(what) {}
	ENetException(const ENetException& rhs) = default;
	ENetException(ENetException&& rhs) = default;

	virtual const char* class_name() const noexcept override { return "ENetException"; }
	virtual std::unique_ptr<GameException> clone() const override { return std::make_unique<ENetException>(*this); }
};

/**
 * Exception for violated input expectations and contracts.
 */
struct EnforceException : public GameException
{
	explicit EnforceException(const char* condition, const char* func, const char* file, int line);
	EnforceException(const EnforceException& rhs) = default;
	EnforceException(EnforceException&& rhs) = default;

	virtual const char* class_name() const noexcept override { return "EnforceException"; }
	virtual std::unique_ptr<GameException> clone() const override { return std::make_unique<EnforceException>(*this); }

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
#if defined(__GNUC__) || defined(__MINGW32__)
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
