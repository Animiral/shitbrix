/**
 * director.hpp
 */

#include "block.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

/**
 * Spawns and removes stuff to and from the stage.
 * The BlockDirector implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are spawning and reaping, block collisions and making blocks fall when they lose support.
 * The BlockDirector does /not/ concern itself with pixel coordinates - it only thinks in block rows and columns.
 */
class BlockDirector
{

public:

	BlockDirector(Stage stage, Pit pit) : stage(stage), pit(pit), bottom(0), rdev(), rndgen(rdev()), m_next_break(0) {}

	void update();

private:

	using BlockVec = std::vector<Block>;

	Stage stage;
	Pit pit;
	BlockVec blocks; // all the blocks under the command of this BlockDirector
	BlockVec previews; // blocks which are fresh spawns and currently inactive
	BlockVec hots; // recently landed or arrived blocks that can start a match
	int bottom; // lowest row that we have already spawned blocks for

	std::random_device rdev;
	std::mt19937 rndgen;
	int m_next_break; // random breakage countdown

	void spawn_block(RowCol rc);
	void spawn_falling(RowCol rc);
	void block_arrive_row(Block block);
	void move_block(Block block, RowCol to);
	BlockVec::iterator reap_block(BlockVec::iterator it);
	void activate_previews();
	void game_over();

};

/**
 * An interface for user input during the game.
 * Applies directions to the cursor.
 */
class CursorDirector
{

public:

	CursorDirector(Pit pit, Cursor cursor) : pit(pit), cursor(cursor) {}

	RowCol rc() const { return cursor->rc; }
	void move(Dir dir);

private:

	Pit pit;
	Cursor cursor;

};
