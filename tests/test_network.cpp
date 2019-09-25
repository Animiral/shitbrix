/**
 * Tests for the communication infrastructure.
 */

#include "network.hpp"
#include "input.hpp"
#include "tests_common.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::Truly;

class NetworkTest : public ::testing::Test
{

public:

	explicit NetworkTest()
	{
		auto channels = make_test_channels(1);
		m_server_protocol = std::make_unique<ServerProtocol>(move(channels.first));
		m_client_protocol = std::make_unique<ClientProtocol>(move(channels.second[0]));
	}

protected:

	std::unique_ptr<ServerProtocol> m_server_protocol;
	std::unique_ptr<ClientProtocol> m_client_protocol;

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

/**
 * Tests whether the ServerProtocol correctly passes the meta message.
 */
TEST_F(NetworkTest, ServerProtocolMeta)
{
	GameMeta meta{3, 1234, 1};
	m_server_protocol->meta(meta);

	MockServerMessages recipient;
	auto matches_meta = [] (GameMeta m)  { return 3 == m.players && 1234 == m.seed && 1 == m.winner; };
	EXPECT_CALL(recipient, meta(Truly(matches_meta))).Times(1);

	m_client_protocol->poll(recipient);
}

/**
 * Tests whether the ServerProtocol correctly passes the input message.
 */
TEST_F(NetworkTest, ServerProtocolInput)
{
	PlayerInput input{1, 2, GameButton::LEFT, ButtonAction::DOWN};
	m_server_protocol->input(Input{input});

	MockServerMessages recipient;
	auto matches_input = [](Input i) { auto pi = i.get<PlayerInput>(); return 1 == pi.game_time && 2 == pi.player && GameButton::LEFT == pi.button && ButtonAction::DOWN == pi.action; };
	EXPECT_CALL(recipient, input(Truly(matches_input))).Times(1);

	m_client_protocol->poll(recipient);
}

/**
 * Tests whether the ServerProtocol correctly passes the retract message.
 */
TEST_F(NetworkTest, ServerProtocolRetract)
{
	m_server_protocol->retract(1);

	MockServerMessages recipient;
	EXPECT_CALL(recipient, retract(1)).Times(1);

	m_client_protocol->poll(recipient);
}

/**
 * Tests whether the ServerProtocol correctly passes the speed message.
 */
TEST_F(NetworkTest, ServerProtocolSpeed)
{
	m_server_protocol->speed(1);

	MockServerMessages recipient;
	EXPECT_CALL(recipient, speed(1)).Times(1);

	m_client_protocol->poll(recipient);
}

/**
 * Tests whether the ServerProtocol correctly passes the start message.
 */
TEST_F(NetworkTest, ServerProtocolStart)
{
	m_server_protocol->start();

	MockServerMessages recipient;
	EXPECT_CALL(recipient, start()).Times(1);

	m_client_protocol->poll(recipient);
}

/**
 * Tests whether the ServerProtocol correctly passes the gameend message.
 */
TEST_F(NetworkTest, ServerProtocolGameend)
{
	m_server_protocol->gameend(2);

	MockServerMessages recipient;
	EXPECT_CALL(recipient, gameend(2)).Times(1);

	m_client_protocol->poll(recipient);
}

/**
 * Tests whether the ClientProtocol correctly passes the meta message.
 */
TEST_F(NetworkTest, ClientProtocolMeta)
{
	GameMeta meta{3, 1234, 1};
	m_client_protocol->meta(meta);

	MockClientMessages recipient;
	auto matches_meta = [] (GameMeta m)  { return 3 == m.players && 1234 == m.seed && 1 == m.winner; };
	EXPECT_CALL(recipient, meta(Truly(matches_meta))).Times(1);

	m_server_protocol->poll(recipient);
}

/**
 * Tests whether the ClientProtocol correctly passes the input message.
 */
TEST_F(NetworkTest, ClientProtocolInput)
{
	PlayerInput input{1, 2, GameButton::LEFT, ButtonAction::DOWN};
	m_client_protocol->input(Input{input});

	MockClientMessages recipient;
	auto matches_input = [](Input i) { auto pi = i.get<PlayerInput>(); return 1 == pi.game_time && 2 == pi.player && GameButton::LEFT == pi.button && ButtonAction::DOWN == pi.action; };
	EXPECT_CALL(recipient, input(Truly(matches_input))).Times(1);

	m_server_protocol->poll(recipient);
}

/**
 * Tests whether the ClientProtocol correctly passes the speed message.
 */
TEST_F(NetworkTest, ClientProtocolSpeed)
{
	m_client_protocol->speed(1);

	MockClientMessages recipient;
	EXPECT_CALL(recipient, speed(1)).Times(1);

	m_server_protocol->poll(recipient);
}

/**
 * Tests whether the ClientProtocol correctly passes the start message.
 */
TEST_F(NetworkTest, ClientProtocolStart)
{
	m_client_protocol->start();

	MockClientMessages recipient;
	EXPECT_CALL(recipient, start()).Times(1);

	m_server_protocol->poll(recipient);
}
