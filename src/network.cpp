#include "network.hpp"

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

SimpleHost::SimpleHost()
	: m_sink(nullptr)
{
	m_server = the_reception->check_in("placeholder");
	m_lobby = m_server->offer({});
}
