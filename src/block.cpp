/**
 * Definitions for block and stage objects
 */

#include "block.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>
#include <functional>

int operator-(BlockCol lhs, BlockCol rhs)
{
	return static_cast<int>(lhs) - static_cast<int>(rhs);
}

void BlockImpl::draw(IContext& context, float dt)
{
	SDL_assert(col != BlockCol::INVALID);

	if(BlockCol::FAKE == col) return;

	Point draw_loc = m_loc;

	// bounce when landing
	if(BlockState::LAND == m_state) {
		// TODO: include dt in landing anim, don’t forget FPS-TPS conversion
		draw_loc.y -= BOUNCE_H * ( time > LAND_TIME/2 ? LAND_TIME-time : time ) / LAND_TIME;
	}

	Gfx gfx = Gfx::BLOCK_BLUE + (col - BlockCol::BLUE);

	BlockFrame frame = BlockFrame::REST;
	if(BlockState::PREVIEW == m_state) frame = BlockFrame::PREVIEW;
	if(BlockState::BREAK == m_state) frame = m_anim;

	context.drawGfx(draw_loc, gfx, static_cast<size_t>(frame));
}

void BlockImpl::animate()
{
	++m_anim;

	if(BlockState::BREAK == m_state && m_anim >= BlockFrame::BREAK_END)
		m_anim = BlockFrame::BREAK_BEGIN;
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


/**
 * Draw the garbage brick.
 * While a Garbage’s rc is always set to point at the lower left space that
 * it occupies, its loc points to the top left corner of the displayed array
 * of graphics.
 */
void Garbage::draw(IContext& context, float dt)
{

	for(int y = 0; y < m_rows*2; y++)
	for(int x = 0; x < m_columns*2; x++) {
		Point piece_loc = { m_loc.x + x*GARBAGE_W, m_loc.y + y*GARBAGE_H };
		GarbageFrame frame = GarbageFrame::MID;

		bool top = 0 == y;
		bool low = m_rows*2 == y+1;
		bool left = 0 == x;
		bool right = m_columns*2 == x+1;

		if(top && left)       frame = GarbageFrame::TOP_LEFT;
		else if(top && right) frame = GarbageFrame::TOP_RIGHT;
		else if(top)          frame = GarbageFrame::TOP;
		else if(low && left)  frame = GarbageFrame::LOW_LEFT;
		else if(low && right) frame = GarbageFrame::LOW_RIGHT;
		else if(low)          frame = GarbageFrame::LOW;
		else if(left)         frame = GarbageFrame::MID_LEFT;
		else if(right)        frame = GarbageFrame::MID_RIGHT;
		else                  frame = GarbageFrame::MID;

		context.drawGfx(piece_loc, Gfx::GARBAGE, static_cast<size_t>(frame));
	}
}

/**
 * Animation, for a garbage block, primarily means the part where it dissolves
 * and turns into small blocks.
 */
void Garbage::animate()
{
	if(State::DISSOLVE == m_state) {
		// TODO: animate garbage block
	}
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
PitImpl::PitImpl(Point loc)
: IAnimation(PIT_Z), m_loc(loc), m_enabled(true), m_scroll(ROW_H - PIT_H),
  m_peak(1), m_highlight_row(0)
{
}

/**
 * Returns the number of the top accessible row in the pit
 */
int PitImpl::top() const
{
	return std::ceil(m_scroll / ROW_H);
}

/**
 * Returns the number of the bottom accessible row in the pit
 */
int PitImpl::bottom() const
{
	return std::floor((m_scroll + PIT_H) / ROW_H) - 1;
}

int PitImpl::peak() const
{
	return m_peak;
}

/**
 * Return the block at the given location.
 */
Block PitImpl::block_at(RowCol rc) const
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
GarbagePtr PitImpl::garbage_at(RowCol rc) const
{
	auto it = m_garbage_map.find(rc);
	if(it == m_garbage_map.end())
		return nullptr;
	else
		return it->second;
}

bool PitImpl::anything_at(RowCol rc) const
{
	return
		block_map.find(rc) != block_map.end() || 
		m_garbage_map.find(rc) != m_garbage_map.end();
}

GarbagePtr PitImpl::spawn_garbage(int columns, int rows)
{
	int row = std::min(m_peak, top()) - 2;
	// int col = (*rndgen)() % (PIT_COLS - columns + 1);
	int col = 0;
	GarbagePtr garbage = std::make_shared<Garbage>(RowCol{row, col}, columns, rows);
	m_garbage.push_back(garbage);
	block(garbage);
	return garbage;
}

/**
 * Set the given location to blocked.
 */
void PitImpl::block(RowCol rc, Block block)
{
	auto result = block_map.emplace(std::make_pair(rc, block));
	game_assert(result.second, "Attempt to block already blocked space in Pit.");

	if(rc.r < m_peak)
		m_peak = rc.r;
}

/**
 * Set the given location to blocked by garbage.
 */
void PitImpl::block(GarbagePtr garbage)
{
	RowCol low_left = garbage->rc();
	int low = low_left.r;
	int high = low - garbage->rows();
	int left = low_left.c;
	int right = left + garbage->columns();

	for(int r = low; r > high; r--) {
		for(int c = left; c < right; c++) {
			RowCol rc{r, c};
			auto result = m_garbage_map.emplace(std::make_pair(rc, garbage));
			game_assert(result.second, "Attempt to block already blocked space in Pit.");
		}
	}

	if(high < m_peak)
		m_peak = high;
}

/**
 * Set the given location to not blocked.
 */
void PitImpl::unblock(RowCol rc)
{
	size_t erased = block_map.erase(rc);
	game_assert(1 == erased, "Attempt to unblock empty space in Pit.");

	// maintain peak by linear search through the pit contents, if necessary
	if(rc.r == m_peak) {
		int lowest_row = this->bottom();

		while(m_peak < lowest_row) {
			for(int c = 0; c < PIT_COLS; c++) {
				if(this->anything_at({m_peak, c}))
					goto peak_done;
			}

			m_peak++; // try next row
		}
	}

peak_done:;
}

/**
 * Set the given location to not blocked.
 */
void PitImpl::unblock(GarbagePtr garbage)
{
	RowCol low_left = garbage->rc();
	int low = low_left.r;
	int high = low - garbage->rows();
	int left = low_left.c;
	int right = left + garbage->columns();

	for(int r = low; r > high; r--) {
		for(int c = left; c < right; c++) {
			RowCol rc{r, c};
			size_t erased = m_garbage_map.erase(rc);
			game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
		}
	}

	// maintain peak by linear search through the pit contents, if necessary
	if(low >= m_peak) {
		int lowest_row = this->bottom();

		while(m_peak < lowest_row) {
			for(int c = 0; c < PIT_COLS; c++) {
				if(this->anything_at({m_peak, c}))
					goto peak_done;
			}

			m_peak++; // try next row
		}
	}

peak_done:;
}

/**
 * Exchanges the blocks at lrc and rrc, including the absence of blocks.
 */
void PitImpl::swap(RowCol lrc, RowCol rrc)
{
	auto left = block_map.find(lrc);
	auto right = block_map.find(rrc);
	auto end = block_map.end();

	if(left != end && right != end) {
		std::swap(left->second, right->second);
	}
	else if(left != end) {
		block_map.emplace(std::make_pair(rrc, std::move(left->second)));
		block_map.erase(lrc);
	}
	else if(right != end) {
		block_map.emplace(std::make_pair(lrc, std::move(right->second)));
		block_map.erase(rrc);
	}
}

void PitImpl::highlight(int row)
{
	m_highlight_row = row;
}

/**
 * The origin {0,0} location of all pit-related objects corresponds with row 0, column 0.
 * We have to transform the object into the pit and from there, apply the pit scrolling.
 */
Point PitImpl::transform(Point point, float dt) const
{
	point.x += m_loc.x;
	point.y += m_loc.y;
	// TODO: include dt in scroll anim, don’t forget FPS-TPS conversion
	point.y -= m_scroll;
	return point;
}

void PitImpl::draw(IContext& context, float dt)
{
	context.clip(m_loc, PIT_W, PIT_H);
	context.translate(m_loc.offset(0, -m_scroll));

	for(auto b: m_blocks)
		b->draw(context, dt);

	for(auto g: m_garbage)
		g->draw(context, dt);

	// draw the highlighted row for debugging
	Point top_left{0, static_cast<float>(m_highlight_row * ROW_H)};
	top_left = transform(top_left); // apply pit offset/scrolling
	context.highlight(top_left, PIT_W, ROW_H);

	context.translate(Point{0,0});
	context.unclip();
}

void PitImpl::animate()
{
	for(auto b : m_blocks)
		b->animate();

	for(auto g : m_garbage)
		g->animate();
}

void PitImpl::update(IContext& context)
{
	for(auto b : m_blocks)
		b->update(context);

	for(auto g : m_garbage)
		g->update(context);

	if(m_enabled)
		m_scroll += SCROLL_SPEED;
}


void CursorImpl::draw(IContext& context, float dt)
{
	float x = static_cast<float>(rc.c*COL_W - (CURSOR_W-2*COL_W)/2);
	float y = static_cast<float>(rc.r*ROW_H - (CURSOR_H-ROW_H)/2);
	Point loc {x, y};

	size_t frame = (anim / FRAME_TIME) % FRAMES;
	context.drawGfx(loc, Gfx::CURSOR, frame);
}

void CursorImpl::animate()
{
	anim++;
}


void BannerImpl::draw(IContext& context, float dt)
{
	context.drawGfx(loc, Gfx::BANNER, static_cast<size_t>(frame));
}


void StageImpl::add(Animation animation)
{
	// insertion sort - animations in the list are always in ascending z_order
	ordered_insert(animations, animation, z_less);
}

void StageImpl::add(Logic logic)
{
	logics.push_back(logic);
}

void StageImpl::remove(Animation animation)
{
	auto it = std::find(animations.begin(), animations.end(), animation);
	SDL_assert(it != animations.end());
	animations.erase(it);
}

void StageImpl::remove(Logic logic)
{
	auto it = std::find(logics.begin(), logics.end(), logic);
	SDL_assert(it != logics.end());
	logics.erase(it);
}

void StageImpl::draw(IContext& context, float dt)
{
	for(auto& animation : animations) animation->draw(context, dt);
}

void StageImpl::animate()
{
	for(auto& animation : animations) animation->animate();
}

void StageImpl::update(IContext& context)
{
	for(auto& logic : logics) logic->update(context);
}

Stage StageBuilder::construct()
{
	Stage stage = std::make_shared<StageImpl>();

	left_pit = std::make_shared<PitImpl>(LPIT_LOC);
	right_pit = std::make_shared<PitImpl>(RPIT_LOC);

	RowCol left_center { (left_pit->top()-left_pit->bottom())/2, PIT_COLS/2-1 };
	left_cursor = std::make_shared<CursorImpl>(left_center);

	RowCol right_center { (right_pit->top()-right_pit->bottom())/2, PIT_COLS/2-1 };
	right_cursor = std::make_shared<CursorImpl>(right_center);

	stage->add(static_cast<Animation>(left_pit));
	stage->add(static_cast<Logic>(left_pit));
	stage->add(static_cast<Animation>(right_pit));
	stage->add(static_cast<Logic>(right_pit));
	stage->add(left_cursor);
	stage->add(right_cursor);

	return stage;
}
