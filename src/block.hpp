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
class BlockImpl : public IAnimation, public ILogicObject
{

public:

	// Public properties - can be read/changed/corrected at will
	BlockCol col;    // color
	Point offset;    // x/y offset from draw center of r/c location
	int time;        // number of ticks until we consider a state switch

	BlockImpl(BlockCol col, RowCol rc, Transform view)
	:
	IAnimation(BLOCK_Z), col(col), offset{0,0}, time(0), m_view(view),
	m_loc(from_rc(rc)), m_rc(rc), m_state(BlockState::PREVIEW)
	{}

	virtual void draw(IVideoContext& context, float dt) override;
	virtual void animate() override;
	virtual void update() override;

	Point loc() const { return m_view->transform(m_loc); }
	RowCol rc() const { return m_rc; }
	void set_rc(RowCol rc);
	BlockState state() const { return m_state; }
	void set_state(BlockState state);
	void swap_toward(RowCol target);

	bool is_arriving();

private:

	static constexpr float BOUNCE_H = 10.f;
	static constexpr int SWAP_TIME = 6;
	static constexpr int LAND_TIME = 20;
	static constexpr int BREAK_TIME = 30;

	Transform m_view;   // view applied to m_loc
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
 * A pit is the playing area where one player’s blocks fall down.
 * The pit owns, animates and updates its contained blocks and garbage.
 * It remembers where blocks are in a sparse matrix.
 * It also handles scrolling.
 */
class PitImpl : public ITransform, public IAnimation, public ILogicObject
{

public:

	PitImpl(Point loc) : IAnimation(PIT_Z), m_loc(loc), m_enabled(true), m_scroll(BLOCK_H - PIT_H) {}

	Point loc() const { return m_loc; }
	BlockVec& blocks() { return m_blocks; }
	int top() const;
	int bottom() const;
	Block block_at(RowCol rc) const;

	void block(RowCol rc, Block block);
	void unblock(RowCol rc);
	void swap(RowCol lrc, RowCol rrc);
	void stop() { m_enabled = false; }
	void start() { m_enabled = true; }

	virtual Point transform(Point point, float dt=0.f) const override;

	virtual void draw(IVideoContext& context, float dt) override;
	virtual void animate() override { for(auto b : m_blocks) b->animate(); }
	virtual void update() override;

private:

	Point m_loc;     // draw location, upper left corner
	bool m_enabled;  // whether or not to scroll the pit on update()
	float m_scroll;  // y-offset for view on pit contents
	BlockVec m_blocks; // list of all blocks in the pit
	std::map<RowCol, Block> block_map; // sparse matrix of blocked spaces

};

using Pit = std::shared_ptr<PitImpl>;

/**
 * Pit visualization for debugging.
 */
class PitViewImpl : public IAnimation
{
public:
	PitViewImpl(Pit pit) : IAnimation(PITVIEW_Z), pit(pit), m_show(false) {}
	virtual void draw(IVideoContext& context, float dt) override;
	virtual void animate() override {}
	void toggle() { m_show = !m_show; }
private:
	Pit pit;
	bool m_show; // whether or not to display this
};

using PitView = std::shared_ptr<PitViewImpl>;

class CursorImpl : public IAnimation
{

public:

	RowCol rc;

	CursorImpl(RowCol rc, Transform view) : IAnimation(CURSOR_Z), rc(rc), anim(0), view(view) {}

	virtual void draw(IVideoContext& context, float dt) override;
	virtual void animate() override;

private:

	static constexpr int FRAME_TIME = 4; // how many sceen frames to display one cursor frame
	static constexpr int FRAMES = 4; // number of available cursor frames

	int anim;
	Transform view;

};

using Cursor = std::shared_ptr<CursorImpl>;

enum class BannerFrame : size_t { WIN=0, LOSE=1 };

class BannerImpl : public IAnimation
{
public:
	Point loc;
	BannerFrame frame;
	BannerImpl(Point loc, BannerFrame frame) : IAnimation(BANNER_Z), loc(loc), frame(frame) {}
	virtual void draw(IVideoContext& context, float dt) override;
	virtual void animate() override {}
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

	void add(Animation animation);
	void add(Logic logic);
	void remove(Animation animation);
	void remove(Logic logic);

	void draw(IVideoContext& context, float dt);
	void animate();
	void update();

private:

	std::vector< Animation > animations;
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
