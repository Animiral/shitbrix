#include "configuration.hpp"
#include "context.hpp"
#include "globals.hpp"
#include "sdl_helper.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "error.hpp"
#include <fstream>
#include <regex>
#include <functional>
#include <map>
#include <SDL.h>

namespace
{

/**
 * Return the corresponding @c NetworkMode for the string representation.
 * @throw ConfigException if the string is not recognized.
 */
NetworkMode parse_network_mode(std::string value);

/**
 * If the string value contains data, convert it to an integer and return it.
 * If the string value is empty, return an empty optional.
 */
std::optional<int> to_opt_int(const std::string& value);

/**
 * Type of a function that sets one configuration variable to the given value.
 */
using ConfigSetter = std::function<void(Configuration&, std::string)>;

/**
 * Lookup table of setter functions by configuration key name.
 */
extern const std::map<std::string, ConfigSetter> config_setter;

}


Configuration::Configuration()
: network_mode{NetworkMode::LOCAL},
  player_number{},
  joystick_number{},
  autorecord{false},
  replay_path{},
  log_path{"logfile.txt"},
  server_url{},
  port{DEFAULT_PORT}
{
}

void Configuration::read_from_file(std::filesystem::path path)
{
	std::ifstream stream{path};
	std::string line;
	
	while(std::getline(stream, line)) {
		const std::regex line_ex{R"(^\s*(\w+)[\s=]+(.*)$)"};
		std::cmatch result;

		if(std::regex_match(line.c_str(), result, line_ex)) {
			const std::string key{result[1]};
			const std::string value{result[2]};
			parse(key, value);
		}
	}

	normalize();
}

void Configuration::read_from_args(int argc, const char* argv[])
{
	for(int i = 1; i < argc; i++) {
		const std::regex assign_ex{R"(^--\s*(\w+)[\s=]+(.*)$)"};
		std::cmatch result;

		if(std::regex_match(argv[i], result, assign_ex)) {
			const std::string key{result[1]};
			const std::string value{result[2]};
			parse(key, value);
		}
		else if(i+1 < argc) {
			if("--" == std::string(argv[i]).substr(0, 2)) {
				parse(argv[i] + 2, argv[i + 1]);
				i++;
			}
			else {
				throw ConfigException(std::string("Unrecognized argument: ") + argv[i]);
			}
		}
		else {
			throw ConfigException(std::string("Missing parameter for ") + argv[i]);
		}
	}

	normalize();
}

void Configuration::parse(std::string key, std::string value)
{
	auto found = config_setter.find(key);

	if(config_setter.end() == found)
		throw ConfigException("Unknown configuration key: " + key);

	found->second(*this, value);
}

void Configuration::normalize()
{
	if(NetworkMode::WITH_SERVER == network_mode) {
		server_url = "localhost";
	}
}


void configure_context(const Configuration& configuration)
{
	the_context.configuration.reset(new Configuration(std::move(configuration)));

	const bool is_server_only = NetworkMode::SERVER == the_context.configuration->network_mode;
	Uint32 sdl_flags = is_server_only ? SDL_INIT_TIMER | SDL_INIT_EVENTS
	                                  : SDL_INIT_EVERYTHING;

	the_context.sdl.reset(new Sdl(sdl_flags));
	the_context.log = create_file_log(the_context.configuration->log_path);

	if(is_server_only) {
		the_context.assets.reset(new NoAssets);
		the_context.audio.reset(new NoAudio);
	}
	else {
		the_context.assets.reset(new FileAssets(*the_context.sdl));
		the_context.audio.reset(new SdlAudio(the_context.sdl->audio()));
	}
}


namespace
{

const char* network_mode_string[] =
{ "local", "client", "server", "with-server"};

NetworkMode parse_network_mode(std::string value)
{
	const auto mode_found = std::find(network_mode_string, std::end(network_mode_string), value);
	const size_t mode_index = std::distance(network_mode_string, mode_found);

	if(std::size(network_mode_string) <= mode_index)
		throw ConfigException("Invalid network mode: \"" + value + "\"");

	return static_cast<NetworkMode>(mode_index);
}

std::optional<int> to_opt_int(const std::string& value)
{
	if(value.empty())
		return {};
	else
		return std::stoi(value);
}

const std::map<std::string, ConfigSetter> config_setter
{
	{"network_mode",    [](Configuration& c, std::string value) { c.network_mode    = parse_network_mode(value); }},
	{"player_number",   [](Configuration& c, std::string value) { c.player_number   = to_opt_int(value); }},
	{"joystick_number", [](Configuration& c, std::string value) { c.joystick_number = to_opt_int(value); }},
	{"autorecord",      [](Configuration& c, std::string value) { c.autorecord      = "true" == value; }},
	{"replay_path",     [](Configuration& c, std::string value) { c.replay_path     = std::filesystem::path{value}; }},
	{"log_path",        [](Configuration& c, std::string value) { c.log_path        = std::filesystem::path{value}; }},
	{"server_url",      [](Configuration& c, std::string value) { c.server_url      = value; }},
	{"port",            [](Configuration& c, std::string value) { c.port            = std::stoi(value);; }},
};

}
