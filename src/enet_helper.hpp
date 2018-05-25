/**
 * C++-friendly wrappers for ENet stuff
 */
#pragma once

#include <enet/enet.h>
#include <memory>
#include <string>

/**
 * Custom deleter for ENet objects, to be used with unique_ptrs.
 */
class ENetDeleter
{
public:
	void operator()(ENetHost* p) const { enet_host_destroy(p); }
	void operator()(ENetPeer* p) const { enet_peer_reset(p); }
	void operator()(ENetPacket* p) const { enet_packet_destroy(p); }
};

using HostPtr = std::unique_ptr<ENetHost, ENetDeleter>;
using PeerPtr = std::unique_ptr<ENetPeer, ENetDeleter>;
using PacketPtr = std::unique_ptr<ENetPacket, ENetDeleter>;

/**
 * Singleton wrapper class and factory for ENet objects and wrappers.
 * Handles safe initialization and shutdown of the ENet library.
 * Methods get_* return a lazy-initialized object of which there is only one.
 * Methods create_* return a new object, often wrapped in a container.
 */
class ENet
{

public:

	static ENet& instance();

	ENet(const ENet& ) =delete;
	ENet& operator=(const ENet& ) =delete;

	// access single-instance objects
	//SDL_Window& window() const { return *m_window; }

	// creation methods

	/**
	 * Create a server host.
	 */
	HostPtr create_server() const;

	/**
	 * Create a client host and connect to the server.
	 */
	std::pair<HostPtr, ENetPeer*> create_client(const char* server_name) const;

	/**
	 * Create a packet.
	 */
	PacketPtr create_packet(const std::string& data, ENetPacketFlag flag) const;

private:

	ENet();
	~ENet();

	//std::unique_ptr<SDL_Window, SdlDeleter> m_window;

};
