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

// drawing order
constexpr int PIT_Z = 1;
constexpr int BLOCK_Z = 3;
constexpr int PITVIEW_Z = 4;
constexpr int CURSOR_Z = 5;

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

	virtual void draw(const IVideoContext& context, float dt) override;
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

bool y_greater(const Block& lhs, const Block& rhs);
bool fallible(Block block);
bool swappable(Block block);
bool matchable(Block block);

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit does not own, animate or update its contained blocks and garbage (the stage does),
 * but it remembers where blocks are.
 * It also handles scrolling.
 */
class PitImpl : public ITransform, public IAnimation, public ILogicObject
{

public:

	PitImpl(Point loc) : IAnimation(PIT_Z), m_loc(loc), m_scroll(BLOCK_H - PIT_H) {}

	Point loc() const { return m_loc; }
	int top() const;
	int bottom() const;
	void block(RowCol rc, Block block);
	void unblock(RowCol rc);
	void swap(RowCol lrc, RowCol rrc);
	Block block_at(RowCol rc) const;

	virtual Point transform(Point point, float dt=0.f) const override;

	virtual void draw(const IVideoContext& context, float dt) override {} // nothing so far
	virtual void animate() override {}
	virtual void update() override;

private:

	Point m_loc;     // draw location, upper left corner
	float m_scroll;  // y-offset for view on pit contents
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
	virtual void draw(const IVideoContext& context, float dt) override;
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

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override;

private:

	static constexpr int FRAME_TIME = 4; // how many sceen frames to display one cursor frame
	static constexpr int FRAMES = 4; // number of available cursor frames

	int anim;
	Transform view;

};

using Cursor = std::shared_ptr<CursorImpl>;

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

	void draw(const IVideoContext& context, float dt);
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

	Stage construct();

};
