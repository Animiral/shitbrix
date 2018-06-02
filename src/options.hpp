#pragma once

#include <string>
#include <optional>

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
	 * Option: --run [client|server|with-server]
	 * Which application mode to launch.
	 * - "client" (default)
	 * - "server": run only the server
	 * - "with-server": run the server in a thread together with the client
	 */
	const char* run_mode() const noexcept { return m_run_mode; }

	/**
	 * Option: --player_number [NUMBER]
	 * Number of the player that is controlled by this client.
	 * The default absence of a value means that this client controls all players.
	 * The other players in the game have a 0-based ascending number.
	 * Local inputs are assigned to the controlled player.
	 */
	std::optional<int> player_number() const noexcept { return m_player_number; }

	/**
	 * Option: --replay [path-string]
	 * The path location of the replay file to be played back.
	 * If nullptr, we run the game interactively.
	 */
	const char* replay_path() const noexcept { return m_replay_path; }

	/**
	 * Option: --logfile [path-string]
	 * The path location of the output log file.
	 */
	const char* log_path() const noexcept { return m_log_path; }

	/**
	 * Option: --server-url [url-string]
	 * Which server to connect to.
	 */
	const char* server_url() const noexcept { return m_server_url; }

private:

	const char* m_run_mode;
	std::optional<int> m_player_number;
	const char* m_replay_path;
	const char* m_log_path;
	const char* m_server_url;

	const char* str_option(int argc, const char* argv[], const std::string& option) noexcept;
	std::optional<int> int_option(int argc, const char* argv[], const std::string& option);
	bool bool_option(int argc, const char* argv[], const std::string& option) noexcept;

};
