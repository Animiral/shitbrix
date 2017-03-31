/**
 * Definitions for drawing routines.
 */
#include "draw.hpp"
#include "globals.hpp"
#include <SDL2/SDL.h>

/**
 * Module-internal implementations.
 */
namespace
{

Point block_loc(const Block& block);
Point garbage_loc(const Garbage& garbage);

void draw_pit(const IContext& context, const Pit& pit, float dt);
void draw_pit_debug_overlay(const IContext& context, const Pit& pit);
void draw_block(const IContext& context, const Block& block, float dt);
void draw_garbage(const IContext& context, const Garbage& garbage, float dt);
void draw_cursor(const IContext& context, const Cursor& cursor, float dt);
void draw_bonus(IContext& context, const BonusIndicator& bonus, float dt);

}


DrawGame::DrawGame(IContext& context)
: m_context(context), m_dt(0.), m_show_cursors(false), m_show_pit_debug_overlay(false)
{
}

void DrawGame::add_pit(const Pit& pit, const Cursor& cursor, BonusIndicator& indicator)
{
	m_drawables.emplace_back(PlayerDrawables{pit, cursor, indicator});
}

void DrawGame::clear()
{
	m_drawables.clear();
	m_dt = 0.f;
	m_show_cursors = false;
}

float DrawGame::dt() const
{
	return m_dt;
}

void DrawGame::set_dt(float dt) const
{
	SDL_assert(dt >= 0.f);
	SDL_assert(dt <= 1.f);

	m_dt = dt;
}

void DrawGame::draw_all() const
{
	for(const PlayerDrawables& drawable : m_drawables) {
		const Pit& pit = drawable.pit;

		m_context.clip(pit.loc(), PIT_W, PIT_H); // restrict drawing area to pit
		m_context.translate(pit.transform(Point{0,0})); // draw all objects relative to pit origin

		draw_pit(m_context, pit, m_dt);

		if(m_show_pit_debug_overlay)
			draw_pit_debug_overlay(m_context, pit);

		if(m_show_pit_debug_highlight) {
			// draw the highlighted row for debugging
			Point top_left{0, static_cast<float>(pit.highlight_row() * ROW_H)};
			m_context.highlight(top_left, PIT_W, ROW_H, 200, 200, 0, 150);
		}

		if(m_show_cursors)
			draw_cursor(m_context, drawable.cursor, m_dt);

		m_context.translate(Point{0,0}); // reset to screen origin
		m_context.unclip(); // unrestrict drawing

		draw_bonus(m_context, drawable.indicator, m_dt);
	}
}

void DrawGame::show_cursors(bool show)
{
	m_show_cursors = show;
}

void DrawGame::toggle_pit_debug_overlay()
{
	m_show_pit_debug_overlay = !m_show_pit_debug_overlay;
}

void DrawGame::toggle_pit_debug_highlight()
{
	m_show_pit_debug_highlight = !m_show_pit_debug_highlight;
}


namespace
{

Point block_loc(const Block& block)
{
	Point loc = from_rc(block.rc());
	float eta = block.eta();

	switch(block.block_state()) {
		case Block::State::FALL:
			loc.y -= eta * ROW_HEIGHT / FALL_SPEED;
			break;

		case Block::State::LAND:
			{
				float h = eta > LAND_TIME/2 ? LAND_TIME-eta : eta;
				loc.y -= h * DrawGame::BLOCK_BOUNCE_H / LAND_TIME;
			}
			break;

		case Block::State::SWAP_LEFT:
			loc.x += eta * COL_W / SWAP_TIME ;
			break;

		case Block::State::SWAP_RIGHT:
			loc.x -= eta * COL_W / SWAP_TIME ;
			break;

		default:
			break;
	}

	return loc;
}

Point garbage_loc(const Garbage& garbage)
{
	Point loc = from_rc(garbage.rc());

	if(Physical::State::FALL == garbage.physical_state()) {
		loc.y -= garbage.eta() * ROW_HEIGHT / FALL_SPEED;
	}

	return loc;
}

void draw_pit(const IContext& context, const Pit& pit, float dt)
{
	for(auto& physical : pit.contents()) {
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			draw_block(context, *block, dt);
		}
		else if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			draw_garbage(context, *garbage, dt);
		}
	}
}

void draw_pit_debug_overlay(const IContext& context, const Pit& pit)
{
	for(auto& physical : pit.contents()) {
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			Block::State state = block->block_state();
			size_t frame = 0;
			if(Block::State::FALL == state) frame = 1;
			if(Block::State::BREAK == state) frame = 2;
			if(Block::Color::FAKE == block->col) frame = 3;
			context.drawGfx(block_loc(*block), Gfx::PITVIEW, frame);
		}
		else if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			Physical::State state = garbage->physical_state();
			size_t frame = 4;
			if(Physical::State::FALL == state) frame = 5;
			context.drawGfx(garbage_loc(*garbage), Gfx::PITVIEW, frame);
		}
	}
}

void draw_block(const IContext& context, const Block& block, float dt)
{
	if(Block::Color::FAKE == block.col) return;

	float time = block.eta();
	Block::State state = block.block_state();
	Gfx gfx = Gfx::BLOCK_BLUE + (block.col - Block::Color::BLUE);
	BlockFrame frame = BlockFrame::REST;

	if(Block::State::PREVIEW == state) {
		frame = BlockFrame::PREVIEW;
	}

	if(Block::State::BREAK == state) {
		SDL_assert(time >= 0.f);
		int begin = static_cast<int>(BlockFrame::BREAK_BEGIN);
		int end = static_cast<int>(BlockFrame::BREAK_END);
		frame = static_cast<BlockFrame>(begin + int(time) % (end - begin));
		// TODO: use the following for single full break anim
		// frame = time * frames / (BLOCK_BREAK_TIME + 1);
	}

	Point draw_loc = block_loc(block);
	context.drawGfx(draw_loc, gfx, static_cast<size_t>(frame));

	if(block.chaining) {
		uint8_t colv = 255 * int(time) % 2;
		context.highlight(draw_loc, BLOCK_W, BLOCK_H, colv, colv, colv, 150);
	}
}

/**
 * Draw the garbage brick.
 * While a Garbage’s rc is always set to point at the lower left space that
 * it occupies, its loc points to the top left corner of the displayed array
 * of graphics.
 */
void draw_garbage(const IContext& context, const Garbage& garbage, float dt)
{
	Point draw_loc = garbage_loc(garbage);

	// Animation, for a garbage block, primarily means the part where it dissolves
	// and turns into small blocks.
	if(Physical::State::BREAK == garbage.physical_state()) {
		// TODO: animate garbage block
	}

	for(int y = 0; y < garbage.rows()*2; y++)
	for(int x = 0; x < garbage.columns()*2; x++) {
		Point piece_loc = { draw_loc.x + x*GARBAGE_W, draw_loc.y + y*GARBAGE_H };
		GarbageFrame frame = GarbageFrame::MID;

		bool top = 0 == y;
		bool low = garbage.rows()*2 == y+1;
		bool left = 0 == x;
		bool right = garbage.columns()*2 == x+1;

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

void draw_cursor(const IContext& context, const Cursor& cursor, float dt)
{
	RowCol rc = cursor.rc;
	float x = static_cast<float>(rc.c*COL_W - (CURSOR_W-2*COL_W)/2);
	float y = static_cast<float>(rc.r*ROW_H - (CURSOR_H-ROW_H)/2);
	Point loc {x, y};

	size_t frame = (cursor.time / DrawGame::CURSOR_FRAME_TIME) % DrawGame::CURSOR_FRAMES;
	context.drawGfx(loc, Gfx::CURSOR, frame);
}

void draw_bonus(IContext& context, const BonusIndicator& bonus, float dt)
{
	Point origin = bonus.origin();

	int combo = 0;
	uint8_t combo_fade = 0;
	int chain = 0;
	uint8_t chain_fade = 0;

	bonus.get_indication(combo, combo_fade, chain, chain_fade);

	context.set_alpha(combo_fade);

	for(int i = 0; i < combo; i++) {
		Point star_loc = origin.offset(0, static_cast<float>(-BONUS_H * (i + 1)));
		context.drawGfx(star_loc, Gfx::BONUS, static_cast<int>(BonusFrame::COMBO));
	}

	context.set_alpha(chain_fade);

	for(int i = 0; i < chain; i++) {
		Point star_loc = origin.offset(static_cast<float>(BONUS_W),
									   static_cast<float>(-BONUS_H * (i + 1)));
		context.drawGfx(star_loc, Gfx::BONUS, static_cast<int>(BonusFrame::CHAIN));
	}

	context.set_alpha(255);
}

}
