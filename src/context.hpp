/**
 * Containers for configurable dependencies.
 * In the future, the application can be made more configurable by abstracting
 * all the interfaces of the contained dependencies.
 */
#pragma once

#include <memory>

class Configuration;
class Sdl;
class Logger;
class Assets;
class Audio;

/**
 * Contains general-purpose objects that should be available everywhere.
 */
struct GlobalContext
{
	~GlobalContext();

	std::unique_ptr<Configuration> configuration; //!< application-wide configuration
	std::unique_ptr<Sdl> sdl; //!< SDL library interface
	std::unique_ptr<Logger> log; //!< logger
	std::unique_ptr<Assets> assets; //!< game asset loader
	std::unique_ptr<Audio> audio; //!< sound output interface
};

/**
 * The generally available context object.
 * All code except the main function may assume that all
 * contained interfaces point to implementations.
 */
extern GlobalContext the_context;
