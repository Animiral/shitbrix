#pragma once

#include "game.hpp"
#include "director.hpp"
#include "event.hpp"
#include "state.hpp"
#include "arbiter.hpp"
#include "network.hpp"
#include "replay.hpp"
#include "error.hpp"
#include <cassert>
#include <memory>
#include <optional>
#include <fstream>

IGame::~IGame() = default;

Switches IGame::switches()
{
	return m_switches;
}

const GameState& IGame::state() const
{
	enforce(m_switches.ingame);
	assert(m_state);
	return *m_state;
}

const Journal& IGame::journal() const
{
	enforce(m_switches.ingame);
	assert(m_journal);
	return *m_journal;
}

evt::GameEventHub& IGame::hub()
{
	enforce(m_switches.ingame);
	assert(m_hub);
	return *m_hub;
}

BlockDirector& IGame::director()
{
	enforce(m_switches.ingame);
	assert(m_director);
	return *m_director;
}

// debug helpers
namespace
{

/**
 * Write a dump file about the given game state.
 * The file path is built out of the optional dump/ directory, the time at which
 * the dump was written and the game_time of the state.
 * If any step does not work, do nothing.
 */
[[ maybe_unused ]]
void debug_dump_state(const GameState& state);

}

void IGame::synchronurse(long target_time)
{
	enforce(m_switches.ingame);
	assert(m_state);
	assert(m_journal);
	assert(m_director);

	// get events from journal, from which inputs will be applied to the state
	const long time0 = m_journal->earliest_undiscovered();

	if(time0 < target_time) {
		*m_state = m_journal->checkpoint_before(time0);
		Log::trace("%s(%d): revert to checkpoint before time=%d -> at time=%d.", __FUNCTION__, target_time, time0, m_state->game_time());
		debug_dump_state(*m_state);
	}

	InputSpan inputs = m_journal->discover_inputs(m_state->game_time() + 1, target_time);
	auto input_it = inputs.first;

	while(m_state->game_time() < target_time && !m_director->over()) {
		for(; input_it != inputs.second && input_it->input.game_time() == m_state->game_time() + 1; ++input_it) {
			Log::trace("%s(%d): apply input %s.", __FUNCTION__, target_time, std::string(input_it->input).c_str());
			m_director->apply_input(input_it->input);
		}

		// Run self-contained object behaviors. state.game_time() is incremented here.
		m_state->update();

		// Run updates based on game logic and interactions.
		m_director->update();
	}

	if(m_director->over())
		return; // stop feeding the journal now

	// save new checkpoint?
	if(target_time >= m_journal->checkpoint_before(target_time).game_time() + CHECKPOINT_INTERVAL) {
		Log::trace("%s(%d): save checkpoint at time=%d.", __FUNCTION__, target_time, m_state->game_time());
		m_journal->add_checkpoint(GameState(*m_state));
		debug_dump_state(*m_state);
	}
}

LocalGame::LocalGame() noexcept = default;

LocalGame::~LocalGame() noexcept = default;

void LocalGame::game_start()
{
	enforce(m_switches.ready);
	enforce(!m_switches.ingame);
	assert(m_meta.has_value());

	Log::info("Initialize new local game for %d players, seed=%u.", m_meta->players, m_meta->seed);

	m_switches.ingame = true;
	m_switches.winner = NOONE;

	m_state = std::make_unique<GameState>(*m_meta);
	m_journal = std::make_unique<Journal>(*m_meta, *m_state);
	m_director = std::make_unique<BlockDirector>();
	m_hub = std::make_unique<evt::GameEventHub>();
	m_director->set_handler(*m_hub);
	m_director->set_state(*m_state);

	auto color_supplier = std::make_unique<RandomColorSupplier>(m_meta->seed, 0);
	m_arbiter = std::make_unique<LocalArbiter>(*m_state, *m_journal, move(color_supplier));
	m_hub->subscribe(*m_arbiter);
}

void LocalGame::game_input(Input input)
{
	enforce(m_switches.ingame);
	assert(m_journal);

	m_journal->add_input(std::move(input));
}

void LocalGame::game_reset(int players)
{
	assert(2 == players); // different player numbers are not yet supported

	m_switches.ingame = false;
	m_switches.ready = true;

	static std::random_device rdev;
	m_meta = GameMeta{players, rdev(), NOONE};

	m_state.reset();
	m_journal.reset();
	m_director.reset();
	m_hub.reset();
	m_arbiter.reset();
}

void LocalGame::set_speed(int speed)
{
	m_switches.speed = speed;
}

void LocalGame::poll()
{
	// game over check
	if(m_switches.ingame && m_director->over()) {
		assert(m_director);
		assert(m_journal);
		const int winner = m_director->winner();
		m_journal->set_winner(winner);
		m_switches.winner = winner;
	}
}

ClientGame::ClientGame(ClientProtocol protocol) noexcept
	: m_protocol(std::move(protocol))
{}

void ClientGame::game_start()
{
	m_protocol.start();
}

void ClientGame::game_input(Input input)
{
	m_protocol.input(input);
}

void ClientGame::game_reset(int players)
{
	m_protocol.meta(GameMeta{players, 0});
}

void ClientGame::set_speed(int speed)
{
	m_protocol.speed(speed);
}

void ClientGame::poll()
{
	m_protocol.poll(*this);
}

void ClientGame::meta(GameMeta meta)
{
	assert(2 == meta.players); // different player numbers are not yet supported

	m_switches.ingame = false;
	m_switches.ready = true;
	m_meta = meta;

	m_state.reset();
	m_journal.reset();
	m_director.reset();
	m_hub.reset();
}

void ClientGame::input(Input input)
{
	assert(m_journal);

	if(!m_switches.ingame)
		throw GameException("Got input from server before the game is running.");

	m_journal->add_input(std::move(input));
}

void ClientGame::speed(int speed)
{
	m_switches.speed = speed;
}

void ClientGame::start()
{
	if(!m_switches.ready)
		throw GameException("Got start from server before the game is ready.");

	if(m_switches.ingame)
		throw GameException("Got start from server while the game is ongoing.");

	assert(m_meta.has_value());

	Log::info("Initialize new client game for %d players, seed=%u.", m_meta->players, m_meta->seed);

	m_switches.ingame = true;
	m_switches.winner = NOONE;

	m_state = std::make_unique<GameState>(*m_meta);
	m_journal = std::make_unique<Journal>(*m_meta, *m_state);
	m_director = std::make_unique<BlockDirector>();
	m_hub = std::make_unique<evt::GameEventHub>();
	m_director->set_handler(*m_hub);
	m_director->set_state(*m_state);
}

void ClientGame::gameend(int winner)
{
	assert(m_journal);

	if(!m_switches.ingame)
		throw GameException("Got gameend from server while the game is not running.");

	m_journal->set_winner(winner);
	m_switches.winner = winner;
}

ServerGame::ServerGame(ServerProtocol protocol) noexcept
	: m_protocol(std::move(protocol))
{}

void ServerGame::game_start()
{
	if(!m_switches.ready)
		throw GameException("Cannot start game before it is ready.");

	if(m_switches.ingame)
		throw GameException("Cannot start game while the game is ongoing.");

	assert(m_meta.has_value());

	Log::info("Initialize new server game for %d players, seed=%u.", m_meta->players, m_meta->seed);

	m_switches.ingame = true;
	m_switches.winner = NOONE;

	m_state = std::make_unique<GameState>(*m_meta);
	m_journal = std::make_unique<Journal>(*m_meta, *m_state);
	m_director = std::make_unique<BlockDirector>();
	m_hub = std::make_unique<evt::GameEventHub>();
	m_director->set_handler(*m_hub);
	m_director->set_state(*m_state);

	auto color_supplier = std::make_unique<RandomColorSupplier>(m_meta->seed, 0);
	m_arbiter = std::make_unique<ServerArbiter>(m_protocol, *m_state, *m_journal, move(color_supplier));
	m_hub->subscribe(*m_arbiter);

	m_protocol.start();
}

void ServerGame::game_input(Input input)
{
	if(!m_switches.ingame)
		throw GameException("Cannot handle inputs before the game is running.");

	assert(m_journal);
	m_journal->add_input(input);

	m_protocol.input(input);
}

void ServerGame::game_reset(int players)
{
	if(2 != players)
		throw GameException("Only 2 players are currently supported.");

	m_switches.ingame = false;
	m_switches.ready = true;

	static std::random_device rdev;
	m_meta = GameMeta{players, rdev(), NOONE};

	m_state.reset();
	m_journal.reset();
	m_director.reset();
	m_hub.reset();
	m_arbiter.reset();

	m_protocol.meta(*m_meta);
}

void ServerGame::set_speed(int speed)
{
	m_switches.speed = speed;
	m_protocol.speed(speed);
}

void ServerGame::poll()
{
	// TODO: on error, properly discard the message and offending client
	m_protocol.poll(*this);

	// game over check
	if(m_switches.ingame && m_director->over()) {
		assert(m_director);
		assert(m_journal);

		const int winner = m_director->winner();
		m_journal->set_winner(winner);
		m_switches.winner = winner;
		m_protocol.gameend(winner);
	}
}

void ServerGame::meta(GameMeta meta)
{
	// TODO: only allow this if the client is privileged
	try {
		game_reset(meta.players);
	}
	catch(const GameException& ) {
		// TODO: report error back? kick the client for invalid messaging?
	}
}

void ServerGame::input(Input input)
{
	try {
		game_input(input);
	}
	catch(const GameException& ) {
		// TODO: report error back? kick the client for invalid messaging?
	}
}

void ServerGame::speed(int speed)
{
	// TODO: only allow this if the client is privileged
	try {
		set_speed(speed);
	}
	catch(const GameException& ) {
		// TODO: report error back? kick the client for invalid messaging?
	}
}

void ServerGame::start()
{
	// TODO: only allow this if the client is privileged
	try {
		game_start();
	}
	catch(const GameException& ) {
		// TODO: report error back? kick the client for invalid messaging?
	}
}

// --- implementation of module-specific functions ---

namespace
{

[[ maybe_unused ]]
void debug_dump_state(const GameState& state)
{
	if(!std::filesystem::is_directory("dump"))
		return; // creating the replay directory is the user's opt-in

	// get dump file name with hundreths precision
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_sec = std::chrono::system_clock::to_time_t(now);
	std::chrono::milliseconds now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	const struct tm* local_now = std::localtime(&now_sec);

	if(nullptr == local_now)
		return;

	const size_t NOW_BUFSIZE = 20;
	std::string now_buffer(NOW_BUFSIZE, '\0');
	const size_t strftime_size = strftime(&now_buffer[0], NOW_BUFSIZE, "%H-%M-%S", local_now);

	if(0 >= strftime_size)
		return;

	const size_t PATH_BUFSIZE = 40;
	std::string path(PATH_BUFSIZE, '\0');
	const int sprintf_result = sprintf(&path[0], "dump/state_%s.%03d_%d.txt", now_buffer.c_str(), (int)(now_ms.count() % 1000), state.game_time());

	if(sprintf_result < 0)
		return;

	path.resize(sprintf_result); // drop trailing '\0's from sprintf

	// We never overwrite dumps.
	if(std::filesystem::exists(path))
		return;

	std::ofstream stream(path);
	debug_asciiart_state(stream, state);
}

}
