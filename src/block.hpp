/**
 * block.hpp
 * In-game objects such as Block, Pit, Stage, StageBuilder
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
class BlockImpl : public ILogic
{

public:

	// Public properties - can be read/changed/corrected at will
	BlockCol col;    // color
	Point offset;    // x/y offset from draw center of r/c location
	int time;        // number of ticks until we consider a state switch

	BlockImpl(BlockCol col, RowCol rc)
	:
	col(col), offset{0,0}, time(0),
	m_loc(from_rc(rc)), m_rc(rc), m_state(BlockState::PREVIEW)
	{}

	virtual void update(IContext& context) override;

	Point loc() const { return m_loc; }
	RowCol rc() const { return m_rc; }
	void set_rc(RowCol rc);
	BlockState state() const { return m_state; }
	void set_state(BlockState state);
	void swap_toward(RowCol target);

	bool is_arriving();

	static constexpr int SWAP_TIME = 6; // number of ticks to swap two blocks
	static constexpr int LAND_TIME = 20; // number of ticks in a block’s landing animation
	static constexpr int BREAK_TIME = 30; // number of ticks for a block to break

private:

	Point m_loc;        // logical location, upper left corner relative to view (not necessarily sprite draw location)
	RowCol m_rc;        // row/col position, - is UP, + is DOWN
	Point m_target;     // target location - where the block really wants to be while it’s busy with an animation like SWAP
	BlockState m_state; // current block state. On state time out, tell an IStateSubscriber (previously saved via BlockImpl::subscribe()) with notify()
	BlockFrame m_anim;  // current animation frame

	void swap();
	void fall();
	void land();
	void dobreak();
	
};

using Block = std::shared_ptr<BlockImpl>;
using BlockVec = std::vector<Block>;

bool y_greater(const Block& lhs, const Block& rhs);
bool fallible(Block block);
bool swappable(Block block);
bool matchable(Block block);

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
	void set_rc(RowCol rc);
	State state() const { return m_state; }
	void set_state(State state);

	bool is_arriving();

	static constexpr int LAND_TIME = 20; // number of ticks in a garbage’s landing animation
	static constexpr int DISSOLVE_TIME = 30; // number of ticks for a garbage block to break

private:

	Point m_loc;        // logical location, upper left corner relative to view (not necessarily sprite draw location)
	RowCol m_rc;        // lower left row/col position, - is UP, + is DOWN
	int m_columns;      // width of this garbage in blocks
	int m_rows;         // height of this garbage in blocks
	Point m_target;     // target location - where the garbage really wants to be while it’s busy with an animation
	State m_state; // current block state. On state time out, tell an IStateSubscriber (previously saved via BlockImpl::subscribe()) with notify()

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
class PitImpl : public ILogic
{

public:

	PitImpl(Point loc);
	Point loc() const { return m_loc; }
	BlockVec& blocks() { return m_blocks; }
	std::vector<GarbagePtr>& garbage() { return m_garbage; }
	const BlockVec& blocks() const { return m_blocks; }
	const std::vector<GarbagePtr>& garbage() const { return m_garbage; }
	int top() const;
	int bottom() const;
	int peak() const;
	Block block_at(RowCol rc) const;
	GarbagePtr garbage_at(RowCol rc) const;
	bool anything_at(RowCol rc) const;
	int highlight_row() const { return m_highlight_row; }

	GarbagePtr spawn_garbage(int columns, int rows);
	void block(RowCol rc, Block block);
	void block(GarbagePtr garbage);
	void unblock(RowCol rc);
	void unblock(GarbagePtr garbage);
	void swap(RowCol lrc, RowCol rrc);
	void stop() { m_enabled = false; }
	void start() { m_enabled = true; }

	/**
	 * Put a debug highlight on a row
	 */
	void highlight(int row);

	Point transform(Point point, float dt=0.f) const;
	virtual void update(IContext& context) override;

private:

	Point m_loc;     // draw location, upper left corner
	bool m_enabled;  // whether or not to scroll the pit on update()
	float m_scroll;  // y-offset for view on pit contents
	int m_peak;      // highest blocked row (may be above visible space)
	BlockVec m_blocks; // list of all blocks in the pit
	std::vector<GarbagePtr> m_garbage; // list of all garbage in the pit
	std::map<RowCol, Block> block_map; // sparse matrix of blocked spaces
	std::map<RowCol, GarbagePtr> m_garbage_map; // sparse matrix of blocked spaces

	int m_highlight_row;

};

using Pit = std::shared_ptr<PitImpl>;

class CursorImpl
{

public:

	RowCol rc;
	int time;

	CursorImpl(RowCol rc) : rc(rc), time(0) {}


};

using Cursor = std::shared_ptr<CursorImpl>;

enum class BannerFrame : size_t { WIN=0, LOSE=1 };

class BannerImpl
{
public:
	Point loc;
	BannerFrame frame;
	BannerImpl(Point loc, BannerFrame frame) : loc(loc), frame(frame) {}
};

using Banner = std::shared_ptr<BannerImpl>;

/**
 * Stage is a container for on-screen objects.
 * The Stage owns all its objects as shared_ptrs.
 */
class StageImpl
{

public:

	StageImpl() {}

	void add(Logic logic);
	void remove(Logic logic);

	void update(IContext& context);

private:

	std::vector< Logic > logics;

};

using Stage = std::shared_ptr<StageImpl>;

class StageBuilder
{

public:

	Pit left_pit;
	Pit right_pit;
	Cursor left_cursor;
	Cursor right_cursor;

	Stage construct();

};
