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

	Director(SharedStage stage, SharedPit pit) : stage(stage), pit(pit), bottom(0), rdev(), rndgen(rdev()) {}

	void update();

private:

	using BlockVec = std::vector<SharedBlock>;

	SharedStage stage;
	SharedPit pit;
	BlockVec blocks; // all the blocks under the command of this Director
	BlockVec previews; // blocks which are fresh spawns and currently inactive
	BlockVec hots; // recently landed or arrived blocks that can start a match
	int bottom; // lowest row that we have already spawned blocks for

	std::random_device rdev;
	std::mt19937 rndgen;

	void spawn_block(RowCol rc);
	void spawn_falling(RowCol rc);
	void block_arrive_row(SharedBlock block);
	BlockVec::iterator reap_block(BlockVec::iterator it);
	void activate_previews();
	void game_over();

};

using SharedDirector = std::shared_ptr<Director>;
