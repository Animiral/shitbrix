/**
 * Interfaces for remote communication between different game instances.
 *
 * The abstract classes facilitate the following protocol, in broad terms:
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
#include <future>
#include <atomic>
#include "globals.hpp"
#include "stage.hpp"
#include "error.hpp"

// ==================== high-level interfaces ====================

/**
 * Represents a game not yet in progress.
 */
struct Offer {};

class Client;
class Server;
class Lobby;
class Host;

/**
 * Interface for messages sent from the host to the client.
 */
class Client
{

public:

	/**
	 * Construct the interface for the client with the given name.
	 */
	Client(std::string name) : m_name(move(name)) {}
	virtual ~Client() = default;

	std::string m_name; //!< Server-wide alias of this client

	// --- messages from Server

	virtual void list(const std::vector<Offer>& offers) = 0;

	// --- messages from Lobby

	virtual void start(std::unique_ptr<Host> host) = 0;

	// --- messages from Host

	virtual void set_meta(const GameMeta& meta) = 0;
	virtual void set_player(int player) = 0;
	virtual void input(const GameInput& input) = 0;
	virtual void sync_state(const GameState& state) = 0;

	virtual void accept(Host& receiver) const = 0;
	virtual void accept(Server& receiver) const = 0;
	virtual void accept(Lobby& receiver) const = 0;

};

/**
 * Interface for messages sent from the client to the list server.
 */
class Server
{

public:

	virtual ~Server() = default;

	virtual std::unique_ptr<Lobby> offer(Offer offer) = 0;
	virtual void remove(const Offer& offer) = 0;
	virtual std::unique_ptr<Lobby> join(const Offer& offer) = 0;

	virtual void accept(Client& receiver) = 0;

};

class Lobby
{

public:

	virtual ~Lobby() = default;

	// --- messages from Host

	virtual std::vector<std::unique_ptr<Client>> start() = 0;

	// --- messages from Client

	virtual void bye() = 0;

	virtual void accept(Client& receiver) = 0;
	virtual void accept(Host& receiver) = 0;

};

/**
 * Interface for sending messages to the host.
 */
class Host
{

public:

	virtual ~Host() = default;

	// --- messages from Lobby

	virtual void set_clients(const std::vector<std::unique_ptr<Client>>& clients) = 0;

	// --- messages from Client

	virtual void input(const GameInput& input) = 0;

	virtual void accept(Lobby& receiver) = 0;
	virtual void accept(Client& receiver) = 0;

};

/**
 * The interface that new clients and hosts can use to
 * introduce themselves to the server.
 */
class Reception
{

public:

	virtual ~Reception() = default;

	virtual std::unique_ptr<Server> check_in(const std::string& name) = 0;

};

// ==================== postal system ====================

#include <queue>

enum class MsgType
{
	META,    //!< game meta-information
	PLAYER,  //!< set player number of client
	INPUT,   //!< player input in the game
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

class Mailbox
{

public:

	void enqueue(Message message);
	void poll(Host& recipient);
	void poll(Lobby& recipient);
	void poll(Server& recipient);
	void poll(Client& recipient);
	void poll(Reception& recipient);

private:

	std::queue<Message> m_queue;

};

class PostMaster
{

public:


};

// ==================== fake implementation ====================

#include <unordered_map>

class FakeReception;
class FakeClient;
class FakeServer;
class FakeLobby;
class FakeHost;

struct FakeStore
{
	std::unique_ptr<FakeReception> reception;
	std::unique_ptr<FakeServer> server;
	std::unordered_map<std::string, std::unique_ptr<FakeClient>> clients;
};

class FakeReception : public Reception
{

public:

	FakeReception(FakeStore& store);

	virtual std::unique_ptr<Server> check_in(const std::string& name) override;

private:

	FakeStore& m_store;

};

class FakeClient : public Client
{

public:

	FakeClient(FakeStore& store, std::string name);

	virtual void list(const std::vector<Offer>& offers) override;

	// --- messages from Lobby

	virtual void start(std::unique_ptr<Host> host) override;

	// --- messages from Host

	virtual void set_meta(const GameMeta& meta) override;
	virtual void set_player(int player) override;
	virtual void input(const GameInput& input) override;
	virtual void sync_state(const GameState& state) override;

	virtual void accept(Host& receiver) const override;
	virtual void accept(Server& receiver) const override;
	virtual void accept(Lobby& receiver) const override;

private:

	FakeStore& m_store;

};

class FakeServer : public Server
{

public:

	virtual std::unique_ptr<Lobby> offer(Offer offer) override;
	virtual void remove(const Offer& offer) override;
	virtual std::unique_ptr<Lobby> join(const Offer& offer) override;

	virtual void accept(Client& receiver) override;

};

class FakeLobby : public Lobby
{

public:

	virtual std::vector<std::unique_ptr<Client>> start() override;

	// --- messages from Client

	virtual void bye() override;

	virtual void accept(Client& receiver) override;
	virtual void accept(Host& receiver) override;

};

class FakeHost : public Host
{

public:

	virtual void set_clients(const std::vector<std::unique_ptr<Client>>& clients) override;

	// --- messages from Client

	virtual void input(const GameInput& input) override;

	virtual void accept(Lobby& receiver) override;
	virtual void accept(Client& receiver) override;

};

class FakeNetworkFactory
{

public:

	std::unique_ptr<Reception> create_reception();
	std::unique_ptr<Server> create_server();

	std::unique_ptr<Lobby> create_host_lobby();
	std::unique_ptr<Lobby> create_client_lobby();

	std::unique_ptr<Host> create_lobby_host();
	std::unique_ptr<Host> create_client_host();

	std::unique_ptr<Client> create_server_client(std::string name);
	std::unique_ptr<Client> create_lobby_client(std::string name);
	std::unique_ptr<Client> create_host_client(std::string name);

private:

	FakeStore m_store;

};

// ==================== simple integration with game logic ====================

#include "replay.hpp"
#include "director.hpp"
#include "input.hpp"
#include "enet_helper.hpp"

/**
 * Contains the data that one side in a networked game needs to keep track of
 * the ongoing game round.
 */
struct GameData
{
	explicit GameData(GameState state, Journal journal);
	GameData(const GameData& rhs) = delete;
	GameData(GameData&& rhs);
	GameData& operator=(GameData& ) = delete;
	GameData& operator=(GameData&& rhs);

	Dials dials; //!< Extra-journal control settings for the current game session
	GameState state; //!< Active and always current game state container
	Rules rules; //!< Game state manipulation routines
	Journal journal; //!< Game events and checkpoints record
};

/**
 * Low-level server implementation.
 * Keeps track of client connections and communicates in @c Messages.
 */
class ENetServer
{

public:

	/**
	 * Construct the server to listen on all network addresses on the port
	 * number specified in the global constant @c NET_PORT.
	 */
	ENetServer(enet_uint16 port);

	/**
	 * Send the message to all clients.
	 */
	void broadcast_message(Message message);

	/**
	 * Listen for client messages and return them.
	 */
	std::vector<Message> poll();

private:

	const HostPtr m_host;

};

/**
 * Low-level client implementation.
 * Keeps track of the connection to the server and communicates in @c Messages.
 */
class ENetClient
{

public:

	/**
	 * Construct the client by connecting to the given server.
	 */
	explicit ENetClient(const char* server_name, enet_uint16 port);

	/**
	 * Send the given message to the server on the MESSAGE_CHANNEL.
	 */
	void send_message(MsgType type, std::string data);

	/**
	 * Handle events and possible new messages from the server.
	 */
	std::vector<Message> poll();

private:

	HostPtr m_host;    //!< ENetHost object
	ENetPeer* m_peer;  //!< ENet peer associated with the server

};

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
	virtual void send_input(GameInput input) = 0;

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
 * This implementation uses communication over a real network.
 */
class BasicClient : public IClient
{

public:

	/**
	 * Construct the client to communicate via the given low-level interface.
	 * The @c BasicClient is the owner of the game state during the game round.
	 */
	explicit BasicClient(std::unique_ptr<ENetClient> client);

	virtual GameData& gamedata() override { enforce(m_gamedata.has_value()); return *m_gamedata; }
	virtual bool is_game_ready() const noexcept override;
	virtual bool is_ingame() const noexcept override;
	virtual void game_start() override;
	virtual void send_input(GameInput input) override;
	virtual void send_reset(GameMeta meta) override;
	virtual void send_speed(int speed) override;
	virtual void poll() override;

private:

	const std::unique_ptr<ENetClient> m_client; //!< low-level communicator object

	std::optional<GameMeta> m_meta; //!< Server information from which to initialize the state
	std::optional<GameData> m_gamedata; //!< information during the game round
	int m_ready = 0; //!< quick-and-dirty state machine. 0=menu, 1=ready, 2=ingame

	/**
	 * Process a single message.
	 * The appropriate changes and effects are then seen in the game state and journal.
	 */
	void handle_message(const Message& message);

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

	virtual GameData& gamedata() override { enforce(m_gamedata.has_value()); return *m_gamedata; }
	virtual bool is_game_ready() const noexcept override;
	virtual bool is_ingame() const noexcept override;
	virtual void game_start() override;
	virtual void send_input(GameInput input) override;
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
class BasicServer
{

public:

	/**
	 * Construct the server to communicate via the given low-level interface.
	 * The @c BasicServer is the owner of the game state during the game round.
	 */
	explicit BasicServer(std::unique_ptr<ENetServer> server);

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

	std::unique_ptr<ENetServer> m_server;

	std::optional<GameMeta> m_meta; //!< Server information from which to initialize the state
	std::optional<GameData> m_gamedata; //!< information during the game round

	/**
	 * Process a single message.
	 * Most often, this means to validate it and forward it to all connected peers.
	 */
	void handle_message(const Message& message);

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
