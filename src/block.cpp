/**
 * Definitions for block and stage objects
 */

#include "block.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

void Block::draw(const IVideoContext& context, float dt)
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

/**
 * State machine spaghetti for block behavior
 */
void Block::update()
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

void Block::set_state(State state)
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

/**
 * Update this falling block
 */
void Block::fall()
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
void Block::land()
{
	if(m_time < 0) {
		set_state(State::REST);
		m_time = 10 - 10 * m_rc.r; // after which auto-breaks
	}
}

/**
 * Update this breaking block
 */
void Block::dobreak()
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


/**
 * Set the given location to blocked.
 */
void Pit::block(RowCol rc, WeakBlock block)
{
	auto result = block_map.emplace(std::make_pair(rc, block));
	game_assert(result.second, "Attempt to block already blocked space in Pit.");
}

/**
 * Set the given location to not blocked.
 */
void Pit::unblock(RowCol rc)
{
	size_t erased = block_map.erase(rc);
	game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
}

/**
 * Return the block at the given location.
 */
WeakBlock Pit::block_at(RowCol rc) const
{
	auto it = block_map.find(rc);
	if(it == block_map.end())
		return WeakBlock(); // default-constructed weak_ptr resembles nullptr
	else
		return it->second;
}


void Stage::add(SharedAnimation animation)
{
	animations.push_back(animation);
}

void Stage::add(SharedLogic logic)
{
	logics.push_back(logic);
}

void Stage::remove(SharedAnimation animation)
{
	auto it = std::find(animations.begin(), animations.end(), animation);
	SDL_assert(it != animations.end());
	animations.erase(it);
}

void Stage::remove(SharedLogic logic)
{
	auto it = std::find(logics.begin(), logics.end(), logic);
	SDL_assert(it != logics.end());
	logics.erase(it);
}

void Stage::addLeftPit(SharedPit pit)
{
	SDL_assert(!left_pit);
	left_pit = pit;
}

void Stage::addRightPit(SharedPit pit)
{
	SDL_assert(!right_pit);
	right_pit = pit;
}

void Stage::draw(const IVideoContext& context, float dt)
{
	context.drawGfx(Gfx::BACKGROUND, Point{0,0});
	for(auto& animation : animations) animation->draw(context, dt);
}

void Stage::animate()
{
	for(auto& animation : animations) animation->animate();
}

void Stage::update()
{
	for(auto& logic : logics) logic->update();
}


std::shared_ptr<Stage> StageBuilder::construct()
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

WeakPit StageBuilder::left_pit() const
{
	return m_left_pit;
}

WeakPit StageBuilder::right_pit() const
{
	return m_right_pit;
}
