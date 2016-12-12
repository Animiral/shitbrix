/**
 * director.hpp
 */

#include "stage.hpp"
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

private:

	/**
	 * Helper struct to enable a std::set of blocks
	 */
	struct BlockLess
	{
		bool operator()(const Block& lhs, const Block& rhs) const { return lhs.rc() < rhs.rc(); }
	};

public:

	using BlockSet = std::set<std::reference_wrapper<Block>, BlockLess>;

	MatchBuilder(const Pit& pit) : pit(pit) {}

	void ignite(Block& block);
	const BlockSet& result() { return m_result; }
	int combo() { return m_result.size(); }

private:

	const Pit& pit;
	BlockSet m_result;

	bool match_at(RowCol rc, Block::Color color);
	void insert(RowCol rc);

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

	BlockDirector(Pit& pit, RndGen rndgen) : pit(pit), bottom(0), m_over(false), rndgen(rndgen) {}

	bool over() const { return m_over; }
	void update(IContext& context);
	bool swap(RowCol lrc);
	void debug_spawn_garbage(int columns, int rows); // spawn some stuff to demo garbage

private:

	using BlockRefVec = std::vector<std::reference_wrapper<Block>>;
	using BlockIt = decltype(std::declval<Pit>().blocks_begin());

	Pit& pit;
	BlockRefVec previews; // blocks which are fresh spawns and currently inactive
	BlockRefVec fallers; // blocks which we want to start falling soon
	BlockRefVec hots; // recently landed or arrived blocks that can start a match
	int bottom; // lowest row that we have already spawned blocks for
	bool m_over; // whether the game is over (the player with this Director loses)

	RndGen rndgen;     // block colors are generated randomly

	void spawn_previews();
	Block& spawn_block(RowCol rc);
	Block& spawn_fake(RowCol rc);

	void block_arrive_fall(Block& block);
	void garbage_arrive_fall(GarbagePtr garbage);
	void block_arrive_swap(Block& block);

	/**
	 * Make blocks above the recently-freed location fall down.
	 */
	void trigger_falls(RowCol free);

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

	CursorDirector(Pit& pit, Cursor& cursor) : pit(pit), m_cursor(cursor) {}

	Cursor& cursor() const { return m_cursor; }
	RowCol rc() const { return m_cursor.rc; }

	/**
	 * Moves the cursor one column in the specified direction, if possible.
	 * Dir::NONE is a special direction that prevents the cursor from
	 * scrolling out of bounds.
	 */
	void move(Dir dir);

private:

	Pit& pit;
	Cursor& m_cursor;

};
