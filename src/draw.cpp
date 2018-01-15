/**
 * Definitions for drawing routines.
 */
#include "draw.hpp"
#include "globals.hpp"
#include <cmath>
#include <SDL2/SDL.h>

/**
 * Module-internal implementations.
 */
namespace
{

Point block_loc(const Block& block);
Point garbage_loc(const Garbage& garbage);
void clip(SDL_Renderer* renderer, Point top_left, int width, int height);
void unclip(SDL_Renderer* renderer);

}

void IDraw::draw(float dt) const
{
	draw_offscreen(dt);

	SDL_Renderer* renderer = &sdl.renderer();
	SDL_RenderPresent(renderer);

	// clear for next frame
	int render_result = SDL_RenderClear(renderer);
	game_assert(0 == render_result, SDL_GetError());
}

DrawMenu::DrawMenu(const Assets& assets)
: m_assets(assets)
{
}

void DrawMenu::draw_offscreen(float) const
{
	SDL_Texture* texture = &m_assets.texture(Gfx::MENUBG, 0);
	SDL_Rect dstrect { 0, 0, 0, 0 };
	int query_result = SDL_QueryTexture(texture, nullptr, nullptr, &dstrect.w, &dstrect.h);
	game_assert(0 == query_result, SDL_GetError());

/*
	int alpha_result = SDL_SetTextureAlphaMod(tex, 255);
	game_assert(0 == alpha_result, SDL_GetError());
*/
	int render_result = SDL_RenderCopy(&sdl.renderer(), texture, nullptr, &dstrect);
	game_assert(0 == render_result, SDL_GetError());
}

DrawGame::DrawGame(const Assets& assets)
: m_show_cursor(false),
  m_show_banner(false),
  m_show_pit_debug_overlay(false),
  m_assets(assets)
{
	SDL_Renderer* renderer = &sdl.renderer();
	m_fadetex.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 1, 1)); // 1x1 pixel for fading
	game_assert(bool(m_fadetex), SDL_GetError());

	int texblend_result = SDL_SetTextureBlendMode(m_fadetex.get(), SDL_BLENDMODE_BLEND);
	game_assert(0 == texblend_result, SDL_GetError());

	int drawblend_result = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
	game_assert(0 == drawblend_result, SDL_GetError());
}

void DrawGame::add_pit(const Pit& pit, const Cursor& cursor,
					   const Banner& banner, const BonusIndicator& indicator)
{
	m_drawables.emplace_back(PlayerDrawables{pit, cursor, banner, indicator});
}

void DrawGame::clear()
{
	m_drawables.clear();
	m_show_cursor = false;
	m_show_banner = false;
}

void DrawGame::fade(float fraction)
{
	m_fade = fraction;
}

void DrawGame::shake(float strength) noexcept
{
	m_shake = m_shake.offset(0.f, strength);
}

void DrawGame::draw_offscreen(float dt) const
{
	SDL_assert(dt >= 0.f);
	SDL_assert(dt <= 1.f);

	draw_background();

	SDL_Renderer* renderer = &sdl.renderer();

	for(const PlayerDrawables& drawable : m_drawables) {
		const Pit& pit = drawable.pit;
		clip(renderer, pit.loc(), PIT_W, PIT_H); // restrict drawing area to pit
		m_pitloc = pit.transform(Point{0,0}); // draw all pit objects relative to pit origin

		draw_pit(pit, dt);

		if(m_show_pit_debug_overlay)
			draw_pit_debug_overlay(pit);

		if(m_show_pit_debug_highlight) {
			// draw the highlighted row for debugging
			Point top_left{0, static_cast<float>(pit.highlight_row() * ROW_H)};
			draw_highlight(top_left, PIT_W, ROW_H, 200, 200, 0, 150);
		}

		if(m_show_cursor)
			draw_cursor(drawable.cursor, dt);

		m_pitloc = Point{0,0}; // reset to screen origin
		unclip(renderer); // unrestrict drawing

		if(m_show_banner)
			draw_banner(drawable.banner, dt);

		draw_bonus(drawable.indicator, dt);
	}

	tint();

	// update shake for next frame
	// shake consists of:
	// * invert the shake translation offset (rotate 180°)
	// * downscale the effect
	// * flavor it with a slight rotation, given by the rotation matrix R(theta)
	constexpr float theta = float(M_PI) / 2.f + .1f; // constant rotation per fame
	const Point prev = m_shake;
	m_shake.x = SHAKE_DECREASE * (prev.x * std::cos(theta) - prev.y * std::sin(theta));
	m_shake.y = SHAKE_DECREASE * (prev.x * std::sin(theta) + prev.y * std::cos(theta));
}

void DrawGame::show_cursor(bool show)
{
	m_show_cursor = show;
}

void DrawGame::show_banner(bool show)
{
	m_show_banner = show;
}

void DrawGame::toggle_pit_debug_overlay()
{
	m_show_pit_debug_overlay = !m_show_pit_debug_overlay;
}

void DrawGame::toggle_pit_debug_highlight()
{
	m_show_pit_debug_highlight = !m_show_pit_debug_highlight;
}

Point DrawGame::translate(Point p) const noexcept
{
	return p.offset(m_pitloc.x, m_pitloc.y).offset(m_shake.x, m_shake.y);
}

void DrawGame::draw_background() const
{
	putsprite(Point{0,0}, Gfx::BACKGROUND);
}

void DrawGame::draw_pit(const Pit& pit, float dt) const
{
	for(auto& physical : pit.contents()) {
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			draw_block(*block, dt);
		}
		else if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			draw_garbage(*garbage, dt);
		}
	}
}

void DrawGame::draw_pit_debug_overlay(const Pit& pit) const
{
	for(auto& physical : pit.contents()) {
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			Block::State state = block->block_state();
			size_t frame = 0;
			if(Block::State::FALL == state) frame = 1;
			if(Block::State::BREAK == state) frame = 2;
			if(Block::Color::FAKE == block->col) frame = 3;
			putsprite(block_loc(*block), Gfx::PITVIEW, frame);
		}
		else if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			Physical::State state = garbage->physical_state();
			size_t frame = 4;
			if(Physical::State::FALL == state) frame = 5;
			putsprite(garbage_loc(*garbage), Gfx::PITVIEW, frame);
		}
	}
}

void DrawGame::draw_block(const Block& block, float dt) const
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
	putsprite(draw_loc, gfx, static_cast<size_t>(frame));

	if(block.chaining) {
		uint8_t colv = 255 * int(time) % 2;
		draw_highlight(draw_loc, BLOCK_W, BLOCK_H, colv, colv, colv, 150);
	}
}

/**
 * Draw the garbage brick.
 * While a Garbage’s rc is always set to point at the lower left space that
 * it occupies, its loc points to the top left corner of the displayed array
 * of graphics.
 */
void DrawGame::draw_garbage(const Garbage& garbage, float dt) const
{
	Point draw_loc = garbage_loc(garbage);
	float time = garbage.eta();
	size_t frame = 0;

	// Animation, for a garbage block, primarily means the part where it dissolves
	// and turns into small blocks.
	if(Physical::State::BREAK == garbage.physical_state()) {
		SDL_assert(time >= 0.f);
		frame = static_cast<size_t>(1 + int(time) % 5);
		// TODO: use the following for single full break anim
		// frame = time * frames / (GARBAGE_BREAK_TIME + 1);
	}

	for(int y = 0; y < garbage.rows()*2; y++)
	for(int x = 0; x < garbage.columns()*2; x++) {
		Point piece_loc = { draw_loc.x + x*GARBAGE_W, draw_loc.y + y*GARBAGE_H };
		Gfx tile = Gfx::GARBAGE_M;

		bool top = 0 == y;
		bool low = garbage.rows()*2 == y+1;
		bool left = 0 == x;
		bool right = garbage.columns()*2 == x+1;

		if(top && left)       tile = Gfx::GARBAGE_LU;
		else if(top && right) tile = Gfx::GARBAGE_RU;
		else if(top)          tile = Gfx::GARBAGE_U;
		else if(low && left)  tile = Gfx::GARBAGE_LD;
		else if(low && right) tile = Gfx::GARBAGE_RD;
		else if(low)          tile = Gfx::GARBAGE_D;
		else if(left)         tile = Gfx::GARBAGE_L;
		else if(right)        tile = Gfx::GARBAGE_R;
		else                  tile = Gfx::GARBAGE_M;

		putsprite(piece_loc, tile, frame);
	}
}

void DrawGame::draw_cursor(const Cursor& cursor, float dt) const
{
	RowCol rc = cursor.rc;
	float x = static_cast<float>(rc.c*COL_W - (CURSOR_W-2*COL_W)/2);
	float y = static_cast<float>(rc.r*ROW_H - (CURSOR_H-ROW_H)/2);
	Point loc {x, y};

	size_t frame = (cursor.time / DrawGame::CURSOR_FRAME_TIME) % DrawGame::CURSOR_FRAMES;
	putsprite(loc, Gfx::CURSOR, frame);
}

void DrawGame::draw_banner(const Banner& banner, float dt) const
{
	putsprite(banner.loc, Gfx::BANNER, static_cast<size_t>(banner.frame));
}

void DrawGame::draw_bonus(const BonusIndicator& bonus, float dt) const
{
	Point origin = bonus.origin();

	int combo = 0;
	uint8_t combo_fade = 0;
	int chain = 0;
	uint8_t chain_fade = 0;

	bonus.get_indication(combo, combo_fade, chain, chain_fade);

	m_alpha = combo_fade;

	for(int i = 0; i < combo; i++) {
		Point star_loc = origin.offset(0, static_cast<float>(-BONUS_H * (i + 1)));
		putsprite(star_loc, Gfx::BONUS, static_cast<int>(BonusFrame::COMBO));
	}

	m_alpha = chain_fade;

	for(int i = 0; i < chain; i++) {
		Point star_loc = origin.offset(static_cast<float>(BONUS_W),
									   static_cast<float>(-BONUS_H * (i + 1)));
		putsprite(star_loc, Gfx::BONUS, static_cast<int>(BonusFrame::CHAIN));
	}

	m_alpha = 255;
}

void DrawGame::draw_highlight(Point top_left, int width, int height,
	                          uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
{
	Point loc = translate(top_left);
	SDL_Rect fill_rect {
		static_cast<int>(loc.x),
		static_cast<int>(loc.y),
		width,
		height
	};

	SDL_Renderer* renderer = &sdl.renderer();
	int color_result = SDL_SetRenderDrawColor(renderer, r, g, b, a);
	game_assert(0 == color_result, SDL_GetError());
	int fill_result = SDL_RenderFillRect(renderer, &fill_rect);
	game_assert(0 == fill_result, SDL_GetError());
}

void DrawGame::putsprite(Point loc, Gfx gfx, size_t frame) const
{
	SDL_Texture* texture = &m_assets.texture(gfx, frame);
	loc = translate(loc);
	SDL_Rect dstrect { std::lround(loc.x), std::lround(loc.y), 0, 0 };
	int query_result = SDL_QueryTexture(texture, nullptr, nullptr, &dstrect.w, &dstrect.h);
	game_assert(0 == query_result, SDL_GetError());

	int alpha_result = SDL_SetTextureAlphaMod(texture, m_alpha);
	game_assert(0 == alpha_result, SDL_GetError());

	int render_result = SDL_RenderCopy(&sdl.renderer(), texture, nullptr, &dstrect);
	game_assert(0 == render_result, SDL_GetError());
}

void DrawGame::tint() const
{
	if(m_fade < 1.f) {
		SDL_Rect rect_pixel{0,0,1,1};
		uint32_t fade_pixel = static_cast<uint32_t>(0xff * (1.f - m_fade));
		int tex_result = SDL_UpdateTexture(m_fadetex.get(), &rect_pixel, &fade_pixel, 1);
		game_assert(0 == tex_result, SDL_GetError());

		SDL_Renderer* renderer = &sdl.renderer();
		int render_result = SDL_RenderCopy(renderer, m_fadetex.get(), nullptr, nullptr);
		game_assert(0 == render_result, SDL_GetError());
	}
}

DrawTransition::DrawTransition(const IDraw& pred_draw, const IDraw& succ_draw)
: m_pred_draw(pred_draw),
  m_succ_draw(succ_draw),
  m_pred_texture(sdl.create_target_texture()),
  m_succ_texture(sdl.create_target_texture())
{
}

void DrawTransition::draw_offscreen(float dt) const
{
	SDL_Renderer* renderer = &sdl.renderer();

	int sdl_result = SDL_SetRenderTarget(renderer, m_pred_texture.get());
	game_assert(0 == sdl_result, SDL_GetError());
	m_pred_draw.draw_offscreen(dt);

	sdl_result = SDL_SetRenderTarget(renderer, m_succ_texture.get());
	game_assert(0 == sdl_result, SDL_GetError());
	m_succ_draw.draw_offscreen(dt);

	sdl_result = SDL_SetRenderTarget(renderer, nullptr);
	game_assert(0 == sdl_result, SDL_GetError());

	// draw to back buffer
	int progress_px = CANVAS_W * m_time / TRANSITION_TIME;
	SDL_Rect left_rect{ 0, 0, progress_px, CANVAS_H };
	SDL_Rect right_rect{ progress_px, 0, CANVAS_W-progress_px, CANVAS_H };

	// swipe transition: successor screen enters from the left.
	sdl_result = SDL_RenderCopy(renderer, m_succ_texture.get(), &left_rect, &left_rect);
	game_assert(0 == sdl_result, SDL_GetError());

	sdl_result = SDL_RenderCopy(renderer, m_pred_texture.get(), &right_rect, &right_rect);
	game_assert(0 == sdl_result, SDL_GetError());
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

void clip(SDL_Renderer* renderer, Point top_left, int width, int height)
{
	int x = static_cast<int>(top_left.x);
	int y = static_cast<int>(top_left.y);
	SDL_Rect clip_rect{x, y, width, height};

	int clip_result = SDL_RenderSetClipRect(renderer, &clip_rect);
	game_assert(0 == clip_result, SDL_GetError());
}

void unclip(SDL_Renderer* renderer)
{
	int clip_result = SDL_RenderSetClipRect(renderer, nullptr);
	game_assert(0 == clip_result, SDL_GetError());
}

}
