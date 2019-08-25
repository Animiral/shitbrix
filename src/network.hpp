/**
 * Interfaces for remote communication between different game instances.
 *
 * As a foundation, @c Channels pass simple @c Messages to remote points.
 *
 * On top of that, @c ClientProtocol and @c ServerProtocol provide a C++ class
 * interface for communication.
 *
 * The protocols are used by the game integration classes, which send and react
 * to network messages using their knowledge of the game state.
 *
 * For the future, more components may follow:
 * 1. The ListServer opens and starts listening for clients.
 * 2. A Host checks in at the Reception and receives the Server proxy object.
 * 3. The Host registers a game offer on the list server.
 * 4. One or more Clients check in at the Reception and receive the Server proxy object.
 * 5. The Client(s) join the offered game and receive the Host proxy object.
 * 6. The Host unlists the offer and starts the game.
 * 7. Afterwards, the Client(s) re-query the Server for the lobby status.
 */
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <future>
#include <atomic>
#include "globals.hpp"
#include "input.hpp"
#include "game.hpp"
#include "error.hpp"

// ==================== low-level communication ====================

/**
 * Enumeration of all messages that are used in this network implementation.
 */
enum class MsgType
{
	META,    //!< game meta-information
	PLAYER,  //!< set player number of client
	INPUT,   //!< player input in the game
	RETRACT, //!< go back on server-induced block/garbage spawns
	SPEED,   //!< game playback speed
	SYNC,    //!< whole game state
	CLIENTS, //!< request for or sync info about connected clients
	START,   //!< start game
	GAMEEND, //!< end game
	BYE,     //!< withdraw from the specified room
	OFFER,   //!< place a game offer
	REMOVE,  //!< retract a game offer
	JOIN,    //!< join a game offered
	LIST,    //!< transmit list of game offers
	CHECKIN, //!< initialize communication with Reception
};

/**
 * Network message representation.
 * All messages sent in ENet packets are represented in this low-level structure.
 * The message payload is still encoded as a free-form string and must be
 * parsed in the proper context.
 */
struct Message
{
	int sender;       //!< sender queue number
	int recipient;    //!< recipient queue number
	MsgType type;     //!< message category
	std::string data; //!< encoded message arguments / payload

	std::string to_string() const;
	static Message from_string(std::string message_string);
};

/**
 * This is an interface for sending and receiving messages.
 * The connected end points and means of transfer are implementation-defined.
 * Use the @c make_server_channel and @c make_client_channel functios to
 * obtain implementations that operate on the network via the enet library.
 */
class IChannel
{

public:

	virtual ~IChannel() = default;

	/**
	 * Send the message to remote peers.
	 */
	virtual void send(Message message) = 0;

	/**
	 * Check for unhandled messages and return them.
	 * Once a message has been polled, it is cleared from the Channel's memory.
	 */
	virtual std::vector<Message> poll() = 0;

};

/**
 * Return a Channel for the server side to communicate with clients.
 * It awaits and accepts clients' connections.
 */
std::unique_ptr<IChannel> make_server_channel(uint16_t port);

/**
 * Return a Channel for the client side to communicate with the server.
 * Connection errors lead to an exception instead of the creation of the channel.
 */
std::unique_ptr<IChannel> make_client_channel(const char* server_name, uint16_t port);

// ==================== communication protocols ====================

/**
 * Interface for messages from the server to the client(s).
 */
class IServerMessages
{

public:

	virtual ~IServerMessages() = default;

	virtual void meta(GameMeta meta) = 0;
	virtual void input(Input input) = 0;
	virtual void speed(int speed) = 0;
	virtual void start() = 0;
	virtual void gameend(int winner) = 0;

};

/**
 * Interface for messages from the client to the server.
 */
class IClientMessages
{

public:

	virtual ~IClientMessages() = default;

	virtual void meta(GameMeta meta) = 0;
	virtual void input(Input input) = 0;
	virtual void speed(int speed) = 0;
	virtual void start() = 0;

};

/**
 * The ServerProtocol implements sending and receiving messages on the server
 * side over a network channel as defined above.
 */
class ServerProtocol : public IServerMessages
{

public:

	/**
	 * Construct the protocol to use the given underlying channel.
	 * The protocol assumes ownership of the channel.
	 */
	explicit ServerProtocol(std::unique_ptr<IChannel> channel);

	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;
	virtual void gameend(int winner) override;

	/**
	 * For every unhandled message in the underlying channel's memory, invoke
	 * the appropriate member function of the given @c IClientMessages with the
	 * transmitted message parameters.
	 */
	void poll(IClientMessages& client_messages);

private:

	std::unique_ptr<IChannel> m_channel;

};

/**
 * The ClientProtocol implements sending and receiving messages on the client
 * side over a network channel as defined above.
 */
class ClientProtocol : public IClientMessages
{

public:

	/**
	 * Construct the protocol to use the given underlying channel.
	 * The protocol assumes ownership of the channel.
	 */
	explicit ClientProtocol(std::unique_ptr<IChannel> channel);

	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;

	/**
	 * For every unhandled message in the underlying channel's memory, invoke
	 * the appropriate member function of the given @c IServerMessages with the
	 * transmitted message parameters.
	 */
	void poll(IServerMessages& server_messages);

private:

	std::unique_ptr<IChannel> m_channel;

};

// ==================== integration with game logic ====================

/**
 * Interface for classes that implement client logic.
 * Connects network messages with the game state and function calls.
 */
class IClient
{

public:

	virtual ~IClient() = default;

	/**
	 * Return the game-round data, if the game is currently ongoing.
	 * The game must be in progress on this client when calling.
	 */
	virtual GameData& gamedata() = 0;

	/**
	 * Return true if the server expects us to start the game now, but the game
	 * state is not yet constructed.
	 */
	virtual bool is_game_ready() const noexcept = 0;

	/**
	 * Return true if the gamedata is valid at the moment.
	 * If this is not the case, accessing `gamedata()` will throw.
	 */
	virtual bool is_ingame() const noexcept = 0;

	/**
	 * Initialize the game state from the meta information.
	 * Note: After this call, all game event handlers in
	 *       `gamedata().rules.event_hub` need to be freshly set up
	 *       as they are not preserved in-between game restarts.
	 */
	virtual void game_start() = 0;

	/**
	 * Apply the given input to the game by sending it to the server.
	 * In the future, we will pre-emptively trust our self-made inputs.
	 */
	virtual void send_input(Input input) = 0;

	/**
	 * Signal to the server that we want to start a fresh game.
	 * TODO: This should only work from a privileged client.
	 */
	virtual void send_reset(GameMeta meta) = 0;

	/**
	 * Signal to the server that we want to change the speed of the game.
	 * To pause the game, set the speed to 0.
	 * To run at regular speed, set the speed to 1.
	 * TODO: This should only work from a privileged client.
	 */
	virtual void send_speed(int speed) = 0;

	/**
	 * Receive and handle incoming messages from the server.
	 */
	virtual void poll() = 0;

};

/**
 * Client logic implementation.
 * This implementation handles the basic game flow while coordinating with a
 * server over a channel.
 */
class BasicClient : public IClient, private IServerMessages
{

public:

	/**
	 * Construct the client to communicate via the given protocol.
	 * The @c BasicClient is the owner of the game state during the game round.
	 */
	explicit BasicClient(ClientProtocol protocol);
	virtual ~BasicClient() noexcept;

	// IClient member functions - implementation of client-side game flow
	virtual GameData& gamedata() override { enforce(m_gamedata.has_value()); return *m_gamedata; }
	virtual bool is_game_ready() const noexcept override;
	virtual bool is_ingame() const noexcept override;
	virtual void game_start() override;
	virtual void send_input(Input input) override;
	virtual void send_reset(GameMeta meta) override;
	virtual void send_speed(int speed) override;
	virtual void poll() override;

private:

	ClientProtocol m_protocol; //!< communicator object

	std::optional<GameMeta> m_meta; //!< Server information from which to initialize the state
	std::optional<GameData> m_gamedata; //!< information during the game round
	int m_ready = 0; //!< quick-and-dirty state machine. 0=menu, 1=ready, 2=ingame

	// IServerMessages member functions - handlers for incoming messages
	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;
	virtual void gameend(int winner) override;

	/**
	 * Initialize the game state from the meta information.
	 * Note: After this call, all game event handlers in
	 *       `gamedata().rules.event_hub` need to be freshly set up
	 *       as they are not preserved in-between game restarts.
	 */
	void game_start_impl();

};

/**
 * Local-only client logic implementation.
 * This implementation offers an interface as if the
 * server was always immediately responsive.
 */
class LocalClient : public IClient
{

public:

	virtual ~LocalClient() noexcept;

	virtual GameData& gamedata() override { enforce(m_gamedata.has_value()); return *m_gamedata; }
	virtual bool is_game_ready() const noexcept override;
	virtual bool is_ingame() const noexcept override;
	virtual void game_start() override;
	virtual void send_input(Input input) override;
	virtual void send_reset(GameMeta meta) override;
	virtual void send_speed(int speed) override;
	virtual void poll() override;

private:

	std::optional<GameMeta> m_meta; //!< Information from which to initialize the state
	std::optional<GameData> m_gamedata; //!< information during the game round
	int m_ready = 0; //!< quick-and-dirty state machine. 0=menu, 1=ready, 2=ingame

};

/**
 * Server logic implementation.
 * Connects network messages with the game state and function calls.
 */
class BasicServer : private IClientMessages
{

public:

	/**
	 * Construct the server to communicate via the given protocol.
	 * The @c BasicServer is the owner of the game state during the game round.
	 */
	explicit BasicServer(ServerProtocol protocol);
	~BasicServer() noexcept;

	/**
	 * Return the game-round data, if the game is currently ongoing.
	 */
	GameData& gamedata() noexcept { enforce(m_gamedata.has_value()); return *m_gamedata; }

	/**
	 * Return true if the prerequisite data is available, but the game
	 * state is not yet constructed.
	 */
	bool is_game_ready() const noexcept;

	/**
	 * Return true if the gamedata is valid at the moment.
	 * If this is not the case, accessing `gamedata()` will throw.
	 */
	bool is_ingame() const noexcept;

	/**
	 * Initialize the game state from the meta information.
	 */
	void game_start();

	/**
	 * Notify the clients that the game has been decided in favor of the given player.
	 */
	void send_gameend(int winner);

	/**
	 * Receive and handle incoming messages from the clients.
	 */
	void poll();

private:

	ServerProtocol m_protocol; //!< communicator object

	std::optional<GameMeta> m_meta; //!< Server information from which to initialize the state
	std::optional<GameData> m_gamedata; //!< information during the game round

	// IClientMessages member functions - handlers for incoming messages
	virtual void meta(GameMeta meta) override;
	virtual void input(Input input) override;
	virtual void speed(int speed) override;
	virtual void start() override;

};

/**
 * Runs a server in a thread until the object is destroyed.
 */
class ServerThread
{

public:

	/**
	 * Start a game server in a separate thread.
	 */
	ServerThread(std::unique_ptr<BasicServer> server);

	/**
	 * Exit from the server thread, if necessary.
	 * Catch all exceptions and log them, if possible.
	 */
	~ServerThread();

	/**
	 * End execution of the server thread, with the possibility to handle
	 * exceptions that propagate out of the thread.
	 * In contrast, the destructor swallows all exceptions.
	 */
	void exit();

private:

	std::atomic_flag m_exit;
	std::future<void> m_future;
	std::unique_ptr<BasicServer> m_server;

	/**
	 * Main entry point of the thread.
	 * It will periodically check the @c m_exit flag while handling requests.
	 */
	void main_loop();

};
