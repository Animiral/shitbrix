#include "options.hpp"
#include <sstream>

Options::Options(int argc, const char* argv[])
: m_run_mode(str_option(argc, argv, "--run")),
  m_player_number(int_option(argc, argv, "--player_number")),
  m_replay_path(str_option(argc, argv, "--replay")),
  m_log_path(str_option(argc, argv, "--logfile")),
  m_server_url(str_option(argc, argv, "--server-url"))
{
	// set defaults for missing options
	if(!m_run_mode) m_run_mode = "client";
	if(!m_log_path) {
		if(0 == strcmp("server", m_run_mode)) m_log_path = "server-logfile.txt";
		else m_log_path = "logfile.txt";
	}
	if(!m_server_url) m_server_url = "localhost6";
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

std::optional<int> Options::int_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	const char** itr = std::find(argv, end, option);
	if (itr != end && ++itr != end)
	{
		return std::stoi(*itr);
	}
	return std::nullopt;
}

bool Options::bool_option(int argc, const char* argv[], const std::string& option) noexcept
{
	auto end = argv + argc;
	return std::find(argv, end, option) != end;
}
