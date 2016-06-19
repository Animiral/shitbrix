/**
 * director.hpp
 */

#include "block.hpp"
#include "context.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

/**
 * Spawns and removes stuff to and from the stage.
 * The Director implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are spawning and reaping, block collisions and making blocks fall when they lose support.
 * The Director does /not/ concern itself with pixel coordinates - it only thinks in block rows and columns.
 */
class Director
{

public:

	Director(SharedStage stage, SharedPit pit) : stage(stage), pit(pit), rdev(), rndgen(rdev()), bottom(0) {}

	void update();

private:

	using BlockVec = std::vector<SharedBlock>;

	SharedStage stage;
	SharedPit pit;
	BlockVec blocks;
	std::random_device rdev;
	std::mt19937 rndgen;
	int bottom; // lowest row that we have already spawned blocks for

	void spawn_block(RowCol rc);
	void spawn_falling(RowCol rc);
	void block_arrive_row(SharedBlock block);
	BlockVec::iterator reap_block(BlockVec::iterator it);
	void game_over();

};

using SharedDirector = std::shared_ptr<Director>;
