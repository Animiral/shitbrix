#pragma once

#include <string>

/**
 * Parses command-line options.
 */
class Options
{

public:

	/**
	 * Parse command line strings into game options.
	 */
	Options(int argc, const char* argv[]);

	/**
	 * The path location of the replay file to be played back.
	 * If nullptr, we run the game interactively.
	 */
	const char* replay_path() const noexcept;

	/**
	 * The path location of the output log file.
	 */
	const char* log_path() const noexcept;

private:

	const char* m_replay_path;
	const char* m_log_path;

	const char* str_option(int argc, const char* argv[], const std::string& option) noexcept;
	bool bool_option(int argc, const char* argv[], const std::string& option) noexcept;

};
