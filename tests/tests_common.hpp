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


// helper function for generating non-random loot for garbage bricks
std::vector<Color> rainbow_loot(size_t count);


// helper function for spawning garbage with generic rainbow loot
Garbage& spawn_garbage(Pit& pit, RowCol rc, int columns, int rows);

/**
 * Swap the blocks at the specified location, regardless of the current cursor position.
 * This is not normally allowed in the game (the cursor does not give random access).
 * @return success of the swapping, just like @c BlockDirector::swap.
 */
bool swap_at(Pit& pit, BlockDirector& director, RowCol rc);

/**
 * Place @c PURPLE and @c ORANGE blocks into rows 1-3 in the pit in a
 * checkerboard pattern. This provides a default floor for tests.
 */
void prefill_pit(Pit& pit);
