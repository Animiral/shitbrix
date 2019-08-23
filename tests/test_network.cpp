/**
 * Tests for the communication infrastructure.
 */

#include "network.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

namespace
{

const enet_uint16 FREE_PORT = 12270; // usually works

}

class NetworkTest : public ::testing::Test
{

public:

	explicit NetworkTest()
	{
		configure_context_for_testing();
		m_server = ENet::instance().create_server(FREE_PORT);
	}

private:

	HostPtr m_server;
};

/**
 * Tests whether messages are correctly (de-)serialized.
 */
TEST_F(NetworkTest, MessageSerialization)
{
	Message m;
	m.sender = 1;
	m.recipient = 2;

	// ====== Message to string ======

	m.type = MsgType::START;
	m.data = "";
	EXPECT_EQ(m.to_string(), "1 2 START ");

	m.type = MsgType::GAMEEND;
	m.data = "1";
	EXPECT_EQ(m.to_string(), "1 2 GAMEEND 1");

	m.type = MsgType::META;
	m.data = "2 4711 -1";
	EXPECT_EQ(m.to_string(), "1 2 META 2 4711 -1");

	m.type = MsgType::SPEED;
	m.data = "1";
	EXPECT_EQ(m.to_string(), "1 2 SPEED 1");

	m.type = MsgType::INPUT;
	m.data = "50 1 LEFT DOWN";
	EXPECT_EQ(m.to_string(), "1 2 INPUT 50 1 LEFT DOWN");

	m.type = MsgType::RETRACT;
	m.data = "50";
	EXPECT_EQ(m.to_string(), "1 2 RETRACT 50");

	// ====== string to Message ======

	m = Message::from_string("3 4 START ");
	EXPECT_EQ(m.sender, 3);
	EXPECT_EQ(m.recipient, 4);
	EXPECT_EQ(m.type, MsgType::START);
	EXPECT_EQ(m.data, "");

	m = Message::from_string("3 4 GAMEEND 1");
	EXPECT_EQ(m.sender, 3);
	EXPECT_EQ(m.recipient, 4);
	EXPECT_EQ(m.type, MsgType::GAMEEND);
	EXPECT_EQ(m.data, "1");

	m = Message::from_string("3 4 META 2 4711 -1");
	EXPECT_EQ(m.sender, 3);
	EXPECT_EQ(m.recipient, 4);
	EXPECT_EQ(m.type, MsgType::META);
	EXPECT_EQ(m.data, "2 4711 -1");

	m = Message::from_string("3 4 SPEED 1");
	EXPECT_EQ(m.sender, 3);
	EXPECT_EQ(m.recipient, 4);
	EXPECT_EQ(m.type, MsgType::SPEED);
	EXPECT_EQ(m.data, "1");

	m = Message::from_string("3 4 INPUT 50 1 LEFT DOWN");
	EXPECT_EQ(m.sender, 3);
	EXPECT_EQ(m.recipient, 4);
	EXPECT_EQ(m.type, MsgType::INPUT);
	EXPECT_EQ(m.data, "50 1 LEFT DOWN");

	m = Message::from_string("3 4 RETRACT 50");
	EXPECT_EQ(m.sender, 3);
	EXPECT_EQ(m.recipient, 4);
	EXPECT_EQ(m.type, MsgType::RETRACT);
	EXPECT_EQ(m.data, "50");
}
