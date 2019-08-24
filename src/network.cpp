#include "network.hpp"
#include "replay.hpp"
#include "arbiter.hpp"
#include <sstream>
#include <cassert>
#include "error.hpp"

// These two libraries are dependencies of ENet.
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace
{

const char* msgtype_string[] =
{"META", "PLAYER", "INPUT", "RETRACT",
 "SPEED", "SYNC", "CLIENTS", "START", "GAMEEND",
 "BYE", "OFFER", "REMOVE", "JOIN", "LIST", "CHECKIN"};

}

std::string Message::to_string() const
{
	const size_t type_index = static_cast<size_t>(type);
	assert(type_index < std::size(msgtype_string));

	std::ostringstream ss;
	ss << sender << " " << recipient << " "
	   << msgtype_string[type_index] << " " << data;
	return ss.str();
}

Message Message::from_string(std::string message_string)
{
	std::istringstream tokenizer(message_string);

	int sender;
	int recipient;
	std::string type_string;
	std::string data;

	tokenizer >> sender >> recipient >> type_string >> std::ws;
	if(!tokenizer)
		throw GameException("Invalid Message string: \"" + message_string + "\"");

	std::getline(tokenizer, data);

	const auto type_found = std::find(msgtype_string, std::end(msgtype_string), type_string);
	const size_t type_index = std::distance(msgtype_string, type_found);
	if(std::size(msgtype_string) <= type_index)
		throw GameException("Invalid Message type string: \"" + type_string + "\"");

	return Message{sender, recipient, static_cast<MsgType>(type_index), data};
}

void Mailbox::enqueue(Message message)
{
	m_queue.push(message);
}

void Mailbox::poll(Host& recipient)
{
	const Message& message = m_queue.front();

	switch(message.type) {

	case MsgType::INPUT:
		{
			// TODO: parse message.data
			Input input{PlayerInput{PlayerInput::TIME_ASAP, 0, GameButton::SWAP, ButtonAction::DOWN}};
			recipient.input(input);
		}
		break;

	case MsgType::BYE:
		{
			// TODO: implement this message
		}
		break;

	default:
		// drop message
		break;

	}
}

void Mailbox::poll(Lobby& recipient)
{
	const Message& message = m_queue.front();

	switch(message.type) {

	case MsgType::BYE:
		{
			// TODO: implement this message
		}
		break;

	default:
		// drop message
		break;

	}
}

void Mailbox::poll(Server& recipient)
{
	const Message& message = m_queue.front();

	switch(message.type) {

	case MsgType::BYE:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::CLIENTS:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::OFFER:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::REMOVE:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::JOIN:
		{
			// TODO: implement this message
		}
		break;

	default:
		// drop message
		break;

	}
}

void Mailbox::poll(Client& recipient)
{
	const Message& message = m_queue.front();

	switch(message.type) {

	case MsgType::BYE:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::META:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::INPUT:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::SYNC:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::CLIENTS:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::START:
		{
			// TODO: implement this message
		}
		break;

	case MsgType::LIST:
		{
			// TODO: implement this message
		}
		break;

	default:
		// drop message
		break;

	}
}

void Mailbox::poll(Reception& recipient)
{
	const Message& message = m_queue.front();

	switch(message.type) {

	case MsgType::CHECKIN:
		{
			// TODO: implement this message
		}
		break;

	default:
		// drop message
		break;

	}
}

FakeReception::FakeReception(FakeStore& store)
	: m_store(store)
{}

std::unique_ptr<Server> FakeReception::check_in(const std::string& name)
{
	// TODO: implement
	m_store.clients[name] = std::make_unique<FakeClient>(m_store, name);
	return std::make_unique<FakeServer>(*m_store.server);
}

FakeClient::FakeClient(FakeStore& store, std::string name)
	: Client(name), m_store(store)
{
}

void FakeClient::list(const std::vector<Offer>& offers)
{
	// TODO: implement
}

void FakeClient::start(std::unique_ptr<Host> host)
{
	// TODO: implement
}

void FakeClient::set_meta(const GameMeta& meta)
{
	// TODO: implement
}

void FakeClient::set_player(int player)
{
	// TODO: implement
}

void FakeClient::input(const Input& input)
{
	// TODO: implement
}

void FakeClient::sync_state(const GameState& state)
{
	// TODO: implement
}

void FakeClient::accept(Host& receiver) const
{
	// TODO: implement
}

void FakeClient::accept(Server& receiver) const
{
	// TODO: implement
}

void FakeClient::accept(Lobby& receiver) const
{
	// TODO: implement
}

std::unique_ptr<Lobby> FakeServer::offer(Offer offer)
{
	// TODO: implement
	return std::make_unique<FakeLobby>();
}

void FakeServer::remove(const Offer& offer)
{
	// TODO: implement
}

std::unique_ptr<Lobby> FakeServer::join(const Offer& offer)
{
	// TODO: implement
	return std::make_unique<FakeLobby>();
}

void FakeServer::accept(Client& receiver)
{
	// TODO: implement
}

std::vector<std::unique_ptr<Client>> FakeLobby::start()
{
	// TODO: implement
	return {};
}

void FakeLobby::bye()
{
	// TODO: implement
}

void FakeLobby::accept(Client& receiver)
{
	// TODO: implement
}

void FakeLobby::accept(Host& receiver)
{
	// TODO: implement
}

void FakeHost::set_clients(const std::vector<std::unique_ptr<Client>>& clients)
{
	// TODO: implement
}

void FakeHost::input(const Input& input)
{
	// TODO: implement
}

void FakeHost::accept(Lobby& receiver)
{
	// TODO: implement
}

void FakeHost::accept(Client& receiver)
{
	// TODO: implement
}

std::unique_ptr<Reception> FakeNetworkFactory::create_reception()
{
	// TODO: implement
	return std::make_unique<FakeReception>(m_store);
}

std::unique_ptr<Server> FakeNetworkFactory::create_server()
{
	// TODO: implement
	return std::make_unique<FakeServer>();
}

std::unique_ptr<Lobby> FakeNetworkFactory::create_host_lobby()
{
	// TODO: implement
	return std::make_unique<FakeLobby>();
}

std::unique_ptr<Lobby> FakeNetworkFactory::create_client_lobby()
{
	// TODO: implement
	return std::make_unique<FakeLobby>();
}

std::unique_ptr<Host> FakeNetworkFactory::create_lobby_host()
{
	// TODO: implement
	return std::make_unique<FakeHost>();
}

std::unique_ptr<Host> FakeNetworkFactory::create_client_host()
{
	// TODO: implement
	return std::make_unique<FakeHost>();
}

std::unique_ptr<Client> FakeNetworkFactory::create_server_client(std::string name)
{
	// TODO: implement
	return std::make_unique<FakeClient>(m_store, "placeholder");
}

std::unique_ptr<Client> FakeNetworkFactory::create_lobby_client(std::string name)
{
	// TODO: implement
	return std::make_unique<FakeClient>(m_store, "placeholder");
}

std::unique_ptr<Client> FakeNetworkFactory::create_host_client(std::string name)
{
	// TODO: implement
	return std::make_unique<FakeClient>(m_store, "placeholder");
}


ENetServer::ENetServer(enet_uint16 port)
	: m_host(ENet::instance().create_server(port))
{
}

void ENetServer::broadcast_message(Message message)
{
	Log::trace("Server send message: %s", message.to_string().c_str());
	PacketPtr packet = ENet::instance().create_packet(message.to_string(), ENET_PACKET_FLAG_RELIABLE);

	enet_host_broadcast(m_host.get(), MESSAGE_CHANNEL, packet.release());
	enet_host_flush(m_host.get());
}

std::vector<Message> ENetServer::poll()
{
	ENetEvent event;
	std::vector<Message> messages;

	while (enet_host_service (m_host.get(), &event, 0) > 0)
	{
		switch (event.type)
		{

		case ENET_EVENT_TYPE_CONNECT:
			Log::info("New client from %x:%u.", event.peer->address.host, event.peer->address.port);
			/* Store any relevant client information here. */
			//event.peer -> data = "Client information";
			//m_peer.emplace_back(event.peer);
			break;

		case ENET_EVENT_TYPE_RECEIVE:
		{
			PacketPtr packet{event.packet};
			// event.peer->data; // use this to identify the peer

			switch(event.channelID) {

			case MESSAGE_CHANNEL:
			{
				const std::string message_string{reinterpret_cast<char*>(packet->data)};
				Log::trace("Server got message: %s", message_string.c_str());
				messages.push_back(Message::from_string(message_string));
			}
				break;

			default:
				// drop packets from unknown channels
				Log::trace("Server got unknown data: %s", std::string{reinterpret_cast<char*>(packet->data)}.c_str());
				break;

			}
		}
			break;

		case ENET_EVENT_TYPE_DISCONNECT:
			Log::info("Client %x:%u disconnected.", event.peer->address.host, event.peer->address.port);
			/* Reset the peer's client information. */
			event.peer -> data = NULL;
			break;

		default:
			Log::error("ENet: unhandled event, type %d.", event.type);
			break;

		}
	}

	return messages;
}


ENetClient::ENetClient(const char* server_name, enet_uint16 port)
{
	std::tie(m_host, m_peer) = ENet::instance().create_client(server_name, port);

	/* Wait up to 5 seconds for the connection attempt to succeed. */
	ENetEvent event;
	if (enet_host_service(m_host.get(), &event, CONNECT_TIMEOUT) <= 0 ||
		event.type != ENET_EVENT_TYPE_CONNECT)
	{
		throw ENetException("Connection to server failed.");
	}
}

void ENetClient::send_message(MsgType type, std::string data)
{
	const Message message{0, 0, type, data};
	Log::trace("Client send message: %s", message.to_string().c_str());
	PacketPtr packet = ENet::instance().create_packet(message.to_string(), ENET_PACKET_FLAG_RELIABLE);

	/* enet_host_broadcast (host, 0, packet);         */
	enetok(enet_peer_send(m_peer, MESSAGE_CHANNEL, packet.release()));
	enet_host_flush(m_host.get());
}

std::vector<Message> ENetClient::poll()
{
	ENetEvent event;
	std::vector<Message> messages;

	while (enet_host_service (m_host.get(), &event, 0) > 0)
	{
		switch (event.type)
		{

		case ENET_EVENT_TYPE_RECEIVE:
		{
			const PacketPtr packet{event.packet};

			enforce(MESSAGE_CHANNEL == event.channelID); // more channels in the future?

			const std::string message_string{reinterpret_cast<char*>(packet->data)};
			Log::trace("Client got message: %s", message_string.c_str());
			messages.push_back(Message::from_string(message_string));
		}
			break;

		case ENET_EVENT_TYPE_DISCONNECT:
			Log::info("Disconnected from server.");
			/* Reset the peer's client information. */
			event.peer->data = NULL;
			break;

		default:
			Log::error("ENet: unhandled event, type %d.", event.type);
			break;

		}
	}

	return messages;
}


namespace
{

/**
 * Construct and wire all the objects that we need to run
 * the game session from the meta-information.
 */
GameData make_gamedata(GameMeta meta)
{
	Log::info("Initialize new game for %d players, seed=%u.", meta.players, meta.seed);

	// [meta](int player) { return std::make_unique<RandomColorSupplier>(meta.seed, player); };
	// IArbiter arbiter = // some arbiter instead of RandomColorSupplier
	auto state = std::make_unique<GameState>(meta);
	auto journal = std::make_unique<Journal>(meta, *state);

	return GameData(move(state), move(journal));
}

/**
 * Construct and wire all the objects that we need to run
 * a local game session from the meta-information.
 */
GameData make_local_gamedata(GameMeta meta)
{
	Log::info("Initialize new local game for %d players, seed=%u.", meta.players, meta.seed);

	// [meta](int player) { return std::make_unique<RandomColorSupplier>(meta.seed, player); };
	// IArbiter arbiter = // some arbiter instead of RandomColorSupplier
	auto state = std::make_unique<GameState>(meta);
	auto journal = std::make_unique<Journal>(meta, *state);
	auto color_supplier = std::make_unique<RandomColorSupplier>(meta.seed, 0);
	auto arbiter = std::make_unique<LocalArbiter>(*state, *journal, move(color_supplier));

	return GameData(move(state), move(journal), move(arbiter));
}

}


BasicClient::BasicClient(std::unique_ptr<ENetClient> client)
: m_client(std::move(client))
{}

BasicClient::~BasicClient() noexcept = default;

bool BasicClient::is_game_ready() const noexcept
{
	return 1 == m_ready;
}

bool BasicClient::is_ingame() const noexcept
{
	return 2 == m_ready;
}

void BasicClient::game_start()
{
	game_start_impl();
	m_ready = 2;
}

void BasicClient::send_input(Input input)
{
	m_client->send_message(MsgType::INPUT, std::string(input));
}

void BasicClient::send_reset(GameMeta meta)
{
	m_client->send_message(MsgType::META, meta.to_string());
	m_client->send_message(MsgType::START, {});
}

void BasicClient::send_speed(int speed)
{
	m_client->send_message(MsgType::SPEED, std::to_string(speed));
}

void BasicClient::poll()
{
	const auto messages = m_client->poll();

	for(const auto& m : messages)
		handle_message(m);
}

void BasicClient::handle_message(const Message& message)
{
	switch(message.type) {

	case MsgType::INPUT:
	{
		if(!m_gamedata.has_value())
			throw GameException("Got input from server before the game is running.");

		m_gamedata->journal->add_input(Input(message.data));
	}
		break;

	case MsgType::SPEED:
	{
		if(!m_gamedata.has_value())
			throw GameException("Got speed from server before the game is running.");

		const int speed = std::stoi(message.data);
		m_gamedata->dials.speed = speed;
	}
		break;

	case MsgType::META:
	{
		m_meta = GameMeta::from_string(message.data);
		m_gamedata.reset(); // new meta info invalidates game state and history
		m_ready = 1;
	}
		break;

	case MsgType::START:
	{
		// Towards the outside, we must pretend not to have constructed the
		// game state yet. We are merely “ready”. However, the server might
		// already send us input messages, which we must be able to handle.
		game_start_impl();
	}
		break;

	case MsgType::GAMEEND:
	{
		if(!m_gamedata.has_value())
			throw GameException("Got gameend from server before the game is running.");

		const int winner = std::stoi(message.data);
		m_gamedata->journal->set_winner(winner);
	}
		break;

	default:
		assert(!"not implemented yet");

	}
}

void BasicClient::game_start_impl()
{
	enforce(is_game_ready() || is_ingame());

	if(m_gamedata.has_value())
		return; // do not prepare twice

	m_gamedata = make_gamedata(*m_meta);
}


LocalClient::~LocalClient() noexcept = default;

bool LocalClient::is_game_ready() const noexcept
{
	return 1 == m_ready;
}

bool LocalClient::is_ingame() const noexcept
{
	return 2 == m_ready;
}

void LocalClient::game_start()
{
	enforce(is_game_ready() || is_ingame());

	m_ready = 2;

	if(m_gamedata.has_value())
		return; // do not prepare twice

	m_gamedata = make_local_gamedata(*m_meta);
}

void LocalClient::send_input(Input input)
{
	if(!m_gamedata.has_value())
		throw GameException("Got input before the game is running.");

	m_gamedata->journal->add_input(std::move(input));
}

void LocalClient::send_reset(GameMeta meta)
{
	m_meta = meta;
	m_gamedata.reset(); // new meta info invalidates game state and history
	m_ready = 1;
	m_gamedata = make_local_gamedata(*m_meta);
}

void LocalClient::send_speed(int speed)
{
	m_gamedata->dials.speed = speed;
}

void LocalClient::poll()
{
	// game over check
	if(is_ingame() && m_gamedata->rules.block_director->over()) {
		const int winner = m_gamedata->rules.block_director->winner();
		m_gamedata->journal->set_winner(winner);
		m_ready = 0;
	}
}


BasicServer::BasicServer(std::unique_ptr<ENetServer> server)
: m_server(std::move(server))
{}

BasicServer::~BasicServer() noexcept = default;

bool BasicServer::is_game_ready() const noexcept
{
	return m_meta.has_value() && !m_gamedata.has_value();
}

bool BasicServer::is_ingame() const noexcept
{
	return m_gamedata.has_value();
}

void BasicServer::game_start()
{
	enforce(is_game_ready());

	m_gamedata = make_gamedata(*m_meta);
}

void BasicServer::send_gameend(int winner)
{
	const Message out_msg{0, 0, MsgType::GAMEEND, std::to_string(winner)};
	m_server->broadcast_message(std::move(out_msg));
}

void BasicServer::poll()
{
	const auto messages = m_server->poll();

	for(const auto& m : messages)
		handle_message(m);
}

void BasicServer::handle_message(const Message& message)
{
	// TODO: on error, properly discard the message and offending client

	switch(message.type) {

	case MsgType::INPUT:
	{
		const Input input(message.data);
		// TODO: validate input

		if(!is_ingame())
			throw GameException("Got input from client before the game is running.");
		m_gamedata->journal->add_input(input);

		const Message out_msg{
			message.sender,
			message.recipient,
			MsgType::INPUT,
			std::string(input)};
		m_server->broadcast_message(std::move(out_msg));
	}
		break;

	case MsgType::SPEED:
	{
		const int speed = std::stoi(message.data);
		// TODO: validate sender and input

		if(!is_ingame())
			throw GameException("Got speed from client before the game is running.");
		m_gamedata->dials.speed = speed;

		const Message out_msg{
			message.sender,
			message.recipient,
			MsgType::SPEED,
			std::to_string(speed)};
		m_server->broadcast_message(std::move(out_msg));
	}
		break;

	case MsgType::META:
	{
		// TODO: validate sender
		m_meta = GameMeta::from_string(message.data);
		m_gamedata.reset(); // new meta info invalidates game state and history

		const Message out_msg{
			message.sender,
			message.recipient,
			MsgType::META,
			m_meta->to_string()};
		m_server->broadcast_message(std::move(out_msg));
	}
		break;

	case MsgType::START:
	{
		// TODO: validate sender
		const Message out_msg{
			message.sender,
			message.recipient,
			MsgType::START,
			{}};
		m_server->broadcast_message(std::move(out_msg));
	}
		break;

	default:
		assert(!"not implemented yet");

	}
}


ServerThread::ServerThread(std::unique_ptr<BasicServer> server)
	: m_server(std::move(server))
{
	enforce(nullptr != m_server);

	m_exit.test_and_set(); // flag is now known set
	m_future = std::async([this] { main_loop(); });
}

ServerThread::~ServerThread()
{
	try {
		exit();
	}
	catch(const std::exception& ex) {
		show_error(ex);
	}
	catch(...) {
		Log::error("Unknown exception occurred.");
	}
}

void ServerThread::exit()
{
	if(m_future.valid()) {
		Log::info("Server thread exit.");
		m_exit.clear(); // this signals the server to exit
		m_future.get(); // propagate exceptions from server thread
	}
}

void ServerThread::main_loop()
{
	set_thread_name("Server Thread");

	// TODO: this code duplicates code from the GameLoop::game_loop function.
	//       It should be refactored so that the timed loop is owned/run
	//       by the active screen and based on a common ILoop.

	Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
	Uint64 freq = SDL_GetPerformanceFrequency();
	long tick = 0; // current logic tick counter
	Uint64 next_logic = t0 + freq / TPS; // time for next logic update
	bool in_game = false; // true if the game round is in progress

	while(m_exit.test_and_set())
	{
		// process messages as long as logic is up to date
		Uint64 now = SDL_GetPerformanceCounter();
		while (now < next_logic) {
			m_server->poll();
			now = SDL_GetPerformanceCounter();

			// yield CPU if we have the time
			if(now < next_logic) {
				Uint64 wait = (next_logic - now) * 1000L / freq; // in ms
				assert(wait <= std::numeric_limits<Uint32>::max());
				SDL_Delay(static_cast<Uint32>(wait));
				now = SDL_GetPerformanceCounter();
			}
		}

		// start game at every opportunity
		if(m_server->is_game_ready()) {
			m_server->game_start();
			t0 = SDL_GetPerformanceCounter();
			tick = 0;
			in_game = true;
		}

		// run logic update, if applicable
		if(in_game && tick > INTRO_TIME) {
			const long game_time = tick - INTRO_TIME;
			GameData& gamedata = m_server->gamedata();
			synchronurse(*gamedata.state, game_time, *gamedata.journal, gamedata.rules);

			// game over check
			if(gamedata.rules.block_director->over()) {
				const int winner = gamedata.rules.block_director->winner();
				gamedata.journal->set_winner(winner);
				m_server->send_gameend(winner);
				replay_write(*gamedata.journal);
				in_game = false;
			}
		}

		tick++;
		next_logic = t0 + (tick + 1) * freq / TPS;
	}
}
