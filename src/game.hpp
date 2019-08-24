#pragma once

#include <memory>
#include "globals.hpp"

class BlockDirector;
class IArbiter;
class GameState;
class Journal;

namespace evt
{

class GameEventHub;

}


/*
 * The rules contain all the implementation objects
 * for advancing a game state for all players.
 */
struct Rules
{
	explicit Rules(std::unique_ptr<IArbiter> arbiter = {});
	explicit Rules(Rules&& ) noexcept;
	Rules& operator=(Rules&& ) noexcept;
	~Rules() noexcept;

	std::unique_ptr<BlockDirector> block_director; //!< game rules implementation
	std::unique_ptr<evt::GameEventHub> event_hub; //!< subscription service for game events
	std::unique_ptr<IArbiter> arbiter; //!< centralized decision component
};

/**
 * Contains the objects which make up the internal game representation and
 * behavior while remaining agnostic towards the mode of the game
 * (client/server, live/replay, display etc.)
 */
struct GameData
{
	explicit GameData(std::unique_ptr<GameState> state, std::unique_ptr<Journal> journal,
	                  std::unique_ptr<IArbiter> arbiter = {});
	GameData(const GameData& rhs) = delete;
	GameData(GameData&& rhs) = default;
	GameData& operator=(GameData& ) = delete;
	GameData& operator=(GameData&& rhs) = default;
	~GameData() noexcept;

	Dials dials; //!< Extra-journal control settings for the current game session
	std::unique_ptr<GameState> state; //!< Active and always current game state container
	std::unique_ptr<Journal> journal; //!< Game events and checkpoints record
	Rules rules; //!< Game state manipulation routines
};
