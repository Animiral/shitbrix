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

enum class BlockCol { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };
enum class BlockState { INVALID, PREVIEW, REST, FALL, LAND, BREAK, DEAD };

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
	BlockCol col;         // color
	RowCol rc;       // row/col position, - is UP, + is DOWN
	Point offset;    // x/y offset from draw center of r/c location

	BlockImpl(BlockCol col, RowCol rc, Transform view)
	:
	col(col), rc(rc), offset{0,0}, m_view(view),
	m_loc{static_cast<float>(rc.c*BLOCK_W), static_cast<float>(rc.r*BLOCK_H)},
	m_state(BlockState::PREVIEW), m_time(0)
	{}

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override;
	virtual void update() override;

	Point loc() const { return m_view->transform(m_loc); }
	BlockState state() const { return m_state; }
	bool is_obstacle() const;
	void set_state(BlockState state);
	bool entering_row();

private:

	static constexpr float BOUNCE_H = 10.f;
	static constexpr int LAND_TIME = 20;
	static constexpr int BREAK_TIME = 30;

	Transform m_view; // view applied to m_loc
	Point m_loc;       // logical location, upper left corner relative to view (not necessarily sprite draw location)
	BlockState m_state;     // current block state. On state time out, tell an IStateSubscriber (previously saved via BlockImpl::subscribe()) with notify()
	int m_time;        // number of ticks until we consider a state switch
	BlockFrame m_anim; // current animation frame

	void fall();
	void land();
	void dobreak();
	
};

using Block = std::shared_ptr<BlockImpl>;

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit does not own, animate or update its contained blocks and garbage (the stage does),
 * but it remembers where blocks are and which spaces are free or blocked.
 * It also handles scrolling.
 */
class PitImpl : public ITransform, public IAnimation, public ILogicObject
{

public:

	PitImpl(Point loc) : m_loc(loc), m_scroll(BLOCK_H - PIT_H) {}

	Point loc() const { return m_loc; }
	int bottom() const;
	void block(RowCol rc, Block block);
	void unblock(RowCol rc);
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
 * Stage is a container for on-screen objects.
 * The Stage owns all its objects as shared_ptrs.
 */
class StageImpl : public IAnimation, public ILogicObject
{

public:

	StageImpl() {}

	void add(Animation animation);
	void add(Logic logic);
	void remove(Animation animation);
	void remove(Logic logic);

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override;
	virtual void update() override;

private:

	std::vector< Animation > animations;
	std::vector< Logic > logics;

};

using Stage = std::shared_ptr<StageImpl>;

class StageBuilder
{
public:
	Stage construct();
};
