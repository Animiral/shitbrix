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
 * Return the corresponding @c LaunchMode for the string representation.
 * @throw ConfigException if the string is not recognized.
 */
LaunchMode parse_launch_mode(std::string value);

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
: launch_mode{LaunchMode::MENU},
  player_number{},
  joystick_number{},
  ai_player{},
  ai_level(1),
  rules{ 0 },
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
		const std::regex line_ex{R"(^\s*([\w\.]+)[\s=]+(.*)$)"};
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
				throwx<ConfigException>("Unrecognized argument: %s", argv[i]);
			}
		}
		else {
			throwx<ConfigException>("Missing parameter for %s", argv[i]);
		}
	}

	normalize();
}

void Configuration::parse(std::string key, std::string value)
{
	auto found = config_setter.find(key);

	if(config_setter.end() == found)
		throwx<ConfigException>("Unknown configuration key: %s", key.c_str());

	found->second(*this, value);
}

void Configuration::normalize()
{
	// Attempt to bring the configuration into a consistent state after loading it.
	if(ai_player.has_value() && (ai_player.value() < 0 || ai_player.value() > 1))
		ai_player.reset();

	if(ai_level < 0 || ai_level > 2)
		ai_level = 1;

	if(rules.cursor_delay < 0)
		rules.cursor_delay = 0;
}


void configure_context(const Configuration& configuration)
{
	the_context.configuration.reset(new Configuration(configuration));

	const bool is_server_only = LaunchMode::SERVER == the_context.configuration->launch_mode;
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

const char* launch_mode_string[] =
{ "menu", "local", "client", "server", "with-server"};

LaunchMode parse_launch_mode(std::string value)
{
	const auto mode_found = std::find(launch_mode_string, std::end(launch_mode_string), value);
	const size_t mode_index = std::distance(launch_mode_string, mode_found);

	if(std::size(launch_mode_string) <= mode_index)
		throwx<ConfigException>("Invalid launch mode: \"%s\"", value.c_str());

	return static_cast<LaunchMode>(mode_index);
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
	{"player_number",      [](Configuration& c, std::string value) { c.player_number   = to_opt_int(value); }},
	{"launch_mode",        [](Configuration& c, std::string value) { c.launch_mode     = parse_launch_mode(value); }},
	{"joystick_number",    [](Configuration& c, std::string value) { c.joystick_number = to_opt_int(value); }},
	{"ai_player",          [](Configuration& c, std::string value) { c.ai_player       = to_opt_int(value); }},
	{"ai_level",           [](Configuration& c, std::string value) { c.ai_level = std::stoi(value); }},
	{"rules.cursor_delay", [](Configuration& c, std::string value) { c.rules.cursor_delay = std::stoi(value); }},
	{"autorecord",         [](Configuration& c, std::string value) { c.autorecord      = "true" == value; }},
	{"replay_path",        [](Configuration& c, std::string value) { c.replay_path     = std::filesystem::path{value}; }},
	{"log_path",           [](Configuration& c, std::string value) { c.log_path        = std::filesystem::path{value}; }},
	{"server_url",         [](Configuration& c, std::string value) { c.server_url      = value; }},
	{"port",               [](Configuration& c, std::string value) { c.port            = std::stoi(value); }},
};

}
