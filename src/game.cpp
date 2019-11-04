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

IGameFactory::IGameFactory() = default;

IGameFactory::~IGameFactory() = default;

std::unique_ptr<GameState> IGameFactory::state()
{
	return move(m_state);
}

std::unique_ptr<Journal> IGameFactory::journal()
{
	return move(m_journal);
}

std::unique_ptr<BlockDirector> IGameFactory::director()
{
	return move(m_director);
}

std::unique_ptr<evt::GameEventHub> IGameFactory::hub()
{
	return move(m_hub);
}

std::unique_ptr<IArbiter> IGameFactory::arbiter()
{
	return move(m_arbiter);
}

void IGameFactory::base_create(GameMeta meta)
{
	m_state = std::make_unique<GameState>(meta);
	m_journal = std::make_unique<Journal>(meta, *m_state);
	m_director = std::make_unique<BlockDirector>();
	m_hub = std::make_unique<evt::GameEventHub>();
	m_director->set_handler(*m_hub);
	m_director->set_state(*m_state);
}

void LocalGameFactory::create(GameMeta meta)
{
	base_create(meta);

	if(meta.replay) {
		m_arbiter.release();
	}
	else {
		auto color_supplier = std::make_unique<RandomColorSupplier>(meta.seed, 0);
		m_arbiter = std::make_unique<LocalArbiter>(*m_state, *m_journal, move(color_supplier));
		m_hub->subscribe(*m_arbiter);
	}
}

void ClientGameFactory::create(GameMeta meta)
{
	base_create(meta);
}

ServerGameFactory::ServerGameFactory(ServerProtocol& protocol)
	: m_protocol(&protocol)
{}

void ServerGameFactory::create(GameMeta meta)
{
	base_create(meta);

	if(meta.replay) {
		m_arbiter.release();
	}
	else {
		auto color_supplier = std::make_unique<RandomColorSupplier>(meta.seed, 0);
		m_arbiter = std::make_unique<ServerArbiter>(*m_protocol, *m_state, *m_journal, move(color_supplier));
		m_hub->subscribe(*m_arbiter);
	}
}

IGame::IGame(std::unique_ptr<IGameFactory> game_factory) noexcept
	: m_game_factory(move(game_factory))
{
	enforce(nullptr != m_game_factory);
}

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

void IGame::before_reset(Handler handler)
{
	m_reset_handler = handler;
}

void IGame::after_start(Handler handler)
{
	m_start_handler = handler;
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

	// if the state is ahead of the target or the new inputs, roll back
	const long time0 = std::min(m_journal->earliest_undiscovered(), target_time + 1);

	if(time0 <= m_state->game_time()) {
		const GameState& checkpoint = m_journal->checkpoint_before(time0);
		before_rollback(target_time, checkpoint.game_time());
		Log::trace("%s(%d): revert to checkpoint before time=%d -> at time=%d.", __FUNCTION__, target_time, time0, checkpoint.game_time());
		*m_state = checkpoint;
		debug_dump_state(*m_state);
	}

	while(m_state->game_time() < target_time && !m_director->over()) {
		InputSpan inputs = m_journal->get_inputs(m_state->game_time() + 1);
		for(auto it = inputs.first; it != inputs.second; ++it) {
			Log::trace("%s(%d): apply input %s.", __FUNCTION__, target_time, std::string(*it).c_str());
			m_director->apply_input(*it);
		}

		// Run self-contained object behaviors.
		// state.game_time() is incremented here.
		m_state->update();

		// Run updates based on game logic and interactions.
		// This may invalidate the above iterators in inputs due to new inputs.
		m_director->update();
	}

	m_journal->discover_inputs(target_time + 1);

	if(m_director->over())
		return; // stop feeding the journal now

	// save new checkpoint?
	if(target_time >= m_journal->checkpoint_before(target_time).game_time() + CHECKPOINT_INTERVAL) {
		Log::trace("%s(%d): save checkpoint at time=%d.", __FUNCTION__, target_time, m_state->game_time());
		m_journal->add_checkpoint(GameState(*m_state));
		debug_dump_state(*m_state);
	}
}

void IGame::load_replay(std::filesystem::path path)
{
	if(!std::filesystem::is_regular_file(path)) {
		throwx<GameException>("Replay not found: %s", path.u8string());
	}

	std::ifstream stream{ path };
	Journal journal = replay_read(stream);
	GameMeta meta = journal.meta();

	// If we want to play back a replay, feed it all to the game
	// and let the normal timing in the game loop take care of it.
	//
	// What actually happens at reset, start and input depends on the concrete
	// game implementation.
	game_reset(meta.players, meta.rules, true);
	game_start();

	for(Input input : journal.inputs()) {
		game_input(input);
	}
}

void IGame::base_start()
{
	enforce(m_switches.ready);
	enforce(!m_switches.ingame);
	assert(m_meta.has_value());

	m_switches.ingame = true;
	m_switches.winner = NOONE;

	m_game_factory->create(*m_meta);

	m_state = m_game_factory->state();
	m_journal = m_game_factory->journal();
	m_director = m_game_factory->director();
	m_hub = m_game_factory->hub();

	// validate factory output
	enforce(nullptr != m_state);
	enforce(nullptr != m_journal);
	enforce(nullptr != m_director);
	enforce(nullptr != m_hub);

	if(m_start_handler)
		m_start_handler();
}

void IGame::base_reset()
{
	if(m_reset_handler)
		m_reset_handler();

	m_switches.ingame = false;
	m_switches.ready = true;

	m_state.reset();
	m_journal.reset();
	m_director.reset();
	m_hub.reset();
}

LocalGame::LocalGame(std::unique_ptr<IGameFactory> game_factory)
	: IGame(move(game_factory))
{}

LocalGame::~LocalGame() noexcept = default;

void LocalGame::game_start()
{
	assert(m_meta.has_value());

	Log::info("Initialize new local game for %d players, seed=%u.", m_meta->players, m_meta->seed);

	base_start();
	m_arbiter = m_game_factory->arbiter();
}

void LocalGame::game_input(Input input)
{
	enforce(m_switches.ingame);
	assert(m_journal);

	m_journal->add_input(std::move(input));
}

void LocalGame::game_reset(const int players, const Rules rules, const bool replay)
{
	assert(2 == players); // different player numbers are not yet supported

	base_reset();
	m_arbiter.reset();

	static std::random_device rdev;
	m_meta = GameMeta{players, replay ? 0 : rdev(), replay, rules, NOONE};
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

void LocalGame::before_rollback(long target_time, long checkpoint_time)
{
	assert(!"Rollback should never happen in local game.");
}

ClientGame::ClientGame(
	std::unique_ptr<IGameFactory> game_factory,
	std::unique_ptr<ClientProtocol> protocol) noexcept
	:
	IGame(move(game_factory)),
	m_protocol(move(protocol))
{
	enforce(nullptr != m_protocol);
}

void ClientGame::game_start()
{
	m_protocol->start();
}

void ClientGame::game_input(Input input)
{
	m_protocol->input(input);
}

void ClientGame::game_reset(const int players, const Rules rules, const bool replay)
{
	const GameMeta meta{ players, 0, replay, rules };
	m_protocol->meta(meta);
}

void ClientGame::set_speed(int speed)
{
	m_protocol->speed(speed);
}

void ClientGame::poll()
{
	m_protocol->poll(*this);
}

void ClientGame::meta(GameMeta meta)
{
	assert(2 == meta.players); // different player numbers are not yet supported

	base_reset();

	m_meta = meta;
}

void ClientGame::input(Input input)
{
	assert(m_journal);

	if(!m_switches.ingame)
		throwx<GameException>("Got input from server before the game is running: %s.", std::string(input).c_str());

	m_journal->add_input(std::move(input));
}

void ClientGame::retract(long cutoff_time)
{
	m_journal->retract(cutoff_time);
}

void ClientGame::speed(int speed)
{
	m_switches.speed = speed;
}

void ClientGame::start()
{
	if(!m_switches.ready)
		throwx<GameException>("Got start from server before the game is ready.");

	if(m_switches.ingame)
		throwx<GameException>("Got start from server while the game is ongoing.");

	assert(m_meta.has_value());

	Log::info("Initialize new client game for %d players, seed=%u.", m_meta->players, m_meta->seed);

	base_start();
}

void ClientGame::gameend(int winner)
{
	assert(m_journal);

	if(!m_switches.ingame)
		throwx<GameException>("Got gameend from server while the game is not running.");

	m_journal->set_winner(winner);
	m_switches.winner = winner;
}

ServerGame::ServerGame(std::unique_ptr<IGameFactory> game_factory, std::unique_ptr<ServerProtocol> protocol) noexcept
	: IGame(move(game_factory)), m_protocol(move(protocol))
{
	enforce(nullptr != m_protocol);
}

ServerGame::~ServerGame() noexcept = default;

void ServerGame::game_start()
{
	if(!m_switches.ready)
		throwx<GameException>("Cannot start game before it is ready.");

	if(m_switches.ingame)
		throwx<GameException>("Cannot start game while the game is ongoing.");

	assert(m_meta.has_value());

	Log::info("Initialize new server game for %d players, seed=%u.", m_meta->players, m_meta->seed);
	
	base_start();
	m_arbiter = m_game_factory->arbiter();

	m_protocol->start();
}

void ServerGame::game_input(Input input)
{
	if(!m_switches.ingame)
		throwx<GameException>("Cannot handle input before the game is running: %s.", std::string(input).c_str());

	assert(m_journal);
	m_journal->add_input(input);

	m_protocol->input(input);
}

void ServerGame::game_reset(const int players, const Rules rules, const bool replay)
{
	base_reset();

	if(2 != players)
		throwx<GameException>("%d players are currently not supported.", players);

	static std::random_device rdev;
	m_meta = GameMeta{players, replay ? 0 : rdev(), replay, rules, NOONE};
	m_protocol->meta(*m_meta);
}

void ServerGame::set_speed(int speed)
{
	m_switches.speed = speed;
	m_protocol->speed(speed);
}

void ServerGame::poll()
{
	// TODO: on error, properly discard the message and offending client
	m_protocol->poll(*this);

	// game over check
	if(m_switches.ingame && m_director->over()) {
		assert(m_director);
		assert(m_journal);

		const int winner = m_director->winner();
		m_journal->set_winner(winner);
		m_switches.winner = winner;
		m_protocol->gameend(winner);
	}
}

void ServerGame::before_rollback(long target_time, long checkpoint_time)
{
	m_protocol->retract(checkpoint_time);
	m_journal->retract(checkpoint_time);
}

void ServerGame::meta(GameMeta meta)
{
	// TODO: only allow this if the client is privileged
	try {
		game_reset(meta.players, meta.rules, meta.replay);
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
