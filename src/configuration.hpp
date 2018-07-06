#pragma once

#include <string>
#include <optional>
#include <filesystem>

/**
 * Network operation mode of the application.
 */
enum class NetworkMode
{
	/* LOCAL, */ //!< Run only on this machine
	CLIENT,      //!< Connect as a client
	SERVER,      //!< Host the game as a server
	WITH_SERVER  //!< Host the game locally and also act as a client
};

/**
 * A collection of values that govern application behavior.
 * Configuration values can be read from a configuration
 * file or from command-line options.
 * Configuration values come in an ordered hierarchy, where values
 * from higher configuration sources override lower ones.
 * From low to high, the sources are in this order:
 * 1. hard-coded default values
 * 2. machine configuration file
 * 3. user's configuration file
 * 4. command-line options
 * 5. run-time settings
 */
class Configuration
{

public:

	/**
	 * Initialize with default values.
	 */
	Configuration();

	Configuration(const Configuration& ) = default;
	Configuration(Configuration&& ) = default;
	Configuration& operator=(const Configuration& ) = default;
	Configuration& operator=(Configuration&& ) = default;

	/**
	 * Which application mode to launch.
	 */
	NetworkMode network_mode;

	/**
	 * Number of the player that is controlled by this client.
	 * The default absence of a value means that this client controls all players.
	 * The other players in the game have a 0-based ascending number.
	 * Local inputs are assigned to the controlled player.
	 */
	std::optional<int> player_number;

	/**
	 * Number of the joystick that we use for input.
	 * By default, we do not accept joystick input.
	 * We currently only support using one joystick at a time.
	 * There is currently no way to identify it by name.
	 */
	std::optional<int> joystick_number;

	/**
	 * Automatically write a replay file after every game.
	 * Even if the option is set to true, the replay/ directory must exist.
	 * By default, autorecording is disabled.
	 */
	bool autorecord;

	/**
	 * The path location of the replay file to be played back.
	 * By default, if unspecified, we run the game interactively.
	 */
	std::optional<std::filesystem::path> replay_path;

	/**
	 * The path location of the output log file.
	 * If unspecified, the log will be appended to a default file.
	 */
	std::filesystem::path log_path;

	/**
	 * The locator of the server to connect to in client mode.
	 * This option is required if running as a client, and ignored if running locally.
	 */
	std::optional<std::string> server_url;

	/**
	 * Which port to use for network connections.
	 * This option applies to both server and client mode.
	 */
	int port;

	/**
	 * Read configuration values from the specified file.
	 * The syntax is "key = value" on every line, where key is the name of one of
	 * the member variables in this @c Configuration.
	 * Ignore lines that start with non-word characters.
	 */
	void read_from_file(std::filesystem::path path);

	/**
	 * Read configuration values from command-line arguments.
	 * The syntax is "--key=value" or "--key value" for every argument, where key
	 * is the name of one of the member variables in this @c Configuration.
	 */
	void read_from_args(int argc, const char* argv[]);

private:

	/**
	 * Set the configuration value with the given key name to the given value.
	 * Convert the string representation of the value to the correct type.
	 */
	void parse(std::string key, std::string value);

	/**
	 * Attempt to bring the configuration into a consistent state after loading it.
	 */
	void normalize();

};


/**
 * Instantiate the members of the global context based on the configuration.
 */
void configure_context(const Configuration& configuration);
