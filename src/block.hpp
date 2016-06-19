/**
 * block.hpp
 * Block objects
 */
#pragma once

#include "shitbrix.hpp"
#include "context.hpp"
#include <SDL2/SDL_assert.h>
#include <memory>
#include <vector>
#include <map>
#include <algorithm>
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

	virtual void draw(const IVideoContext& context, float dt) override
	{
		Point draw_loc = loc;

		// bounce when landing
		if(State::LAND == m_state) {
			draw_loc.y -= BOUNCE_H * ( m_time > LAND_TIME/2 ? (LAND_TIME-m_time+dt) : (m_time-dt) ) / LAND_TIME;
		}

		switch(col) {
			case Col::BLUE:   context.drawGfx(Gfx::BLOCK_BLUE, draw_loc);   break;
			case Col::RED:    context.drawGfx(Gfx::BLOCK_RED, draw_loc);    break;
			case Col::YELLOW: context.drawGfx(Gfx::BLOCK_YELLOW, draw_loc); break;
			case Col::GREEN:  context.drawGfx(Gfx::BLOCK_GREEN, draw_loc);  break;
			case Col::PURPLE: context.drawGfx(Gfx::BLOCK_PURPLE, draw_loc); break;
			case Col::ORANGE: context.drawGfx(Gfx::BLOCK_ORANGE, draw_loc); break;
			default: SDL_assert_paranoid(false);
		}
	}

	virtual void animate() override {}

	/**
	 * State machine spaghetti for block behavior
	 */
	virtual void update() override
	{
		m_time--;

		switch(m_state) {
			case State::REST: if(1 == m_time) set_state(State::BREAK); break; // rest lasts forever by default, auto-break for debug
			case State::FALL: fall(); break;
			case State::LAND: land(); break;
			case State::BREAK: dobreak(); break;
			case State::DEAD: throw GameException("Cannot update() dead block.");
			default: SDL_assert_paranoid(false);
		}
	}

	RowCol rc() const
	{
		return m_rc;
	}

	State state() const
	{
		return m_state;
	}

	void set_state(State state)
	{
		m_state = state;

		switch(state) {
			case State::REST:
				m_time = 0; // do not auto-break (for debug purposes)
				break;

			case State::LAND:
				// Correct the block by any eventual extra-pixels
				loc.x -= offset.x;
				loc.y -= offset.y;
				offset = Point{0,0};
				m_time = LAND_TIME;
				break;

			case State::BREAK:
				m_time = BREAK_TIME;
				break;

			default: break;
		}
	}

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

	/**
	 * Update this falling block
	 */
	void fall()
	{
		loc.y += FALL_SPEED;
		offset.y += FALL_SPEED;

		// go to next row?
		if(offset.y > BLOCK_H/2) {
			m_rc.r++;
			offset.y -= BLOCK_H;
		}

		// arrived at next row (center)
		if(offset.y >= 0 && offset.y < FALL_SPEED) {
			SharedSubscriber locked_subscriber = subscriber.lock();
			SDL_assert(locked_subscriber);
			locked_subscriber->notify_block_arrive_row(shared_from_this());
		}
	}

	/**
	 * Update this landing block
	 */
	void land()
	{
		if(m_time < 0) {
			set_state(State::REST);
			m_time = 10 - 10 * m_rc.r; // after which auto-breaks
		}
	}

	/**
	 * Update this breaking block
	 */
	void dobreak()
	{
		if(m_time < 0) {
			set_state(State::DEAD);
			// std::cerr << "This block is dead.\n";
			SharedSubscriber locked_subscriber = subscriber.lock();
			SDL_assert(locked_subscriber);
			locked_subscriber->notify_block_dead(shared_from_this());
			// std::cerr << "Subscriber has been notified.\n";
		}
	}

};

/**
 * Stage is a container for on-screen objects.
 * The Stage owns all its objects as shared_ptrs.
 */
class Stage : public IAnimation, public ILogicObject
{

public:

	Stage() {}

	void add(SharedAnimation animation)
	{
		animations.push_back(animation);
	}

	void add(SharedLogic logic)
	{
		logics.push_back(logic);
	}

	void remove(SharedAnimation animation)
	{
		auto it = std::find(animations.begin(), animations.end(), animation);
		SDL_assert(it != animations.end());
		animations.erase(it);
	}

	void remove(SharedLogic logic)
	{
		auto it = std::find(logics.begin(), logics.end(), logic);
		SDL_assert(it != logics.end());
		logics.erase(it);
	}

	void addLeftPit(SharedPit pit)
	{
		SDL_assert(!left_pit);
		left_pit = pit;
	}

	void addRightPit(SharedPit pit)
	{
		SDL_assert(!right_pit);
		right_pit = pit;
	}

	virtual void draw(const IVideoContext& context, float dt) override
	{
		context.drawGfx(Gfx::BACKGROUND, Point{0,0});
		for(auto& animation : animations) animation->draw(context, dt);
	}

	virtual void animate() override
	{
		for(auto& animation : animations) animation->animate();
	}

	virtual void update() override
	{
		for(auto& logic : logics) logic->update();
	}

private:

	std::vector< SharedAnimation > animations;
	std::vector< SharedLogic > logics;
	SharedPit left_pit;
	SharedPit right_pit;

};

using SharedStage = std::shared_ptr<Stage>;
using WeakStage = std::weak_ptr<Stage>;

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit itself does not animate or update the contained blocks and garbage.
 */
class Pit : public IPit, public IAnimation, public ILogicObject
{

public:

	Pit(Point loc) : m_loc(loc) {}

	virtual void draw(const IVideoContext& context, float dt) override
	{
		// nothing so far
	}

	virtual void animate() override {}
	virtual void update() override {}

	virtual Point loc() const override
	{
		return m_loc;
	}

	/**
	 * Set the given location to blocked.
	 */
	virtual void block(RowCol rc, WeakBlock block) override
	{
		auto result = block_map.emplace(std::make_pair(rc, block));
		game_assert(result.second, "Attempt to block already blocked space in Pit.");
	}

	/**
	 * Set the given location to not blocked.
	 */
	virtual void unblock(RowCol rc) override
	{
		size_t erased = block_map.erase(rc);
		game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
	}

	/**
	 * Return the block at the given location.
	 */
	virtual WeakBlock block_at(RowCol rc) const override
	{
		auto it = block_map.find(rc);
		if(it == block_map.end())
			return WeakBlock(); // default-constructed weak_ptr resembles nullptr
		else
			return it->second;
	}

private:

	Point m_loc;   // location, upper left corner
	std::map<RowCol, WeakBlock> block_map; // sparse matrix of blocked spaces

};

class StageBuilder
{

public:

	std::shared_ptr<Stage> construct()
	{
		SharedStage stage = std::make_shared<Stage>();

		auto lpit = std::make_shared<Pit>(LPIT_LOC);
		auto rpit = std::make_shared<Pit>(RPIT_LOC);

		m_left_pit = static_cast<WeakPit>(lpit);
		m_right_pit = static_cast<WeakPit>(rpit);

		stage->add(static_cast<SharedAnimation>(lpit));
		stage->add(static_cast<SharedLogic>(lpit));
		stage->add(static_cast<SharedAnimation>(rpit));
		stage->add(static_cast<SharedLogic>(rpit));

		// auto block1 = std::make_shared<Block> (Block::Col::BLUE, (Point){30,30}, 0, 0, Block::State::REST, 0);
		// stage->add(static_cast<SharedAnimation>(block1));
		// stage->add(static_cast<SharedLogic>(block1));

		// auto block2 = std::make_shared<Block> (Block::Col::RED, (Point){80,30}, 0, 0, Block::State::FALL, 0);
		// stage->add(static_cast<SharedAnimation>(block2));
		// stage->add(static_cast<SharedLogic>(block2));

		return stage;
	}

	WeakPit left_pit() const
	{
		return m_left_pit;
	}

	WeakPit right_pit() const
	{
		return m_right_pit;
	}

private:

	WeakPit m_left_pit;
	WeakPit m_right_pit;

};
