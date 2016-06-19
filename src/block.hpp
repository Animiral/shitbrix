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
class Block : public IAnimation, public ILogicObject, public std::enable_shared_from_this<Block>
{

public:

	enum class Col { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };
	enum class State { INVALID, REST, FALL, LAND, BREAK, DEAD };

	Block(Col col, Point loc, RowCol rc, State state, WeakSubscriber subscriber)
	: col(col), loc(loc), m_rc(rc), offset{0,0}, m_state(state), m_time(0), subscriber(subscriber) {}

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override {}
	virtual void update() override;
	RowCol rc() const { return m_rc; }
	State state() const { return m_state; }
	void set_state(State state);

private:

	static constexpr float BOUNCE_H = 10.f;
	static constexpr int LAND_TIME = 20;
	static constexpr int BREAK_TIME = 1; // 30;

	Col col;         // color
	Point loc;       // logical location, upper left corner (not necessarily sprite draw location)
	RowCol m_rc;     // row/col position, - is UP, + is DOWN
	Point offset;    // x/y offset from draw center of r/c location
	State m_state;   // current block state. On state time out, tell an IStateSubscriber (previously saved via Block::subscribe()) with notify()
	int m_time;      // number of ticks until we consider a state switch
	WeakSubscriber subscriber; // receives notifications about block state

	void fall();
	void land();
	void dobreak();
	
};

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit itself does not animate or update the contained blocks and garbage.
 */
class Pit : public IPit, public IAnimation, public ILogicObject
{

public:

	Pit(Point loc) : m_loc(loc) {}

	virtual void draw(const IVideoContext& context, float dt) override {} // nothing so far
	virtual void animate() override {}
	virtual void update() override {}

	virtual Point loc() const override { return m_loc; }
	virtual void block(RowCol rc, WeakBlock block) override;
	virtual void unblock(RowCol rc) override;
	virtual WeakBlock block_at(RowCol rc) const override;

private:

	Point m_loc;   // location, upper left corner
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
	void addLeftPit(SharedPit pit);
	void addRightPit(SharedPit pit);

	virtual void draw(const IVideoContext& context, float dt) override;
	virtual void animate() override;
	virtual void update() override;

private:

	std::vector< SharedAnimation > animations;
	std::vector< SharedLogic > logics;
	SharedPit left_pit;
	SharedPit right_pit;

};

using SharedStage = std::shared_ptr<Stage>;
using WeakStage = std::weak_ptr<Stage>;

class StageBuilder
{

public:

	std::shared_ptr<Stage> construct();
	WeakPit left_pit() const;
	WeakPit right_pit() const;

private:

	WeakPit m_left_pit;
	WeakPit m_right_pit;

};
