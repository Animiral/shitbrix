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
	 * Helper struct to enable a std::set of blocks and garbage
	 */
	struct PhysicalLess
	{
		bool operator()(const Physical& lhs, const Physical& rhs) const { return lhs.rc() < rhs.rc(); }
	};

public:

	using BlockSet = std::set<std::reference_wrapper<Block>, PhysicalLess>;
	using GarbageSet = std::set<std::reference_wrapper<Garbage>, PhysicalLess>;

	MatchBuilder(const Pit& pit) : pit(pit) {}

	void ignite(Block& block);
	const BlockSet& result() { return m_result; }

	/**
	 * For each block in the result set of the MatchBuilder,
	 * examine the adjacent locations for Garbage blocks.
	 * The set of garbage bricks found in this manner is marked
	 * and available via @ref touched_garbage.
	 */
	void find_touch_garbage();
	const GarbageSet& touched_garbage() const noexcept { return m_touched_garbage; }

	int combo() { return m_result.size(); }

private:

	const Pit& pit;
	BlockSet m_result;
	GarbageSet m_touched_garbage;

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

	using PhysicalRefVec = std::vector<std::reference_wrapper<Physical>>;
	using BlockRefVec = std::vector<std::reference_wrapper<Block>>;
	using GarbageRefVec = std::vector<std::reference_wrapper<Garbage>>;

	Pit& pit;
	BlockRefVec previews; // blocks which are fresh spawns and currently inactive
	PhysicalRefVec fallers; // objects which we want to start falling soon
	GarbageRefVec dissolvers; // blocks which are shrinking or dying
	BlockRefVec hots; // recently landed or arrived blocks that can start a match
	int bottom; // lowest row that we have already spawned blocks for
	bool m_over; // whether the game is over (the player with this Director loses)

	RndGen rndgen;     // block colors are generated randomly

	void spawn_previews();

	/**
	 * Put a block of random color at the specified location with the state.
	 */
	Block& spawn_random_block(RowCol rc, Block::State state);

	/**
	 * Fake blocks are used to replace empty spaces for the duration of a swap().
	 */
	Block& spawn_fake_block(RowCol rc);

	/**
	 * Split the garbage block into matchable blocks and the
	 * smaller remains of the garbage brick.
	 * All objects involved may fall down immediately.
	 */
	void dissolve_garbage(Garbage& garbage);

	/**
	 * Examine the pit contents for stalling elements, such as blocks in the
	 * process of breaking up. If no such elements are found, re-enable
	 * time-based automatic scrolling of the pit.
	 */
	void try_resume_pitscroll();

	/**
	 * Return true if the physical has space to fall down.
	 * In contrast to @ref Pit::can_fall, which determines whether the object
	 * can move into the space below immediately, this function tests whether
	 * it is fit to be placed in the set of “fallers”, to begin falling soon.
	 */
	bool can_fall(Physical& physical);

	void arrive_fall(Physical& physical);
	void block_arrive_swap(Block& block);

	/**
	 * Make blocks above the recently-freed location fall down.
	 */
	void trigger_falls(RowCol free);
	void trigger_falls_impl(RowCol rc);

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
