/**
 * Definitions for stage objects
 */

#include "stage.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>
#include <functional>

int operator-(BlockCol lhs, BlockCol rhs)
{
	return static_cast<int>(lhs) - static_cast<int>(rhs);
}

/**
 * State machine spaghetti for block behavior
 */
void BlockImpl::update(IContext& context)
{
	time--;

	switch(m_state) {
		case BlockState::PREVIEW: break;
		case BlockState::REST: break;
		case BlockState::SWAP: swap(); break;
		case BlockState::FALL: fall(); break;
		case BlockState::LAND: land(); break;
		case BlockState::BREAK: dobreak(); break;
		case BlockState::DEAD: throw GameException("Cannot update() dead block.");
		default: SDL_assert_paranoid(false);
	}
}

/**
 * Changes the block’s logical location while maintaining its draw position,
 * now relative to the new rc.
 */
void BlockImpl::set_rc(RowCol rc)
{
	offset.x -= (rc.c - m_rc.c) * COL_W;
	offset.y -= (rc.r - m_rc.r) * ROW_H;
	m_rc = rc;
}

void BlockImpl::set_state(BlockState state)
{
	SDL_assert(state != BlockState::PREVIEW && state != BlockState::SWAP); // use swap_toward() instead
	SDL_assert(m_state != BlockState::DEAD); // cannot change out of dead state

	m_state = state;

	switch(state) {
		case BlockState::LAND:
			// Correct the block by any eventual extra-pixels
			m_loc.x -= offset.x;
			m_loc.y -= offset.y;
			offset = Point{0,0};
			time = LAND_TIME;
			break;

		case BlockState::BREAK:
			time = BREAK_TIME;
			m_anim = BlockFrame::BREAK_BEGIN;
			break;

		default: break;
	}
}

/**
 * Starts the swapping state & animation for this block.
 * This function replaces set_state(BlockState::SWAP) because of the additional
 * information that must be conveyed in the target parameter.
 */
void BlockImpl::swap_toward(RowCol target)
{
	m_state = BlockState::SWAP;
	time = SWAP_TIME;
	m_target = from_rc(target);
}

/**
 * Returns true if the block is just now arriving at the center of a new row.
 */
bool BlockImpl::is_arriving()
{
	return BlockState::FALL == m_state && offset.y >= 0 && offset.y < FALL_SPEED;
}

/**
 * Update this swapping block
 */
void BlockImpl::swap()
{
	if(time > 0.f) {
		float adv_x = (m_target.x - m_loc.x) / time;
		float adv_y = (m_target.y - m_loc.y) / time;
		m_loc.x += adv_x;
		m_loc.y += adv_y;
		offset.x += adv_x;
		offset.y += adv_y;
	}
	else {
		m_loc = m_target;
		offset = Point{0,0};
	}
}

/**
 * Update this falling block
 */
void BlockImpl::fall()
{
	m_loc.y += FALL_SPEED;
	offset.y += FALL_SPEED;
}

/**
 * Update this landing block
 */
void BlockImpl::land()
{
	if(time < 0) {
		set_state(BlockState::REST);
		time = 10 - 10 * m_rc.r; // after which auto-breaks
	}
}

/**
 * Update this breaking block
 */
void BlockImpl::dobreak()
{
	if(time < 0) {
		set_state(BlockState::DEAD);
	}
}

/**
 * Comparison predicate for ordering blocks bottom-to-top.
 */
bool y_greater(const Block& lhs, const Block& rhs)
{
	return rhs->rc().r < lhs->rc().r;
}

bool fallible(Block block)
{
	BlockState state = block->state();
	return BlockState::REST == state || BlockState::LAND == state;
}

bool swappable(Block block)
{
	BlockState state = block->state();
	return BlockState::REST == state ||
	       BlockState::SWAP == state ||
	       BlockState::FALL == state ||
	       BlockState::LAND == state;
}

bool matchable(Block block)
{
	BlockState state = block->state();
	return BlockState::REST == state || BlockState::LAND == state;
}


void Garbage::update(IContext& context)
{
	time--;

	switch(m_state) {
		case State::REST: break;
		case State::FALL: fall(); break;
		case State::LAND: land(); break;
		case State::DISSOLVE: dobreak(); break;
		case State::DEAD: throw GameException("Cannot update() dead garbage.");
		default: SDL_assert_paranoid(false);
	}
}

/**
 * Changes the garbage’s logical location while maintaining its draw position,
 * now relative to the new rc.
 */
void Garbage::set_rc(RowCol rc)
{
	offset.x -= (rc.c - m_rc.c) * COL_W;
	offset.y -= (rc.r - m_rc.r) * ROW_H;
	m_rc = rc;
}

void Garbage::set_state(State state)
{
	SDL_assert(m_state != State::DEAD); // cannot change out of dead state

	m_state = state;

	switch(state) {
		case State::LAND:
			// Correct the garbage by any eventual extra-pixels
			m_loc.x -= offset.x;
			m_loc.y -= offset.y;
			offset = Point{0,0};
			time = LAND_TIME;
			break;

		case State::DISSOLVE:
			time = DISSOLVE_TIME;
			break;

		default: break;
	}
}

bool Garbage::is_arriving()
{
	return State::FALL == m_state && offset.y >= 0 && offset.y < FALL_SPEED;
}

void Garbage::fall()
{
	m_loc.y += FALL_SPEED;
	offset.y += FALL_SPEED;
}

void Garbage::land()
{
	if(time < 0) {
		set_state(State::REST);
	}
}

void Garbage::dobreak()
{
	if(time < 0) {
		set_state(State::DEAD);
	}
}


/**
 * Constructs a Pit at the specified draw location.
 */
Pit::Pit(Point loc)
: m_loc(loc), m_enabled(true), m_scroll(ROW_H - PIT_H),
  m_peak(1), m_highlight_row(0)
{
}

/**
 * Return the block at the given location.
 */
Block Pit::block_at(RowCol rc) const
{
	auto it = block_map.find(rc);
	if(it == block_map.end())
		return nullptr;
	else
		return it->second;
}

/**
 * Return the garbage at the given location.
 */
GarbagePtr Pit::garbage_at(RowCol rc) const
{
	auto it = m_garbage_map.find(rc);
	if(it == m_garbage_map.end())
		return nullptr;
	else
		return it->second;
}

bool Pit::anything_at(RowCol rc) const
{
	return
		block_map.find(rc) != block_map.end() || 
		m_garbage_map.find(rc) != m_garbage_map.end();
}

BlockImpl& Pit::spawn_block(BlockCol color, RowCol rc, BlockState state)
{
	game_assert(rc.c >= 0 && rc.c < PIT_COLS, "Attempt to spawn block out of bounds.");
	game_assert(!anything_at(rc), "Attempt to spawn block at occupied location.");

	auto block = std::make_shared<BlockImpl>(color, rc, state);
	m_blocks.push_back(block);

	auto result = block_map.emplace(rc, block);
	game_assert(result.second, "Attempt to block already blocked space in Pit.");

	if(rc.r < m_peak)
		m_peak = rc.r;

	return *m_blocks.back();
}

Garbage& Pit::spawn_garbage(RowCol rc, int width, int height)
{
	// make sure the Garbage fits in the Pit
	game_assert(rc.c >= 0 && rc.c + width <= PIT_COLS, "Attempt to spawn garbage out of bounds.");

	GarbagePtr garbage = std::make_shared<Garbage>(rc, width, height);
	m_garbage.push_back(garbage);

	block_garbage(garbage);

	if(rc.r < m_peak)
		m_peak = rc.r;

	return *m_garbage.back();
}

bool Pit::can_fall(RowCol from) const
{
	auto block = block_at(from);
	auto garbage = garbage_at(from);
	RowCol to {from.r+1, from.c};

	// there must be no obstacle at the target location and the
	// target location must be a valid location in the pit
	if(block) {
		return !anything_at(to);
	}
	else if(garbage) {
		for(int c = to.c; c < to.c + garbage->columns(); c++) {
			RowCol target{to.r, c};

			if(anything_at(target))
				return false;
		}
	}
	else {
		game_assert(false, "Asking to fall from empty coordinate in Pit.");
	}

	return true;
}

void Pit::fall(RowCol from)
{
	auto block = block_at(from);
	auto garbage = garbage_at(from);

	if(block) fall_block(block);
	else if(garbage) fall_garbage(garbage);
	else game_assert(false, "Attempt to fall from empty coordinate in Pit.");

	refresh_peak();
}

void Pit::swap(RowCol lrc, RowCol rrc)
{
	auto left = block_map.find(lrc);
	auto right = block_map.find(rrc);
	auto end = block_map.end();

	game_assert(left != end && right != end, "Attempt to swap nonexistant blocks.");

	std::swap(left->second, right->second);
	left->second->set_rc(rrc);
	right->second->set_rc(lrc);
}

void Pit::remove_dead()
{
	bool did_erase = false;

	for(auto it = m_blocks.begin(); it != m_blocks.end(); ) {
		if(BlockState::DEAD == (*it)->state()) {
			RowCol rc = (*it)->rc();

			size_t erased = block_map.erase(rc);
			game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
			did_erase = true;

			it = m_blocks.erase(it);
		}
		else {
			++it;
		}
	}

	if(did_erase)
		refresh_peak();
}

Garbage* Pit::shrink(Garbage& garbage)
{
	RowCol rc = garbage.rc();
	int low = rc.r + garbage.rows() - 1;

	for(int c = rc.c; c < rc.c + garbage.columns(); c++) {
		size_t erased = m_garbage_map.erase(RowCol{low, c});
		game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
	}

	// The garbage loses one row. If that is all, remove it entirely.
	if(garbage.shrink() <= 0) {
		auto is_gone = [] (GarbagePtr gptr) { return gptr->rows() <= 0; };
		std::remove_if(m_garbage.begin(), m_garbage.end(), is_gone);
		refresh_peak();
		return nullptr;
	}
	else {
		return &garbage;
	}
}

void Pit::clear()
{
	m_blocks.clear();
	m_garbage.clear();
	block_map.clear();
	m_garbage_map.clear();
	m_peak = 1;
}

/**
 * Returns the number of the top accessible row in the pit
 */
int Pit::top() const
{
	return std::ceil(m_scroll / ROW_H);
}

/**
 * Returns the number of the bottom accessible row in the pit
 */
int Pit::bottom() const
{
	return std::floor((m_scroll + PIT_H) / ROW_H) - 1;
}

int Pit::peak() const
{
	return m_peak;
}

void Pit::highlight(int row)
{
	m_highlight_row = row;
}

/**
 * The origin {0,0} location of all pit-related objects corresponds with row 0, column 0.
 * We have to transform the object into the pit and from there, apply the pit scrolling.
 */
Point Pit::transform(Point point, float dt) const
{
	point.x += m_loc.x;
	point.y += m_loc.y;
	// TODO: include dt in scroll anim, don’t forget FPS-TPS conversion
	point.y -= m_scroll;
	return point;
}

void Pit::update(IContext& context)
{
	for(auto b : m_blocks)
		b->update(context);

	for(auto g : m_garbage)
		g->update(context);

	if(m_enabled)
		m_scroll += SCROLL_SPEED;
}

void Pit::refresh_peak()
{
	// maintain peak by linear search through the pit contents
	int lowest_row = this->bottom();

	while(m_peak < lowest_row) {
		for(int c = 0; c < PIT_COLS; c++) {
			if(this->anything_at({m_peak, c}))
				return;
		}

		m_peak++; // try next row
	}
}

void Pit::fall_block(Block block)
{
	RowCol rc = block->rc();
	RowCol to { rc.r+1, rc.c };

	game_assert(!anything_at(to), "Attempt to move block to occupied location.");

	auto erased = block_map.erase(rc);
	game_assert(1 == erased, "Block not found at expected space in Pit.");
	auto emplace_result = block_map.emplace(to, block);
	game_assert(emplace_result.second, "Attempt to block already blocked space in Pit.");
	block->set_rc(to);
}

void Pit::fall_garbage(GarbagePtr garbage)
{
	RowCol rc = garbage->rc();
	RowCol to { rc.r+1, rc.c };

	unblock_garbage(*garbage);
	garbage->set_rc(to);
	block_garbage(garbage);
}

void Pit::block_garbage(GarbagePtr garbage)
{
	RowCol rc = garbage->rc();

	for(int r = rc.r; r < rc.r + garbage->rows(); r++) {
		for(int c = rc.c; c < rc.c + garbage->columns(); c++) {
			RowCol target{r, c};
			auto result = m_garbage_map.emplace(std::make_pair(target, garbage));
			game_assert(result.second, "Attempt to block already blocked space in Pit.");
		}
	}
}

void Pit::unblock_garbage(const Garbage& garbage)
{
	RowCol rc = garbage.rc();

	for(int r = rc.r; r < rc.r + garbage.rows(); r++) {
		for(int c = rc.c; c < rc.c + garbage.columns(); c++) {
			RowCol target{r, c};
			size_t erased = m_garbage_map.erase(target);
			game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
		}
	}
}


Stage::PitCursor::PitCursor(Point loc)
: pit(loc),
  cursor(RowCol{ -PIT_ROWS/2, PIT_COLS/2-1 })
{}

Stage::PitCursor& Stage::add_pit(Point loc)
{
	m_pits.push_back(std::make_unique<PitCursor>(loc));
	return *m_pits.back();
}

void Stage::update(IContext& context)
{
	for(auto& pc : m_pits) {
		pc->pit.update(context);
		pc->cursor.update();
	}
}


std::unique_ptr<Stage> StageBuilder::construct()
{
	auto stage = std::make_unique<Stage>();

	auto& left_pc = stage->add_pit(LPIT_LOC);
	auto& right_pc = stage->add_pit(RPIT_LOC);

	this->left_pit = &left_pc.pit;
	this->right_pit = &right_pc.pit;
	this->left_cursor = &left_pc.cursor;
	this->right_cursor = &right_pc.cursor;

	return stage;
}
