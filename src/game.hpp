#pragma once

#include <memory>
#include <functional>
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
 * An abstract factory that can create dependencies for a @c Game tailored to
 * a specific scenario, like production or testing.
 *
 * The @c Game object keeps the factory until the start of the game, when the
 * created objects become necessary.
 */
class IGameFactory
{

public:

	explicit IGameFactory();
	virtual ~IGameFactory() = 0;

	/**
	 * Build all game objects based on the given meta information.
	 */
	virtual void create(GameMeta meta) = 0;

	// Getters for the created objects.
	// The ownership transfers to the caller.
	std::unique_ptr<GameState> state();
	std::unique_ptr<Journal> journal();
	std::unique_ptr<BlockDirector> director();
	std::unique_ptr<evt::GameEventHub> hub();
	std::unique_ptr<IArbiter> arbiter();

protected:

	/**
	 * Common implementation for creating everything but the arbiter.
	 */
	void base_create(GameMeta meta);

	// storage for the created objects - all concrete factories will need this.
	std::unique_ptr<GameState> m_state;
	std::unique_ptr<Journal> m_journal;
	std::unique_ptr<BlockDirector> m_director;
	std::unique_ptr<evt::GameEventHub> m_hub;
	std::unique_ptr<IArbiter> m_arbiter;

};

/**
 * The concrete factory which creates dependencies for @c LocalGame.
 */
class LocalGameFactory : public IGameFactory
{

public:

	virtual void create(GameMeta meta) override;

};

/**
 * The concrete factory which creates dependencies for @c ClientGame.
 */
class ClientGameFactory : public IGameFactory
{

public:

	virtual void create(GameMeta meta) override;

};

/**
 * The concrete factory which creates dependencies for @c ServerGame.
 */
class ServerGameFactory : public IGameFactory
{

public:

	explicit ServerGameFactory(ServerProtocol& protocol);

	virtual void create(GameMeta meta) override;

private:

	ServerProtocol* m_protocol;

};

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

	explicit IGame(std::unique_ptr<IGameFactory> game_factory) noexcept;
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
	 * If the @c replay flag is true, the game starts in replay mode. This means
	 * that the input set from the replay is complete and no extra live inputs
	 * should be generated based on random decisions.
	 *
	 * Invalidate all references to the @c GameState, @c Journal and @c GameEventHub.
	 * The game is then no longer in progress and possibly not ready.
	 *
	 * After this reset, the game may not immediately be reset.
	 * In network mode, we can merely request that the server should reset.
	 *
	 * TODO: In network mode, this should only work from a privileged client.
	 */
	virtual void game_reset(int players, bool replay) = 0;

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
	 * Callback type for changes in the game state machine.
	 */
	using Handler = std::function<void()>;

	/**
	 * Set the callback handler to be called just before the game resets.
	 */
	void before_reset(Handler handler);

	/**
	 * Set the callback handler to be called just after the game starts.
	 */
	void after_start(Handler handler);

	/**
	 * Based on all available information: inputs gathered, game journal and
	 * game rules, calculate the game state to the given @c target_time.
	 *
	 * @throw EnforceException if the game is not in progress.
	 */
	void synchronurse(long target_time);

	/**
	 * Read the replay from the given replay file.
	 *
	 * The game resets and then starts using the meta-information from the replay.
	 * The game state is then the initial state and the input history is available
	 * in the journal.
	 *
	 * Clients can navigate to any point in the replay using the @c synchronurse function.
	 */
	void load_replay(std::filesystem::path path);

protected:

	Switches m_switches; //!< extra control information values
	Handler m_reset_handler; //!< callable to notify on game reset
	Handler m_start_handler; //!< callable to notify on game reset

	/**
	 * Create the objects that every @c Game implementation needs at game start.
	 */
	void base_start();

	/**
	 * Destroy/reset the game objects when they are no longer needed.
	 */
	void base_reset();

	/**
	 * This method is called by @c synchronurse if new inputs lead to a rollback
	 * of the game state to an earlier point.
	 *
	 * @param target_time the target time passed to @c synchronurse
	 * @param checkpoint_time the time of the checkpoint from which we start
	 */
	virtual void before_rollback(long target_time, long checkpoint_time) {}

	std::optional<GameMeta> m_meta; //!< game meta-info, available when ready or ingame
	std::unique_ptr<IGameFactory> m_game_factory; //!< creates dependencies in @c base_start
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

	explicit LocalGame(std::unique_ptr<IGameFactory> game_factory);
	virtual ~LocalGame() noexcept;

	// IGame member functions - local-specific implementation
	virtual void game_start() override;
	virtual void game_input(Input input) override;
	virtual void game_reset(int players, bool replay) override;
	virtual void set_speed(int speed) override;
	virtual void poll() override;

protected:

	virtual void before_rollback(long target_time, long checkpoint_time) override;

private:

	std::unique_ptr<IArbiter> m_arbiter; //!< centralized decision component, non-null ingame

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
	explicit ClientGame(std::unique_ptr<IGameFactory> game_factory, std::unique_ptr<ClientProtocol> protocol) noexcept;

	// IGame member functions - client-specific implementation
	virtual void game_start() override;
	virtual void game_input(Input input) override;
	virtual void game_reset(int players, bool replay) override;
	virtual void set_speed(int speed) override;
	virtual void poll() override;

private:

	std::unique_ptr<ClientProtocol> m_protocol; //!< communicator object

	// IServerMessages member functions - handlers for incoming messages
	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void retract(long cutoff_time) override;
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
	explicit ServerGame(std::unique_ptr<IGameFactory> game_factory, std::unique_ptr<ServerProtocol> protocol) noexcept;
	~ServerGame() noexcept;

	// IGame member functions - server-specific implementation
	virtual void game_start() override;
	virtual void game_input(Input input) override;
	virtual void game_reset(int players, bool replay) override;
	virtual void set_speed(int speed) override;
	virtual void poll() override;

protected:

	virtual void before_rollback(long target_time, long checkpoint_time) override;

private:

	std::unique_ptr<IArbiter> m_arbiter;  //!< centralized decision component, non-null ingame
	std::unique_ptr<ServerProtocol> m_protocol; //!< communicator object

	// IClientMessages member functions - handlers for incoming messages
	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;

};
