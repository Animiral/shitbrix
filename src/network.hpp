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

// ==================== high-level interfaces ====================

#include <vector>
#include <string>
#include <memory>
#include <future>
#include <atomic>
#include "globals.hpp"
#include "stage.hpp"

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
	SYNC,    //!< whole game state
	CLIENTS, //!< request for or sync info about connected clients
	START,   //!< start game
	BYE,     //!< withdraw from the specified room
	OFFER,   //!< place a game offer
	REMOVE,  //!< retract a game offer
	JOIN,    //!< join a game offered
	LIST,    //!< transmit list of game offers
	CHECKIN, //!< initialize communication with Reception
};

struct Message
{
	int sender;       //!< sender queue number
	int recipient;    //!< recipient queue number
	MsgType type;     //!< message category
	std::string data; //!< encoded message arguments / payload
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
#include "input.hpp"
#include "enet_helper.hpp"

class ENetServer
{

public:

	ENetServer();

	void broadcast_input(GameInput input);
	void poll();

private:

	HostPtr m_host;
	std::vector<PeerPtr> m_peer;

};

class ENetClient
{

public:

	explicit ENetClient(const char* server_name);
	Journal& journal() noexcept { return m_journal; }
	void send_input(GameInput input);
	void poll();

private:

	HostPtr m_host;
	PeerPtr m_peer;
	Journal m_journal;

};


class ClientStub
{

public:

	ClientStub();

	Journal& journal() noexcept { return m_journal; }
	void send_input(GameInput input);
	void poll() {}

private:

	Journal m_journal;

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
	ServerThread();

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

	void main_loop();

};
