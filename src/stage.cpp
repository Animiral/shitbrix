/**
 * Definitions for stage objects
 */

#include "stage.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>
#include <functional>

Physical::Physical(RowCol rc, State state)
: m_loc(from_rc(rc)),
  m_offset{0,0},
  m_target(m_loc),
  m_rc(rc),
  m_state(state),
  m_time(1),
  m_speed(1)
{
	// exclude locations that are well-known to lie out of bounds
	SDL_assert(rc.c >= 0 && rc.c < PIT_COLS);

	// exclude states in which we know that physicals do not spawn
	SDL_assert(State::DEAD != state &&
	           State::LAND != state &&
	           State::BREAK != state);
}

float Physical::eta() const noexcept
{
	return float(m_time) / m_speed;
}

void Physical::set_rc(RowCol rc)
{
	m_offset.x -= (rc.c - m_rc.c) * COL_W;
	m_offset.y -= (rc.r - m_rc.r) * ROW_H;
	m_rc = rc;
}

bool Physical::is_arriving() const noexcept
{
	// Physical states are generally time-based.
	return (m_time <= 0 && m_time > -m_speed) ||
	// However, the falling mechanics still use pixel offsets.
	// The second check addresses this for compatibility.
	       (State::FALL == m_state && m_offset.y >= 0);
}

bool Physical::is_fallible() const noexcept
{
	return State::REST == m_state || State::LAND == m_state;
}

void Physical::update()
{
	SDL_assert(State::DEAD != m_state);

	m_time -= m_speed;
	update_impl();

	if(State::FALL == m_state) {
		// because FALL_SPEED is in points and m_loc, m_offset are pixels,
		// adjust it based on the known pixel height of a row.
		m_loc.y += ROW_H * FALL_SPEED / ROW_HEIGHT;
		m_offset.y += ROW_H * FALL_SPEED / ROW_HEIGHT;
	}
	else if(State::LAND == m_state) {
		if(is_arriving()) {
			set_state(State::REST);
		}
	}
}

void Physical::set_state(State state, int time, int speed) noexcept
{
	SDL_assert(m_state != State::DEAD); // cannot change out of dead state
	SDL_assert(time >= 1); // state must last at least one tick
	SDL_assert(speed >= 1); // time must run out, not in

	set_state_impl(state, time, speed);

	m_state = state;
	m_time = time;
	m_speed = speed;

	// Correct the object by any eventual extra-pixels
	if(State::LAND == state) {
		m_loc.x -= m_offset.x;
		m_loc.y -= m_offset.y;
		m_offset = Point{0,0};
	}
}


Block::Block(Color col, RowCol rc, State state)
: Physical(rc, static_cast<Physical::State>(state)),
  col(col),
  chaining(false),
  m_anim(BlockFrame::REST)
{}

void Block::swap_toward(RowCol target) noexcept
{
	set_state(static_cast<Physical::State>(State::SWAP), SWAP_TIME);
	m_target = from_rc(target);
}

bool Block::is_swappable() const noexcept
{
	State state = block_state();

	return State::REST == state ||
	       State::SWAP == state ||
	       State::FALL == state ||
	       State::LAND == state;
}

bool Block::is_matchable() const noexcept
{
	State state = block_state();
	return State::REST == state || State::LAND == state;
}

void Block::update_impl()
{
	State bstate = block_state();

	if(State::SWAP == bstate) {
		swap();
	}
	else if(State::BREAK == bstate) {
		dobreak();
	}
}

void Block::set_state_impl(Physical::State state, int, int) noexcept
{
	SDL_assert(State::PREVIEW != static_cast<State>(state));

	if(Physical::State::BREAK == state) {
		m_anim = BlockFrame::BREAK_BEGIN;
	}
}

void Block::swap()
{
	float time = eta();

	if(time > 0.f) {
		float adv_x = (m_target.x - m_loc.x) / time;
		float adv_y = (m_target.y - m_loc.y) / time;
		m_loc.x += adv_x;
		m_loc.y += adv_y;
		m_offset.x += adv_x;
		m_offset.y += adv_y;
	}
	else {
		m_loc = m_target;
		m_offset = Point{0,0};
	}
}


void Block::dobreak()
{
	if(is_arriving()) {
		set_state(Physical::State::DEAD);
	}
}

int operator-(Block::Color lhs, Block::Color rhs) noexcept
{
	return static_cast<int>(lhs) - static_cast<int>(rhs);
}

bool y_greater(const Block& lhs, const Block& rhs) noexcept
{
	return rhs.rc().r < lhs.rc().r;
}


Garbage::Garbage(RowCol rc, int columns, int rows)
: Physical(rc, State::FALL),
  m_columns(columns),
  m_rows(rows)
{}


Pit::Pit(Point loc) noexcept
: m_loc(loc), m_enabled(true), m_scroll((1-PIT_ROWS) * ROW_HEIGHT),
  m_speed(SCROLL_SPEED), m_peak(1), m_highlight_row(0)
{
}

Physical* Pit::at(RowCol rc) const noexcept
{
	auto it = m_content_map.find(rc);
	if(it == m_content_map.end())
		return nullptr;
	else
		return it->second;
}

Block* Pit::block_at(RowCol rc) const noexcept
{
	return dynamic_cast<Block*>(at(rc));
}

Garbage* Pit::garbage_at(RowCol rc) const noexcept
{
	return dynamic_cast<Garbage*>(at(rc));
}

Block& Pit::spawn_block(Block::Color color, RowCol rc, Block::State state)
{
	game_assert(rc.c >= 0 && rc.c < PIT_COLS, "Attempt to spawn block out of bounds.");
	game_assert(!at(rc), "Attempt to spawn block at occupied location.");

	auto block = std::make_unique<Block>(color, rc, state);
	Block* raw_block = block.get();
	fill_area(*raw_block);

	m_contents.push_back(std::move(block));

	if(rc.r < m_peak)
		m_peak = rc.r;

	return *raw_block;
}

Garbage& Pit::spawn_garbage(RowCol rc, int width, int height)
{
	// make sure the Garbage fits in the Pit
	game_assert(rc.c >= 0 && rc.c + width <= PIT_COLS, "Attempt to spawn garbage out of bounds.");

	auto garbage = std::make_unique<Garbage>(rc, width, height);
	Garbage* raw_garbage = garbage.get();
	fill_area(*raw_garbage);

	m_contents.push_back(std::move(garbage));

	if(rc.r < m_peak)
		m_peak = rc.r;

	return *raw_garbage;
}

bool Pit::can_fall(Physical& physical) const noexcept
{
	RowCol rc = physical.rc();

	// there must be no obstacle at the target location and the
	// target location must be a valid location in the pit
	RowCol to {rc.r + physical.rows(), rc.c};

	for(int c = to.c; c < to.c + physical.columns(); c++) {
		RowCol target{to.r, c};

		if(at(target))
			return false;
	}

	return true;
}

void Pit::fall(Physical& physical)
{
	Block* block = dynamic_cast<Block*>(&physical);
	Garbage* garbage = dynamic_cast<Garbage*>(&physical);

	if(block) fall_block(*block);
	else if(garbage) fall_garbage(*garbage);
	else game_assert(false, "Attempt to fall unknown object.");

	refresh_peak();
}

void Pit::swap(Block& left, Block& right) noexcept
{
	RowCol lrc = left.rc();
	RowCol rrc = right.rc();

	auto left_entry = m_content_map.find(lrc);
	auto right_entry = m_content_map.find(rrc);
	auto end = m_content_map.end();

	game_assert(left_entry != end && right_entry != end, "Attempt to swap nonexistant blocks.");

	left.set_rc(rrc);
	right.set_rc(lrc);
	std::swap(left_entry->second, right_entry->second);

	// To enable skill chains, the chaining marker stays with the falling block
	std::swap(left.chaining, right.chaining);
}

void Pit::remove_dead()
{
	bool did_erase = false;

	for(auto it = m_contents.begin(); it != m_contents.end(); ) {
		if(Physical::State::DEAD == (*it)->physical_state()) {
			clear_area(**it);
			did_erase = true;

			it = m_contents.erase(it);
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
		size_t erased = m_content_map.erase(RowCol{low, c});
		game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
	}

	// The garbage loses one row. If that is all, remove it entirely.
	if(garbage.shrink() <= 0) {
		auto is_gone = [] (PhysVec::reference ptr) { return ptr->rows() <= 0; };
		auto new_end = std::remove_if(m_contents.begin(), m_contents.end(), is_gone);
		m_contents.erase(new_end, m_contents.end());

		refresh_peak();
		return nullptr;
	}
	else {
		return &garbage;
	}
}

void Pit::clear()
{
	m_contents.clear();
	m_content_map.clear();
	m_peak = 1;
}

int Pit::top() const noexcept
{
	return std::ceil(float(m_scroll) / ROW_HEIGHT);
}

int Pit::bottom() const noexcept
{
	return std::floor(float(m_scroll) / ROW_HEIGHT) + PIT_ROWS - 1;
}

int Pit::peak() const noexcept
{
	return m_peak;
}

void Pit::highlight(int row) noexcept
{
	m_highlight_row = row;
}

Point Pit::transform(Point point, float dt) const noexcept
{
	point.x += m_loc.x;
	point.y += m_loc.y;
	// TODO: include dt in scroll anim, don’t forget FPS-TPS conversion
	point.y -= ROW_H * m_scroll / ROW_HEIGHT;
	return point;
}

void Pit::update()
{
	for(auto& p : m_contents)
		p->update();

	if(m_enabled)
		m_scroll += m_speed;
}

void Pit::refresh_peak() noexcept
{
	// maintain peak by linear search through the pit contents
	int lowest_row = this->bottom();

	while(m_peak < lowest_row) {
		for(int c = 0; c < PIT_COLS; c++) {
			if(this->at({m_peak, c}))
				return;
		}

		m_peak++; // try next row
	}
}

void Pit::fall_block(Block& block)
{
	RowCol rc = block.rc();
	RowCol to { rc.r+1, rc.c };

	game_assert(!at(to), "Attempt to move block to occupied location.");

	auto erased = m_content_map.erase(rc);
	game_assert(1 == erased, "Block not found at expected space in Pit.");
	auto emplace_result = m_content_map.emplace(to, &block);
	game_assert(emplace_result.second, "Attempt to block already blocked space in Pit.");
	block.set_rc(to);
}

void Pit::fall_garbage(Garbage& garbage)
{
	RowCol rc = garbage.rc();
	RowCol to { rc.r+1, rc.c };

	clear_area(garbage);
	garbage.set_rc(to);
	fill_area(garbage);
}

void Pit::fill_area(Physical& physical)
{
	RowCol rc = physical.rc();

	for(int r = rc.r; r < rc.r + physical.rows(); r++) {
		for(int c = rc.c; c < rc.c + physical.columns(); c++) {
			RowCol target{r, c};
			auto result = m_content_map.emplace(std::make_pair(target, &physical));
			game_assert(result.second, "Attempt to block already blocked space in Pit.");
		}
	}
}

void Pit::clear_area(const Physical& physical)
{
	RowCol rc = physical.rc();

	for(int r = rc.r; r < rc.r + physical.rows(); r++) {
		for(int c = rc.c; c < rc.c + physical.columns(); c++) {
			RowCol target{r, c};
			size_t erased = m_content_map.erase(target);
			game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
		}
	}
}


void BonusIndicator::display_combo(int combo) noexcept
{
	m_combo = combo;
	m_combo_time = DISPLAY_TIME;
}

void BonusIndicator::display_chain(int chain) noexcept
{
	m_chain = chain;
	m_chain_time = DISPLAY_TIME;
}

void BonusIndicator::get_indication(int& combo, uint8_t& combo_fade, int& chain, uint8_t& chain_fade) const noexcept
{
	combo = m_combo;
	combo_fade = static_cast<uint8_t>(std::max(0, std::min(255, 255 + 255 * m_combo_time / FADE_TIME)));

	chain = m_chain;
	chain_fade = static_cast<uint8_t>(std::max(0, std::min(255, 255 + 255 * m_chain_time / FADE_TIME)));
}

void BonusIndicator::update() noexcept
{
	m_combo_time--;
	m_chain_time--;
}


Stage::PitCursor::PitCursor(Point loc, Point bonus_loc)
: pit(loc),
  cursor(RowCol{ -PIT_ROWS/2, PIT_COLS/2-1 }),
  bonus(bonus_loc)
{}

Stage::PitCursor& Stage::add_pit(Point loc, Point bonus_loc)
{
	m_pits.push_back(std::make_unique<PitCursor>(loc, bonus_loc));
	return *m_pits.back();
}

void Stage::update()
{
	for(auto& pc : m_pits) {
		pc->pit.update();
		pc->cursor.update();
		pc->bonus.update();
	}
}


std::unique_ptr<Stage> StageBuilder::construct()
{
	auto stage = std::make_unique<Stage>();

	auto& left_pc = stage->add_pit(LPIT_LOC, LBONUS_LOC);
	auto& right_pc = stage->add_pit(RPIT_LOC, RBONUS_LOC);

	this->left_pit = &left_pc.pit;
	this->left_cursor = &left_pc.cursor;
	this->left_bonus = &left_pc.bonus;

	this->right_pit = &right_pc.pit;
	this->right_cursor = &right_pc.cursor;
	this->right_bonus = &right_pc.bonus;

	return stage;
}
