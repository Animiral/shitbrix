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
	 * Option: --run=[client|server|with-server]
	 * Which application mode to launch.
	 * - "client" (default)
	 * - "server": run only the server
	 * - "with-server": run the server in a thread together with the client
	 */
	const char* run_mode() const noexcept { return m_run_mode; }

	/**
	 * Option: --replay=[path-string]
	 * The path location of the replay file to be played back.
	 * If nullptr, we run the game interactively.
	 */
	const char* replay_path() const noexcept { return m_replay_path; }

	/**
	 * Option: --logfile=[path-string]
	 * The path location of the output log file.
	 */
	const char* log_path() const noexcept { return m_log_path; }

	/**
	 * Option: --server-url=[url-string]
	 * Which server to connect to.
	 */
	const char* server_url() const noexcept { return m_server_url; }

private:

	const char* m_run_mode;
	const char* m_replay_path;
	const char* m_log_path;
	const char* m_server_url;

	const char* str_option(int argc, const char* argv[], const std::string& option) noexcept;
	bool bool_option(int argc, const char* argv[], const std::string& option) noexcept;

};
