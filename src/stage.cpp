/**
 * Definitions for stage objects
 */

#include "stage.hpp"
#include "error.hpp"
#include <algorithm>
#include <functional>

Physical::Physical(RowCol rc, State state)
: m_rc(rc),
  m_state(state),
  m_time(1),
  m_speed(1),
  m_tag(TAG_NONE)
{
	// exclude locations that are well-known to lie out of bounds
	enforce(rc.c >= 0 && rc.c < PIT_COLS);
}

float Physical::eta() const noexcept
{
	return float(m_time) / m_speed;
}

bool Physical::is_arriving() const noexcept
{
	// Physical states are generally time-based.
	return m_time <= 0 && m_time > -m_speed;
}

bool Physical::is_fallible() const noexcept
{
	return State::REST == m_state || State::LAND == m_state; // && !has_tag(TAG_FALL);
}

void Physical::update()
{
	enforce(State::DEAD != m_state);

	m_time -= m_speed;
	update_impl();

	if(State::LAND == m_state) {
		if(is_arriving()) {
			set_state(State::REST);
		}
	}
}

void Physical::set_state(State state, int time, int speed) noexcept
{
	enforce(m_state != State::DEAD); // cannot change out of dead state
	enforce(time >= 1); // state must last at least one tick
	enforce(speed >= 1); // time must run out, not in

	set_state_impl(state, time, speed);

	m_state = state;
	m_time = time;
	m_speed = speed;
}

void Physical::continue_state(int time_bonus) noexcept
{
	// The bonus must be large enough to prime the object for another arrival
	enforce(m_time + time_bonus > 0);

	m_time += time_bonus;
}


Block::Block(Color col, RowCol rc, State state)
: Physical(rc, static_cast<Physical::State>(state)),
  col(col),
  chaining(false),
  m_anim(BlockFrame::REST)
{}

void Block::set_state(Physical::State state, int time, int speed) noexcept
{
	Physical::set_state(state, time, speed);
}

void Block::set_state(State state, int time, int speed) noexcept
{
	Physical::set_state(static_cast<Physical::State>(state), time, speed);
}

bool Block::is_swappable() const noexcept
{
	State state = block_state();

	return State::REST == state ||
	       State::FALL == state ||
	       State::LAND == state ||
	       State::SWAP_LEFT == state ||
	       State::SWAP_RIGHT == state;
}

bool Block::is_matchable() const noexcept
{
	State state = block_state();
	return State::REST == state || State::LAND == state;
}

void Block::update_impl()
{
	if(State::BREAK == block_state() && is_arriving()) {
		set_state(Physical::State::DEAD);
	}
}

void Block::set_state_impl(Physical::State state, int, int) noexcept
{
	enforce(State::PREVIEW != static_cast<State>(state));

	if(Physical::State::BREAK == state) {
		m_anim = BlockFrame::BREAK_BEGIN;
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


Garbage::Garbage(RowCol rc, int columns, int rows, Loot loot)
	: Physical(rc, State::REST),
	m_columns(columns),
	m_rows(rows),
	m_loot(loot)
{
	enforce(columns > 0);
	enforce(rows > 0);
	enforce(loot.size() == columns * rows);
}

Loot::const_iterator Garbage::loot() const
{
	enforce(m_rows > 0);
	return m_loot.begin();
}

int Garbage::shrink() noexcept
{
	enforce(m_rows > 0);
	m_loot.erase(m_loot.begin(), m_loot.begin() + m_columns);
	--m_rows;
	enforce(m_loot.size() == m_columns * m_rows);
	return m_rows;
}


IBlocksQueue::~IBlocksQueue() = default;


RandomBlocksQueue::RandomBlocksQueue(unsigned seed)
	: m_record(), m_generator(seed), m_index(0)
{
}

Block::Color RandomBlocksQueue::next() noexcept
{
	if(m_record.size() <= m_index) {
		std::uniform_int_distribution<int> color_distribution { 1, 6 };
		Block::Color color = static_cast<Block::Color>(color_distribution(m_generator));
		m_record.push_back(color);
		m_index++;
		return color;
	}
	else {
		return m_record[m_index++];
	}
}

void RandomBlocksQueue::backtrack(size_t index) noexcept
{
	m_index = index;
}


Pit::Pit(Point loc, std::unique_ptr<IBlocksQueue> grow_queue, std::unique_ptr<IBlocksQueue> emerge_queue) noexcept
: m_loc(loc),
  m_cursor{RowCol{ -PIT_ROWS/2, PIT_COLS/2-1 }, 0},
  m_enabled(true),
  m_scroll((1-PIT_ROWS) * ROW_HEIGHT),
  m_speed(SCROLL_SPEED),
  m_peak(1),
  m_chain(0),
  m_recovery(0),
  m_panic(PANIC_TIME),
  m_grow_queue(move(grow_queue)),
  m_emerge_queue(move(emerge_queue)),
  m_highlight_row(0)
{
}

Pit::Pit(const Pit& rhs)
: m_grow_queue(rhs.m_grow_queue->clone()),
  m_emerge_queue(rhs.m_emerge_queue->clone()),
  m_contents(rhs.copy_contents())
{
	assign_basic(rhs);
	make_content_map();
}

Pit& Pit::operator=(const Pit& rhs)
{
	assign_basic(rhs);
	m_grow_queue = rhs.m_grow_queue->clone();
	m_emerge_queue = rhs.m_emerge_queue->clone();
	m_contents = rhs.copy_contents();
	make_content_map();
	return *this;
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

bool Pit::is_full() const noexcept
{
	auto is_above = [t = top()] (const PhysVec::value_type& p)
	{
		return Physical::State::REST == p->physical_state() && p->rc().r < t;
	};

	return m_contents.end() != std::find_if(m_contents.begin(), m_contents.end(), is_above);
}

Block& Pit::spawn_block(Block::Color color, RowCol rc, Block::State state)
{
	enforce(rc.c >= 0);
	enforce(rc.c < PIT_COLS);

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
	enforce(rc.c >= 0);
	enforce(rc.c + width <= PIT_COLS);

	Loot loot(width * height);
	for(Block::Color& c : loot)
		c = m_emerge_queue->next();

	auto garbage = std::make_unique<Garbage>(rc, width, height, move(loot));
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
	else assert(false); // unknown type of object

	refresh_peak();
}

void Pit::swap(Block& left, Block& right)
{
	RowCol lrc = left.rc();
	RowCol rrc = right.rc();

	auto left_entry = m_content_map.find(lrc);
	auto right_entry = m_content_map.find(rrc);
	auto end = m_content_map.end();

	// sanity checks: blocks must exist where the content map remembers them
	if(left_entry == end || left_entry->second != &left ||
	   right_entry == end || right_entry->second != &right) {
		throw LogicException("Pit: Blocks to be swapped are not recognized and might be foreign.");
	}

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

void Pit::untag_all() noexcept
{
	for(auto& physical : m_contents)
		physical->clear_tags();
}

Garbage* Pit::shrink(Garbage& garbage)
{
	RowCol rc = garbage.rc();
	int low = rc.r + garbage.rows() - 1;

	for(int c = rc.c; c < rc.c + garbage.columns(); c++) {
		size_t erased = m_content_map.erase(RowCol{low, c});
		assert(1 == erased); // sanity check: this space must have been previously occupied
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

void Pit::cursor_move(Dir dir) noexcept
{
	enforce(Dir::NONE != dir);

	switch(dir)
	{
		case Dir::LEFT:  if(m_cursor.rc.c > 0)          m_cursor.rc.c--; break;
		case Dir::RIGHT: if(m_cursor.rc.c < PIT_COLS-2) m_cursor.rc.c++; break;
		case Dir::UP:    if(m_cursor.rc.r > top())      m_cursor.rc.r--; break;
		case Dir::DOWN:  if(m_cursor.rc.r < bottom())   m_cursor.rc.r++; break;
		default: assert(false);
	}
}

int Pit::top() const noexcept
{
	return static_cast<int>(std::ceil(float(m_scroll) / ROW_HEIGHT));
}

int Pit::bottom() const noexcept
{
	return static_cast<int>(std::floor(float(m_scroll) / ROW_HEIGHT)) + PIT_ROWS - 1;
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

	// keep cursor in accessible bounds at all times
	while(m_cursor.rc.r < top())
		m_cursor.rc.r++;

	m_cursor.time++;
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

	if(at(to))
		throw LogicException("Pit: Attempt to move block to occupied location.");

	auto erased = m_content_map.erase(rc);
	assert(1 == erased); // sanity check: this space must have been previously occupied
	auto emplace_result = m_content_map.emplace(to, &block);
	assert(emplace_result.second); // sanity check: this space must be free to place a block in
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

			if(!result.second)
				throw LogicException("Pit: Attempt to block already blocked space.");
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
			assert(1 == erased); // sanity check: this space must have been previously occupied
		}
	}
}

void Pit::assign_basic(const Pit& rhs)
{
	m_loc = rhs.m_loc;
	m_cursor = rhs.m_cursor;
	m_enabled = rhs.m_enabled;
	m_scroll = rhs.m_scroll;
	m_speed = rhs.m_speed;
	m_peak = rhs.m_peak;
	m_chain = rhs.m_chain;
	m_recovery = rhs.m_recovery;
	m_panic = rhs.m_panic;
	m_highlight_row = rhs.m_highlight_row;
}

Pit::PhysVec Pit::copy_contents() const
{
	PhysVec copy;

	for(const auto& physical : m_contents) {
		copy.emplace_back(physical->clone());
	}

	return copy;
}

void Pit::make_content_map()
{
	assert(m_content_map.empty()); // leftover content map

	for(const auto& physical : m_contents)
		fill_area(*physical);
}


namespace
{

/**
 * Return the draw location of the Pit with the index, given the number of total players.
 * This is only a placeholder for a more general layout function that can layout all
 * on-screen elements for all players.
 */
Point layout_pit(int players, int index)
{
	enforce(2 == players); // different player number not supported yet

	return index <= 0 ? LPIT_LOC : RPIT_LOC;
}

}

GameState::GameState(GameMeta meta)
: m_game_time(0)
{
	static const RowCol CURSOR_DEFAULT{-PIT_ROWS / 2, PIT_COLS / 2 - 1};

	for(int i = 0; i < meta.players; i++)
	{
		Point loc = layout_pit(meta.players, i);
		std::unique_ptr<RandomBlocksQueue> spawn_queue = std::make_unique<RandomBlocksQueue>(meta.seed * (i + 1));
		std::unique_ptr<RandomBlocksQueue> emerge_queue = std::make_unique<RandomBlocksQueue>(meta.seed * (i+1) * 3 + 5);
		m_pit.push_back(std::make_unique<Pit>(loc, move(spawn_queue), move(emerge_queue)));
	}
}

GameState::GameState(const GameState& rhs)
: m_game_time(rhs.m_game_time)
{
	for(const auto& pit : rhs.m_pit)
		m_pit.push_back(std::make_unique<Pit>(*pit));
}

GameState& GameState::operator=(GameState rhs) noexcept
{
	m_game_time = rhs.m_game_time;
	swap(m_pit, rhs.m_pit);
	return *this;
}

void GameState::update()
{
	for(const auto& pit : m_pit)
		pit->update();

	m_game_time++;
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


Stage::Stage(GameState init)
	: m_state(std::move(init))
{
	enforce(2 == m_state.pit().size()); // different player number not supported yet

	Point lbanner_loc{LPIT_LOC.offset((PIT_W - BANNER_W) / 2., (PIT_H - BANNER_H) / 2.)};
	m_sobs.push_back({Banner(lbanner_loc), BonusIndicator(LBONUS_LOC)});

	Point rbanner_loc{RPIT_LOC.offset((PIT_W - BANNER_W) / 2., (PIT_H - BANNER_H) / 2.)};
	m_sobs.push_back({Banner(rbanner_loc), BonusIndicator(RBONUS_LOC)});
}

void Stage::update()
{
	m_state.update();

	for(auto& sob : m_sobs)
		sob.bonus.update();
}
