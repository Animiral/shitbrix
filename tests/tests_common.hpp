/**
 * tests_common.hpp
 * Definitions for shared helpers for unit tests.
 */
#pragma once

#include "stage.hpp"
#include "director.hpp"
#include "network.hpp"

/**
 * Set the global context to use stub implementations for our test environment.
 */
void configure_context_for_testing();

/**
 * Create a game context for testing game scenarios.
 */
GameData make_gamedata_for_testing();

/**
 * A block color supplier testing that simply rotates through colors.
 */
class RainbowColorSupplier : public IColorSupplier
{

public:

	virtual Block::Color next_spawn() noexcept override;
	virtual Block::Color next_emerge() noexcept override { return next_spawn(); }
	virtual std::unique_ptr<IColorSupplier> clone() const override { return std::make_unique<RainbowColorSupplier>(*this); }

private:

	Block::Color m_color;

};

/**
 * Swap the blocks at the specified location, regardless of the current cursor position.
 * This is not normally allowed in the game (the cursor does not give random access).
 * @return success of the swapping, just like @c BlockDirector::swap.
 */
bool swap_at(Pit& pit, BlockDirector& director, RowCol rc);
