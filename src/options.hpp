#pragma once

#include <string>

/**
 * Parses command-line options.
 */
class Options
{

public:

	Options(int argc, const char* argv[]);
	const char* replay_file() const;

private:

	const char* m_replay_file;

	const char* str_option(int argc, const char* argv[], const std::string& option);
	bool bool_option(int argc, const char* argv[], const std::string& option);

};
