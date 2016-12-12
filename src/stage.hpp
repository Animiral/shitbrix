/**
 * stage.hpp
 * This module contains all the basic classes of objects that we see
 * exclusively during gameplay (on the GameScreen).
 * These are objects such as Block, Pit, Cursor and Stage itself.
 */
#pragma once

#include "globals.hpp"
#include "context.hpp"
#include <memory>
#include <vector>
#include <map>
#include <random>

enum class BlockCol { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE, FAKE };
enum class BlockState { INVALID, PREVIEW, REST, SWAP, FALL, LAND, BREAK, DEAD };

// Allow operator- on BlockCol
int operator-(BlockCol lhs, BlockCol rhs);

/**
 * Single block, comes in 6 colors
 *
 * # Block states
 *  * INVALID: no block should ever have this non-state
 *  * PREVIEW: init state. (Partially) visible, but not yet subject to matches and swapping
 *  * REST: the block is inactive and stationary. Only resting blocks can match.
 *  * FALL: on its way down the pit at FALL_SPEED
 *  * LAND: for a short period of time, after its fall stops, the block holds out on matches & can be swapped
 *  * BREAK: the block has been matched and is in the process of destruction
 *  * DEAD: should be removed from the game asap as it is an error to logic update() a dead block
 */
class Block : public ILogic
{

public:

	// Public properties - can be read/changed/corrected at will
	BlockCol col;    // color
	Point offset;    // x/y offset from draw center of r/c location
	int time;        // number of ticks until we consider a state switch

	Block(BlockCol col, RowCol rc, BlockState state)
	:
	col(col), offset{0,0}, time(0),
	m_loc(from_rc(rc)), m_rc(rc), m_state(state)
	{}

	virtual void update(IContext& context) override;

	Point loc() const { return m_loc; }
	RowCol rc() const { return m_rc; }
	void set_rc(RowCol rc);
	BlockState state() const { return m_state; }
	void set_state(BlockState state);
	void swap_toward(RowCol target);

	bool is_arriving() const;
	bool is_fallible() const;
	bool is_swappable() const;
	bool is_matchable() const;

	static constexpr int SWAP_TIME = 6; // number of ticks to swap two blocks
	static constexpr int LAND_TIME = 20; // number of ticks in a block’s landing animation
	static constexpr int BREAK_TIME = 30; // number of ticks for a block to break

private:

	Point m_loc;        // logical location, upper left corner relative to view (not necessarily sprite draw location)
	RowCol m_rc;        // row/col position, - is UP, + is DOWN
	Point m_target;     // target location - where the block really wants to be while it’s busy with an animation like SWAP
	BlockState m_state; // current block state
	BlockFrame m_anim;  // current animation frame

	void swap();
	void fall();
	void land();
	void dobreak();
	
};

bool y_greater(const Block& lhs, const Block& rhs);

/**
 * Garbage block.
 * This block is a bit like the common blocks in that it occupies some space
 * in the pit. Garbage blocks span multiple spaces. They never spawn from the
 * bottom, always falling from above.
 */
class Garbage : public ILogic
{

public:

	enum class State { REST, FALL, LAND, DISSOLVE, DEAD };

	// Public properties - can be read/changed/corrected at will
	Point offset;    // x/y offset from draw center of r/c location
	int time;        // number of ticks until we consider a state switch

	Garbage(RowCol rc, int columns, int rows)
	:
	offset{0,0}, time(0),
	m_loc(from_rc(RowCol{rc.r + rows - 1, rc.c})), m_rc(rc),
	m_columns(columns), m_rows(rows), m_state(State::FALL)
	{}

	virtual void update(IContext& context) override;

	Point loc() const { return m_loc; }
	RowCol rc() const { return m_rc; }
	int rows() const { return m_rows; }
	int columns() const { return m_columns; }
	int shrink() { return --m_rows; }
	void set_rc(RowCol rc);
	State state() const { return m_state; }
	void set_state(State state);

	bool is_arriving();

	static constexpr int LAND_TIME = 20; // number of ticks in a garbage’s landing animation
	static constexpr int DISSOLVE_TIME = 30; // number of ticks for a garbage block to break

private:

	Point m_loc;        // logical location, upper left corner relative to view (not necessarily sprite draw location)
	RowCol m_rc;        // upper left row/col position, - is UP, + is DOWN
	int m_columns;      // width of this garbage in blocks
	int m_rows;         // height of this garbage in blocks
	Point m_target;     // target location - where the garbage really wants to be while it’s busy with an animation
	State m_state;      // current garbage state

	void fall();
	void land();
	void dobreak();

};

using GarbagePtr = std::shared_ptr<Garbage>;

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit owns and updates its contained blocks and garbage.
 * It remembers where blocks are in a sparse matrix.
 * It also handles scrolling.
 */
class Pit : public ILogic
{

public:

	Pit(Point loc);

	Point loc() const { return m_loc; }

	auto blocks_begin() { return m_blocks.begin(); }
	auto blocks_end() { return m_blocks.end(); }
	auto garbage_begin() { return m_garbage.begin(); }
	auto garbage_end() { return m_garbage.end(); }
	auto blocks_begin() const { return m_blocks.begin(); }
	auto blocks_end() const { return m_blocks.end(); }
	auto garbage_begin() const { return m_garbage.begin(); }
	auto garbage_end() const { return m_garbage.end(); }

	Block* block_at(RowCol rc) const;
	Garbage* garbage_at(RowCol rc) const;
	bool anything_at(RowCol rc) const;

	/**
	 * Create a new Block with the specified properties in the Pit.
	 * Caution! This may invalidate all existing references to Blocks in the Pit.
	 *
	 * @return a reference to the created Block
	 */
	Block& spawn_block(BlockCol color, RowCol rc, BlockState state);

	/**
	 * Create a new Garbage with the specified dimensions.
	 * Caution! This may invalidate all existing references to Garbage in the Pit.
	 *
	 * @return a reference to the created Garbage
	 */
	Garbage& spawn_garbage(RowCol rc, int width, int height);

	/**
	 * Return true if it is acceptable to move the object located at the
	 * from-location one row down, based on spaces blocked.
	 */
	bool can_fall(RowCol from) const;

	/**
	 * Move the object located at the at-location one row down.
	 * For larger objects (Garbage), only the at-location that corresponds
	 * to the object’s own primary rc (lower left tile for Garbage), not
	 * any location blocked by the object, is accepted.
	 */
	void fall(RowCol from);

	/**
	 * Swap the locations of the Blocks at the specified coordinates.
	 */
	void swap(RowCol lrc, RowCol rrc);

	/**
	 * Remove dead Blocks from the Pit to clean it up.
	 * Caution! This may invalidate all existing references to Blocks in the Pit.
	 */
	void remove_dead();

	/**
	 * Reduce the size of the referenced Garbage by one row from the bottom.
	 * If that one row was the entire size of the Garbage, it is removed
	 * completely.
	 * Caution! This may invalidate all existing references to Garbage in the Pit.
	 *
	 * @return a pointer to the reduced Garbage, or nullptr if the Garbage is gone
	 */
	Garbage* shrink(Garbage& garbage);

	/**
	 * Remove all objects from the Pit.
	 */
	void clear();

	int top() const;
	int bottom() const;
	int peak() const;
	int highlight_row() const { return m_highlight_row; }

	void stop() { m_enabled = false; }
	void start() { m_enabled = true; }

	/**
	 * Put a debug highlight on a row
	 */
	void highlight(int row);

	Point transform(Point point, float dt=0.f) const;
	virtual void update(IContext& context) override;

private:

	using BlockVec = std::vector<std::unique_ptr<Block>>;

	Point m_loc;     // draw location, upper left corner
	bool m_enabled;  // whether or not to scroll the pit on update()
	float m_scroll;  // y-offset for view on pit contents
	int m_peak;      // highest blocked row (may be above visible space)
	BlockVec m_blocks; // list of all blocks in the pit
	std::vector<GarbagePtr> m_garbage; // list of all garbage in the pit
	std::map<RowCol, Block*> block_map; // sparse matrix of blocked spaces
	std::map<RowCol, GarbagePtr> m_garbage_map; // sparse matrix of blocked spaces

	int m_highlight_row;

	void refresh_peak(); //!< Search for the new m_peak
	void fall_block(Block& block); //!< Move the Block to the to-location.
	void fall_garbage(Garbage& garbage); //!< Move the Garbage to the to-location.
	void block_garbage(Garbage& garbage); //!< Mark the garbage area as occupied.
	void unblock_garbage(const Garbage& garbage); //!< Mark the garbage area as free.

};

class Cursor
{

public:

	RowCol rc;
	int time;

	Cursor(RowCol rc) : rc(rc), time(0) {}

	void update() { time++; }

};

enum class BannerFrame : size_t { WIN=0, LOSE=1 };

class Banner
{

public:

	Banner(Point loc, BannerFrame frame) : loc(loc), frame(frame) {}

	Point loc;
	BannerFrame frame;

};

/**
 * Stage is a container for on-screen objects.
 * The Stage owns all its contained objects.
 */
class Stage
{

public:

	Stage() {}
	Stage(const Stage& ) =delete;

	//! Helper struct for stage contents
	struct PitCursor
	{
		PitCursor(Point loc);

		Pit pit;
		Cursor cursor;
	};

	using PitsVector = std::vector<std::unique_ptr<PitCursor>>;

	/**
	 * Add a pit to the stage to be displayed at the given point coordinates.
	 */
	PitCursor& add_pit(Point loc);
	void update(IContext& context);

	PitsVector& pits() { return m_pits; }
	const PitsVector& pits() const { return m_pits; }

private:

	PitsVector m_pits;

};

class StageBuilder
{

public:

	Pit* left_pit;
	Pit* right_pit;
	Cursor* left_cursor;
	Cursor* right_cursor;

	std::unique_ptr<Stage> construct();

};
