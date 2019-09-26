#include "network.hpp"
#include "replay.hpp"
#include "arbiter.hpp"
#include "game.hpp"
#include "director.hpp"
#include "enet_helper.hpp"
#include "error.hpp"
#include <sstream>
#include <cassert>

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


namespace
{

/**
 * The @c ServerChannel is a channel implementation that listens for clients
 * on the network. It broadcasts `Message` data structures to all connected
 * peers and receives messages from them.
 */
class ServerChannel : public IChannel
{

public:

	explicit ServerChannel(enet_uint16 port)
		: m_host(ENet::instance().create_server(port))
	{}

	virtual void send(Message message) override
	{
		Log::trace("Server send message: %s", message.to_string().c_str());
		PacketPtr packet = ENet::instance().create_packet(message.to_string(), ENET_PACKET_FLAG_RELIABLE);

		enet_host_broadcast(m_host.get(), MESSAGE_CHANNEL, packet.release());
		enet_host_flush(m_host.get());
	}

	virtual std::vector<Message> poll() override
	{
		ENetEvent event;
		std::vector<Message> messages;

		while(enet_host_service(m_host.get(), &event, 0) > 0) {
			switch(event.type) {

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
				event.peer->data = NULL;
				break;

			default:
				Log::error("ENet: unhandled event, type %d.", event.type);
				break;

			}
		}

		return messages;
	}

private:

	const HostPtr m_host;  //!< ENetHost object

};

/**
 * The @c ClientChannel is a channel implementation that connects to a
 * server listening on the network. It sends `Message` data structures
 * to the server and receives messages from it.
 */
class ClientChannel : public IChannel
{

public:

	explicit ClientChannel(const char* server_name, enet_uint16 port)
	{
		std::tie(m_host, m_peer) = ENet::instance().create_client(server_name, port);

		/* Wait up to 5 seconds for the connection attempt to succeed. */
		ENetEvent event;
		if(enet_host_service(m_host.get(), &event, CONNECT_TIMEOUT) <= 0 ||
			event.type != ENET_EVENT_TYPE_CONNECT) {
			throw ENetException("Connection to server failed.");
		}
	}

	virtual void send(Message message) override
	{
		Log::trace("Client send message: %s", message.to_string().c_str());
		PacketPtr packet = ENet::instance().create_packet(message.to_string(), ENET_PACKET_FLAG_RELIABLE);

		enetok(enet_peer_send(m_peer, MESSAGE_CHANNEL, packet.release()));
		enet_host_flush(m_host.get());
	}

	virtual std::vector<Message> poll() override
	{
		ENetEvent event;
		std::vector<Message> messages;

		while(enet_host_service(m_host.get(), &event, 0) > 0) {
			switch(event.type) {

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

private:

	HostPtr m_host;    //!< ENetHost object
	ENetPeer* m_peer;  //!< ENet peer associated with the server

};

}

std::unique_ptr<IChannel> make_server_channel(uint16_t port)
{
	return std::make_unique<ServerChannel>(port);
}

std::unique_ptr<IChannel> make_client_channel(const char* server_name, uint16_t port)
{
	return std::make_unique<ClientChannel>(server_name, port);
}


ServerProtocol::ServerProtocol(std::unique_ptr<IChannel> channel)
	: m_channel(move(channel))
{
	enforce(nullptr != m_channel);
}

void ServerProtocol::meta(GameMeta meta)
{
	const Message out_msg{0, 0, MsgType::META, meta.to_string()};
	m_channel->send(std::move(out_msg));
}

void ServerProtocol::input(Input input)
{
	const Message out_msg{0, 0, MsgType::INPUT, std::string(input)};
	m_channel->send(std::move(out_msg));
}

void ServerProtocol::retract(long cutoff_time)
{
	const Message out_msg{0, 0, MsgType::RETRACT, std::to_string(cutoff_time)};
	m_channel->send(std::move(out_msg));
}

void ServerProtocol::speed(int speed)
{
	const Message out_msg{0, 0, MsgType::SPEED, std::to_string(speed)};
	m_channel->send(std::move(out_msg));
}

void ServerProtocol::start()
{
	const Message out_msg{0, 0, MsgType::START, {}};
	m_channel->send(std::move(out_msg));
}

void ServerProtocol::gameend(int winner)
{
	const Message out_msg{0, 0, MsgType::GAMEEND, std::to_string(winner)};
	m_channel->send(std::move(out_msg));
}

void ServerProtocol::poll(IClientMessages& client_messages)
{
	const auto messages = m_channel->poll();

	for(const auto& message : messages) {
		switch(message.type) {

		case MsgType::INPUT:
		{
			const Input input(message.data);
			client_messages.input(std::move(input));
		}
		break;

		case MsgType::SPEED:
		{
			const int speed = std::stoi(message.data);
			client_messages.speed(speed);
		}
		break;

		case MsgType::META:
		{
			const GameMeta meta = GameMeta::from_string(message.data);
			client_messages.meta(meta);
		}
		break;

		case MsgType::START:
			client_messages.start();
			break;

		default:
			assert(!"not implemented yet");

		}
	}
}

ClientProtocol::ClientProtocol(std::unique_ptr<IChannel> channel)
	: m_channel(move(channel))
{
	enforce(nullptr != m_channel);
}

void ClientProtocol::meta(GameMeta meta)
{
	const Message out_msg{0, 0, MsgType::META, meta.to_string()};
	m_channel->send(std::move(out_msg));
}

void ClientProtocol::input(Input input)
{
	const Message out_msg{0, 0, MsgType::INPUT, std::string(input)};
	m_channel->send(std::move(out_msg));
}

void ClientProtocol::speed(int speed)
{
	const Message out_msg{0, 0, MsgType::SPEED, std::to_string(speed)};
	m_channel->send(std::move(out_msg));
}

void ClientProtocol::start()
{
	const Message out_msg{0, 0, MsgType::START, {}};
	m_channel->send(std::move(out_msg));
}

void ClientProtocol::poll(IServerMessages& server_messages)
{
	const auto messages = m_channel->poll();

	for(const auto& message : messages) {
		switch(message.type) {

		case MsgType::INPUT:
		{
			const Input input(message.data);
			server_messages.input(std::move(input));
		}
		break;

		case MsgType::RETRACT:
		{
			const long cutoff_time = std::stoi(message.data);
			server_messages.retract(cutoff_time);
		}
		break;

		case MsgType::SPEED:
		{
			const int speed = std::stoi(message.data);
			server_messages.speed(speed);
		}
		break;

		case MsgType::META:
		{
			const GameMeta meta = GameMeta::from_string(message.data);
			server_messages.meta(meta);
		}
		break;

		case MsgType::START:
			server_messages.start();
			break;

		case MsgType::GAMEEND:
		{
			const int winner = std::stoi(message.data);
			server_messages.gameend(winner);
		}
		break;

		default:
			assert(!"not implemented yet");

		}
	}
}


ServerThread::ServerThread(std::unique_ptr<IGame> game)
	: m_game(std::move(game))
{
	enforce(nullptr != m_game);

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

	// count ticks from 0 when game starts
	m_game->after_start([&t0, &tick] { t0 = SDL_GetPerformanceCounter(); tick = 0; });

	while(m_exit.test_and_set())
	{
		// process messages as long as logic is up to date
		Uint64 now = SDL_GetPerformanceCounter();
		while (now < next_logic) {
			m_game->poll();
			now = SDL_GetPerformanceCounter();

			// yield CPU if we have the time
			if(now < next_logic) {
				Uint64 wait = (next_logic - now) * 1000L / freq; // in ms
				assert(wait <= std::numeric_limits<Uint32>::max());
				SDL_Delay(static_cast<Uint32>(wait));
				now = SDL_GetPerformanceCounter();
			}
		}

		// run logic update, if applicable
		if(m_game->switches().ingame && tick > INTRO_TIME) {
			const long game_time = tick - INTRO_TIME;
			m_game->synchronurse(game_time);
		}

		tick++;
		next_logic = t0 + ((Uint64)tick + 1) * freq / TPS;
	}
}
