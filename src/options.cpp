#include "options.hpp"
#include <sstream>

Options::Options(int argc, const char* argv[])
: m_replay_path(str_option(argc, argv, "--replay")),
  m_log_path(str_option(argc, argv, "--logfile")),
  m_server_url(str_option(argc, argv, "--server-url"))
{
	// set defaults for missing options
	if(!m_log_path) m_log_path = "logfile.txt";
	if(!m_server_url) m_server_url = "localhost6";
}

const char* Options::replay_path() const noexcept
{
	return m_replay_path;
}

const char* Options::log_path() const noexcept
{
	return m_log_path;
}

const char* Options::server_url() const noexcept
{
	return m_server_url;
}

// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
const char* Options::str_option(int argc, const char* argv[], const std::string& option) noexcept
{
	auto end = argv + argc;
	const char** itr = std::find(argv, end, option);
	if (itr != end && ++itr != end)
	{
		return *itr;
	}
	return nullptr;
}

bool Options::bool_option(int argc, const char* argv[], const std::string& option) noexcept
{
	auto end = argv + argc;
	return std::find(argv, end, option) != end;
}
