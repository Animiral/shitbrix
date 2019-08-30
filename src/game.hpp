#pragma once

#include <memory>
#include "globals.hpp"
#include "input.hpp"
#include "network.hpp"

// forward declarations
class BlockDirector;
class GameState;
class Journal;
class IArbiter;

namespace evt
{

class GameEventHub;

}

/**
 * These switches contain general information about the state of the current
 * game session outside the journal record of the game. They do not directly
 * affect gameplay.
 * In a networked game, these switches are coordinated between the server and
 * clients.
 */
struct Switches
{
	int speed = 1; //!< display speed of the game (currently just 0 for pause and 1 normally)
	bool ready = false; //!< true if the game is ready to start
	bool ingame = false; //!< true if game in progress - objects like state exist
	int winner = NOONE; //!< if the game is over, contains the index of the winner
};

/**
 * Interface for classes that implement a game session.
 *
 * Includes all game state, logic and communication facilities.
 * The implementation may coordinate over the network with a server.
 *
 * Does not handle presentation, input devices, screen transitions etc.
 */
class IGame
{

public:

	virtual ~IGame() = 0;

	/**
	 * Return the current switch values.
	 * These contain extra control information outside gameplay.
	 */
	Switches switches();

	/**
	 * Return the game state object.
	 * This reference is valid as long as the current game is ongoing or over.
	 *
	 * @throw EnforceException if the game is not currently in progress.
	 */
	const GameState& state() const;

	/**
	 * Return the record of game events and checkpoints.
	 * This reference is valid as long as the current game is ongoing or over.
	 *
	 * @throw EnforceException if the game is not currently in progress.
	 */
	const Journal& journal() const;

	/**
	 * Return the subscription service for game events.
	 * This reference is valid as long as the current game is ongoing or over.
	 *
	 * @throw EnforceException if the game is not currently in progress.
	 */
	evt::GameEventHub& hub();

	/**
	 * Return the high-level game logic implementation.
	 * This is only exposed to enable debug functions.
	 * TODO: remove this and instead directly disable the gameover check here.
	 *
	 * @throw EnforceException if the game is not currently in progress.
	 */
	BlockDirector& director();

	/**
	 * Start the game based on the internal meta information.
	 * The internal information is available iff the ready switch is true.
	 *
	 * After this start, the game may not immediately be in progress.
	 * In network mode, we can merely request that the server should start.
	 *
	 * TODO: In network mode, this should only work from a privileged client.
	 *
	 * @throw EnforceException if the game is not ready to start.
	 */
	virtual void game_start() = 0;

	/**
	 * Apply the given input to the game.
	 */
	virtual void game_input(Input input) = 0;

	/**
	 * Start a fresh game with the specified number of players.
	 *
	 * Invalidate all references to the @c GameState, @c Journal and @c GameEventHub.
	 * The game is then no longer in progress and possibly not ready.
	 *
	 * After this reset, the game may not immediately be reset.
	 * In network mode, we can merely request that the server should reset.
	 *
	 * TODO: In network mode, this should only work from a privileged client.
	 */
	virtual void game_reset(int players) = 0;

	/**
	 * Change the speed of the game.
	 *
	 * To pause the game, set the speed to 0.
	 * To run at regular speed, set the speed to 1.
	 *
	 * TODO: In network mode, this should only work from a privileged client.
	 */
	virtual void set_speed(int speed) = 0;

	/**
	 * Look for external messages and handle them.
	 *
	 * In the network implementation, such messages include control messages
	 * and additional inputs from the server.
	 */
	virtual void poll() = 0;

	/**
	 * Based on all available information: inputs gathered, game journal and
	 * game rules, calculate the game state to the given @c target_time.
	 *
	 * @throw EnforceException if the game is not in progress.
	 */
	void synchronurse(long target_time);

protected:

	Switches m_switches; //!< extra control information values

	std::optional<GameMeta> m_meta; //!< game meta-info, available when ready or ingame
	std::unique_ptr<GameState> m_state; //!< game state object, non-null ingame
	std::unique_ptr<Journal> m_journal; //!< game record, non-null ingame
	std::unique_ptr<BlockDirector> m_director; //!< game rules implementation
	std::unique_ptr<evt::GameEventHub> m_hub; //!< game events subscriptions, non-null ingame

};

/**
 * Local-only game implementation.
 *
 * This implementation offers an interface as if the
 * server was always immediately responsive.
 */
class LocalGame : public IGame
{

public:

	explicit LocalGame() noexcept;
	virtual ~LocalGame() noexcept;

	// IGame member functions - local-specific implementation
	virtual void game_start() override;
	virtual void game_input(Input input) override;
	virtual void game_reset(int players) override;
	virtual void set_speed(int speed) override;
	virtual void poll() override;

private:

	std::unique_ptr<IArbiter> m_arbiter;  //!< centralized decision component, non-null ingame

};

/**
 * Client game implementation.
 *
 * This implementation coordinates with a server over a protocol.
 */
class ClientGame : public IGame, private IServerMessages
{

public:

	/**
	 * Construct the game to communicate via the given protocol.
	 */
	explicit ClientGame(ClientProtocol protocol) noexcept;

	// IGame member functions - client-specific implementation
	virtual void game_start() override;
	virtual void game_input(Input input) override;
	virtual void game_reset(int players) override;
	virtual void set_speed(int speed) override;
	virtual void poll() override;

private:

	ClientProtocol m_protocol; //!< communicator object

	// IServerMessages member functions - handlers for incoming messages
	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;
	virtual void gameend(int winner) override;

};

/**
 * Server game implementation.
 *
 * Provides coordination and game decisions for connected clients.
 */
class ServerGame : public IGame, private IClientMessages
{

public:

	/**
	 * Construct the game to communicate via the given protocol.
	 */
	explicit ServerGame(ServerProtocol protocol) noexcept;

	// IGame member functions - server-specific implementation
	virtual void game_start() override;
	virtual void game_input(Input input) override;
	virtual void game_reset(int players) override;
	virtual void set_speed(int speed) override;
	virtual void poll() override;

private:

	std::unique_ptr<IArbiter> m_arbiter;  //!< centralized decision component, non-null ingame
	ServerProtocol m_protocol; //!< communicator object

	// IClientMessages member functions - handlers for incoming messages
	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;

};
