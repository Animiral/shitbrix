#include "arbiter.hpp"
#include "input.hpp"
#include "replay.hpp"
#include "network.hpp"
#include "error.hpp"
#include <cassert>


IColorSupplier::~IColorSupplier() = default;


RandomColorSupplier::RandomColorSupplier(unsigned seed, int player)
	: /* m_record(), */ m_generator(seed * (player + 1))
{
}

Color RandomColorSupplier::next_spawn() noexcept
{
	// For the moment, this implementation simply generates random colors without
	// any interference. In the future, it must be built not to generate blocks
	// such that they already form a match when they arrive in the pit.

	static std::uniform_int_distribution<int> color_distribution { 1, 6 };
	Color color = static_cast<Color>(color_distribution(m_generator));
	//m_record.push_back(color); // required later
	return color;
}

Color RandomColorSupplier::next_emerge() noexcept
{
	return next_spawn();
}

IArbiter::~IArbiter() noexcept = default;

namespace
{
// Arbiter game logic/decision functions.
// Every arbiter uses the same method of determining things like garbage spawn placement.

/**
 * Return the appropriate @c SpawnGarbageInputs that follow a successful block match.
 */
std::vector<Input> inputs_from_match(evt::Match match, const GameState& state, IColorSupplier& color_supplier);

/**
 * Return the appropriate @c SpawnGarbageInput that follows a successful chain match.
 */
std::vector<Input> input_from_chain(evt::Chain chain, const GameState& state, IColorSupplier& color_supplier);

/**
 * Return the appropriate @c SpawnBlockInput for a pit in need of a refill.
 */
Input input_from_starve(evt::Starve starve, const GameState& state, IColorSupplier& color_supplier);

/**
 * Return the appropriate @c SpawnGarbageInputs that follow a successful block match.
 */
Input input_garbage(long game_time, int victim, int columns, int rows, bool right_side,
                    const GameState& state, IColorSupplier& color_supplier);

}

LocalArbiter::LocalArbiter(GameState& state, Journal& journal,
	std::unique_ptr<IColorSupplier> color_supplier)
	: m_state(&state), m_journal(&journal),
	m_color_supplier(std::move(color_supplier))
{
	enforce(nullptr != m_color_supplier);
}

void LocalArbiter::fire(evt::Match match)
{
	for(Input input : inputs_from_match(match, *m_state, *m_color_supplier)) {
		m_journal->add_input(std::move(input));
	}
}

void LocalArbiter::fire(evt::Chain chain)
{
	for(Input input : input_from_chain(chain, *m_state, *m_color_supplier)) {
		m_journal->add_input(std::move(input));
	}
}

void LocalArbiter::fire(evt::Starve starve)
{
	Input input = input_from_starve(starve, *m_state, *m_color_supplier);
	m_journal->add_input(Input{std::move(input)});
}

ServerArbiter::ServerArbiter(ServerProtocol& server_protocol, GameState& state, Journal& journal, std::unique_ptr<IColorSupplier> color_supplier)
	: m_server_protocol(&server_protocol), m_state(&state), m_journal(&journal), m_color_supplier(move(color_supplier))
{
	enforce(nullptr != m_color_supplier);
}

void ServerArbiter::fire(evt::Match match)
{
	for(Input input : inputs_from_match(match, *m_state, *m_color_supplier)) {
		m_journal->add_input(input);
		m_server_protocol->input(input);
	}
}

void ServerArbiter::fire(evt::Chain chain)
{
	for(Input input : input_from_chain(chain, *m_state, *m_color_supplier)) {
		m_journal->add_input(input);
		m_server_protocol->input(input);
	}
}

void ServerArbiter::fire(evt::Starve starve)
{
	Input input = input_from_starve(starve, *m_state, *m_color_supplier);
	m_journal->add_input(input);
	m_server_protocol->input(input);
}


namespace
{

std::vector<Input> inputs_from_match(evt::Match match, const GameState& state, IColorSupplier& color_supplier)
{
	int victim = state.opponent(match.trivia.player);
	int input_time = match.trivia.game_time + 1; // reaction to event

	std::vector<Input> inputs;

	if(match.combo >= 3) {
		int counter = match.combo - 3; // number of small blocks to drop
		bool right_side = false;

		while(counter > 0) {
			if(1 == counter)
				inputs.push_back(input_garbage(input_time, victim, 3, 1, right_side, state, color_supplier));
			else if(2 == counter)
				inputs.push_back(input_garbage(input_time, victim, 4, 1, right_side, state, color_supplier));
			else
				inputs.push_back(input_garbage(input_time, victim, 5, 1, right_side, state, color_supplier));

			counter -= 3;
			right_side = !right_side;
		}
	}

	return inputs;
}

std::vector<Input> input_from_chain(evt::Chain chain, const GameState& state, IColorSupplier& color_supplier)
{
	if(chain.counter <= 0)
		return {}; // no chain - no garbage

	int victim = state.opponent(chain.trivia.player);
	int input_time = chain.trivia.game_time + 1; // reaction to event

	// Even though the interface allows us to throw any number of garbage bricks,
	// the current gameplay rules prescribe just one, no matter how big.
	return {input_garbage(input_time, victim, PIT_COLS, chain.counter, false, state, color_supplier)};
}

Input input_from_starve(evt::Starve starve, const GameState& state, IColorSupplier& color_supplier)
{
	std::array<Color, PIT_COLS> colors;

	for(auto it = colors.begin(); colors.end() != it; ++it)
		*it = color_supplier.next_spawn();
	
	Pit& pit = *state.pit().at(starve.trivia.player);
	return Input{SpawnBlockInput{starve.trivia.game_time, starve.trivia.player, pit.bottom() + 1, colors}};
}

Input input_garbage(long game_time, int victim, int columns, int rows, bool right_side,
					const GameState& state, IColorSupplier& color_supplier)
{
	assert(victim >= 0);
	assert(victim < state.pit().size());
	assert(columns > 0);
	assert(columns <= PIT_COLS);
	assert(rows > 0);

	Pit& pit = *state.pit().at(victim);
	int spawn_row = std::min(pit.peak(), pit.top()) - rows - 1;
	RowCol rc{spawn_row, 0};

	Loot loot(columns * rows);
	for(Color& c : loot)
		c = color_supplier.next_emerge();

	return Input{SpawnGarbageInput{game_time, victim, rows, columns, rc, move(loot)}};
}

}
