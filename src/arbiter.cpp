#include "arbiter.hpp"
#include "input.hpp"
#include "replay.hpp"
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

LocalArbiter::LocalArbiter(GameState& state, Journal& journal,
	std::unique_ptr<IColorSupplier> color_supplier)
	: m_state(&state), m_journal(&journal),
	m_color_supplier(std::move(color_supplier))
{
	enforce(nullptr != m_color_supplier);
}

void LocalArbiter::fire(evt::Match match)
{
	int victim = m_state->opponent(match.trivia.player);
	int input_time = match.trivia.game_time + 1; // reaction to event

	if(match.combo >= 3) {
		int counter = match.combo - 3; // number of small blocks to drop
		bool right_side = false;

		while(counter > 0) {
			if(1 == counter) input_garbage(input_time, victim, 3, 1, right_side);
			else if(2 == counter) input_garbage(input_time, victim, 4, 1, right_side);
			else input_garbage(input_time, victim, 5, 1, right_side);

			counter -= 3;
			right_side = !right_side;
		}
	}
}

void LocalArbiter::fire(evt::Chain chain)
{
	int victim = m_state->opponent(chain.trivia.player);
	int input_time = chain.trivia.game_time + 1; // reaction to event

	input_garbage(input_time, victim, PIT_COLS, chain.counter, false);
}

void LocalArbiter::fire(evt::Starve starve)
{
	int victim = m_state->opponent(starve.trivia.player);
	Pit& pit = *m_state->pit().at(victim);
	std::array<Color, PIT_COLS> colors;

	for(auto it = colors.begin(); colors.end() != it; ++it)
		*it = m_color_supplier->next_spawn();

	SpawnBlockInput input{starve.trivia.game_time, victim, pit.bottom() + 1, colors};
	m_journal->add_input(Input{std::move(input)});
}

void LocalArbiter::input_garbage(long game_time, int victim, int columns, int rows, bool right_side)
{
	assert(victim >= 0);
	assert(victim < m_state->pit().size());
	assert(columns > 0);
	assert(columns <= PIT_COLS);
	assert(rows > 0);

	Pit& pit = *m_state->pit().at(victim);
	int spawn_row = std::min(pit.peak(), pit.top()) - rows - 1;
	RowCol rc{spawn_row, 0};

	Loot loot(columns * rows);
	for(Color& c : loot)
		c = m_color_supplier->next_emerge();

	SpawnGarbageInput input { game_time, victim, columns, rows, rc, move(loot) };
	m_journal->add_input(Input{std::move(input)});
}

ServerArbiter::ServerArbiter(ENetServer& server, GameState& state, Journal& journal, std::unique_ptr<IColorSupplier> color_supplier)
{ /* TODO */ }

void ServerArbiter::fire(evt::Match match)
{
/* TODO
	int victim = m_state->opponent(match.trivia.player);
	int input_time = match.trivia.game_time + 1; // reaction to event

	if(match.combo >= 3) {
		int counter = match.combo - 3; // number of small blocks to drop
		bool right_side = false;

		while(counter > 0) {
			if(1 == counter) input_garbage(input_time, victim, 3, 1, right_side);
			else if(2 == counter) input_garbage(input_time, victim, 4, 1, right_side);
			else input_garbage(input_time, victim, 5, 1, right_side);

			counter -= 3;
			right_side = !right_side;
		}
	}
	*/
}

void ServerArbiter::fire(evt::Chain chain)
{
/* TODO
	int victim = m_state->opponent(chain.trivia.player);
	int input_time = chain.trivia.game_time + 1; // reaction to event

	input_garbage(input_time, victim, PIT_COLS, chain.counter, false);
	*/
}

void ServerArbiter::fire(evt::Starve starve)
{
	/* TODO
	int victim = m_state->opponent(starve.trivia.player);
	Pit& pit = *m_state->pit().at(victim);
	std::array<Color, PIT_COLS> colors;

	for(auto it = colors.begin(); colors.end() != it; ++it)
		*it = m_color_supplier->next_spawn();

	SpawnBlockInput input{starve.trivia.game_time, victim, pit.bottom() + 1, colors};
	m_journal->add_input(Input{std::move(input)});
	*/
}

void ServerArbiter::input_garbage(long game_time, int victim, int columns, int rows, bool right_side)
{ /* TODO */ }
