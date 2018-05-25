#include "network.hpp"
#include "error.hpp"

// These two libraries are dependencies of ENet.

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

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
			GameInput input{GameInput::TIME_ASAP, 0, GameButton::SWAP, ButtonAction::DOWN};
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

void FakeClient::input(const GameInput& input)
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

void FakeHost::input(const GameInput& input)
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


ENetServer::ENetServer()
	: m_host(ENet::instance().create_server())
{
}

void ENetServer::broadcast_input(GameInput input)
{
	PacketPtr packet = ENet::instance().create_packet(input.to_string(), ENET_PACKET_FLAG_RELIABLE);

	enet_host_broadcast(m_host.get(), INPUT_CHANNEL, packet.release());
	enet_host_flush(m_host.get());
}

void ENetServer::poll()
{
	ENetEvent event;

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

			case INPUT_CHANNEL:
			{
				std::string input_string{reinterpret_cast<char*>(packet->data)};
				Log::trace("Server got input: %s", input_string.c_str());
				GameInput input = GameInput::from_string(input_string);
				// TODO: validate input
				broadcast_input(input);
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
}


namespace
{

Journal make_journal()
{
	GameMeta meta{2, std::random_device{}()};
	return Journal(meta, GameState{meta});
}

}


ENetClient::ENetClient(const char* server_name)
	: m_journal(make_journal())
{
	std::tie(m_host, m_peer) = ENet::instance().create_client(server_name);

	/* Wait up to 5 seconds for the connection attempt to succeed. */
	ENetEvent event;
	if (enet_host_service(m_host.get(), &event, CONNECT_TIMEOUT) <= 0 ||
		event.type != ENET_EVENT_TYPE_CONNECT)
	{
		throw ENetException("Connection to server failed.");
	}
}

void ENetClient::send_input(GameInput input)
{
	PacketPtr packet = ENet::instance().create_packet(input.to_string(), ENET_PACKET_FLAG_RELIABLE);

	/* enet_host_broadcast (host, 0, packet);         */
	enetok(enet_peer_send(m_peer, INPUT_CHANNEL, packet.release()));
	enet_host_flush(m_host.get());
}

void ENetClient::poll()
{
	ENetEvent event;

	while (enet_host_service (m_host.get(), &event, 0) > 0)
	{
		switch (event.type)
		{

		case ENET_EVENT_TYPE_RECEIVE:
		{
			PacketPtr packet{event.packet};

			enforce(INPUT_CHANNEL == event.channelID); // more channels in the future?

			std::string input_string{reinterpret_cast<char*>(packet->data)};
			GameInput input = GameInput::from_string(input_string);
			m_journal.add_input(input);
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
}


ClientStub::ClientStub()
	: m_journal(make_journal())
{
	//m_server = the_reception->check_in("placeholder");
	//m_lobby = m_server->offer({});
}

void ClientStub::send_input(GameInput input)
{
	// TODO: set input.game_time to server's time
	m_journal.add_input(input);
}


ServerThread::ServerThread()
{
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
	ENetServer server;

	while(m_exit.test_and_set())
	{
		server.poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
	}
}
