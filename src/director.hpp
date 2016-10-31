/**
 * director.hpp
 */

#include "block.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>
#include <set>

using RndGen = std::shared_ptr<std::mt19937>;

/**
 * Examines the pit for matching blocks from a sequence of “hot” blocks
 * which have just been moved or landed. They are passed to the MatchBuilder via ignite(). 
 * Returns all detected matching blocks (3 or more in a row from a hot block) in result().
 * The combo() specifies the number of blocks resolved at the same time.
 */
class MatchBuilder
{

public:

	MatchBuilder(Pit pit) : pit(pit) {}

	void ignite(Block block);
	const std::set<Block>& result() { return m_result; }
	int combo() { return m_result.size(); }

private:

	Pit pit;
	std::set<Block> m_result;

	// bool add_block(RowCol rc);
	bool match_at(RowCol rc, BlockCol color);

};

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

	BlockDirector(Stage stage, Pit pit, RndGen rndgen) : stage(stage), pit(pit), bottom(0), m_over(false), rndgen(rndgen) {}

	bool over() const { return m_over; }
	void update(IContext& context);
	bool swap(RowCol lrc);

private:

	Stage stage;
	Pit pit;
	BlockVec previews; // blocks which are fresh spawns and currently inactive
	BlockVec hots; // recently landed or arrived blocks that can start a match
	int bottom; // lowest row that we have already spawned blocks for
	bool m_over; // whether the game is over (the player with this Director loses)

	RndGen rndgen;     // block colors are generated randomly

	void spawn_previews();
	Block spawn_block(RowCol rc);
	Block spawn_fake(RowCol rc);

	void block_arrive_fall(Block block);
	void block_arrive_swap(Block block);

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

	CursorDirector(Pit pit, Cursor cursor) : pit(pit), m_cursor(cursor) {}

	Cursor cursor() const { return m_cursor; }
	RowCol rc() const { return m_cursor->rc; }

	/**
	 * Moves the cursor one column in the specified direction, if possible.
	 * Dir::NONE is a special direction that prevents the cursor from
	 * scrolling out of bounds.
	 */
	void move(Dir dir);

private:

	Pit pit;
	Cursor m_cursor;

};
