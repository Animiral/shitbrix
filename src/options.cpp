#include "options.hpp"


#include <sstream>

Options::Options(int argc, const char* argv[])
: m_replay_file(str_option(argc, argv, "--replay"))
{
}

const char* Options::replay_file() const
{
	return m_replay_file;
}

// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
const char* Options::str_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	const char** itr = std::find(argv, end, option);
	if (itr != end && ++itr != end)
	{
		return *itr;
	}
	return nullptr;
}

bool Options::bool_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	return std::find(argv, end, option) != end;
}
