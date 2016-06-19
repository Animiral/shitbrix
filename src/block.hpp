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

// Single block, comes in 6 colors
// State machine: a block can change state only after its time has run down (1 per tick)
class Block : public IAnimation, public ILogicObject
{

public:

	enum class Col { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };
	enum class State { INVALID, REST, FALL, LAND, BREAK, DEAD };

	// Public properties - can be read/changed/corrected at will
	Col col;         // color
	RowCol rc;       // row/col position, - is UP, + is DOWN
	Point offset;    // x/y offset from draw center of r/c location

	Block(Col col, RowCol rc, State state, SharedTransform view)
	:
	col(col), rc(rc), offset{0,0}, m_view(view),
	m_loc{static_cast<float>(rc.c*BLOCK_W), static_cast<float>(rc.r*BLOCK_H)},
	m_state(state), m_time(0)
	{}

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override {}
	virtual void update() override;
	Point loc() const { return m_view->transform(m_loc); }
	State state() const { return m_state; }
	void set_state(State state);
	bool entering_row();

private:

	static constexpr float BOUNCE_H = 10.f;
	static constexpr int LAND_TIME = 20;
	static constexpr int BREAK_TIME = 1; // 30;

	SharedTransform m_view; // view applied to m_loc
	Point m_loc;     // logical location, upper left corner relative to view (not necessarily sprite draw location)
	State m_state;   // current block state. On state time out, tell an IStateSubscriber (previously saved via Block::subscribe()) with notify()
	int m_time;      // number of ticks until we consider a state switch

	void fall();
	void land();
	void dobreak();
	
};

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit does not own, animate or update its contained blocks and garbage (the stage does),
 * but it remembers where blocks are and which spaces are free or blocked.
 * It also handles scrolling.
 */
class Pit : public ITransform, public IAnimation, public ILogicObject
{

public:

	Pit(Point loc) : m_loc(loc), m_scroll(BLOCK_H - PIT_H) {}

	Point loc() const { return m_loc; }
	int bottom() const;
	void block(RowCol rc, WeakBlock block);
	void unblock(RowCol rc);
	WeakBlock block_at(RowCol rc) const;

	virtual Point transform(Point point, float dt=0.f) const override;

	virtual void draw(const IVideoContext& context, float dt) override {} // nothing so far
	virtual void animate() override {}
	virtual void update() override;

private:

	Point m_loc;     // draw location, upper left corner
	float m_scroll;  // y-offset for view on pit contents
	std::map<RowCol, WeakBlock> block_map; // sparse matrix of blocked spaces

};

/**
 * Stage is a container for on-screen objects.
 * The Stage owns all its objects as shared_ptrs.
 */
class Stage : public IAnimation, public ILogicObject
{

public:

	Stage() {}

	void add(SharedAnimation animation);
	void add(SharedLogic logic);
	void remove(SharedAnimation animation);
	void remove(SharedLogic logic);

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override;
	virtual void update() override;

private:

	std::vector< SharedAnimation > animations;
	std::vector< SharedLogic > logics;

};

using SharedStage = std::shared_ptr<Stage>;
using WeakStage = std::weak_ptr<Stage>;

class StageBuilder
{
public:
	SharedStage construct();
};

using SharedPit = std::shared_ptr<Pit>;
