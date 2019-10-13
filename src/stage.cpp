#include "stage.hpp"
#include "state.hpp"
#include "draw.hpp"
#include "event.hpp"
#include "error.hpp"
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

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


IParticle::IParticle(const Point p, const float orientation,
	const float xspeed, const float yspeed, const float turn, const float gravity,
	const int ttl)
	: m_p(p), m_orientation(orientation), m_xspeed(xspeed), m_yspeed(yspeed),
	m_turn(turn), m_gravity(gravity), m_ttl(ttl)
{
	enforce(0 <= ttl);
}

void IParticle::update()
{
	enforce(0 < m_ttl);

	update_impl();

	m_p.x += m_xspeed;
	m_p.y += m_yspeed;
	m_orientation += m_turn;
	m_yspeed += m_gravity;
	m_ttl--;
}


SpriteParticle::SpriteParticle(const Point p, const float orientation,
	const float xspeed, const float yspeed, const float turn, const float gravity,
	const int ttl, const Gfx gfx)
	: IParticle(p, orientation, xspeed, yspeed, turn, gravity, ttl),
	m_gfx(gfx),
	m_frame(0)
{
	enforce(Gfx::PARTICLE == gfx); // as of right now, there is only one particle sprite available
}

void SpriteParticle::draw(const float dt, IDraw& draw) const
{
	draw.gfx_rotate(int(std::round(p().x)), int(std::round(p().y)), orientation(), m_gfx, m_frame);
}

void SpriteParticle::update_impl()
{
	m_frame++;

	if(PARTICLE_FRAMES <= m_frame)
		m_frame = 0;
}


TrailParticle::TrailParticle(const Point p, const float orientation,
	const float xspeed, const float yspeed, const float turn, const float gravity,
	const int ttl, const Palette palette)
	: IParticle(p, orientation, xspeed, yspeed, turn, gravity, ttl),
	m_palette(palette),
	m_trail(),
	m_length(0)
{
}

void TrailParticle::draw(const float dt, IDraw& draw) const
{
	Point p0 = p();

	for(int i = 0; i < m_length; i++) {
		const Point p1 = m_trail[i];

		for(int dx = -1; dx <= 1; dx++)
		for(int dy = -1; dy <= 1; dy++) {
			draw.line(int(std::round(p0.x))+dx, int(std::round(p0.y))+dy,
				int(std::round(p1.x))+dx, int(std::round(p1.y))+dy, m_palette[i]);
		}

		p0 = p1;
	}
}

void TrailParticle::update_impl()
{
	// shift m_trail
	std::copy(++m_trail.rbegin(), m_trail.rend(), m_trail.rbegin());
	m_trail[0] = p();

	if(m_length < TRAIL_PARTICLE_MAXLEN)
		m_length++;
}


ParticleGenerator::ParticleGenerator(const Point p, const int density, const float intensity, IDraw& draw)
	: m_p(p), m_density(density), m_intensity(intensity), m_draw(&draw)
{
}

void ParticleGenerator::trigger()
{
#define MY_PI           3.14159265358979323846f  /* pi */
	static std::minstd_rand generator;
	static std::uniform_real_distribution<float> orientation_distribution{ 0., 2.f * MY_PI };
	static std::uniform_real_distribution<float> spd_distribution{ 1.f, 5.f };
	static std::uniform_real_distribution<float> turn_distribution{ -.5f, .5f };

	for(int i = 0; i < m_density; i++) {
		const float orientation = orientation_distribution(generator);
		const float speed = spd_distribution(generator) * m_intensity;
		const float turn = turn_distribution(generator);
		const float gravity = .3f * m_intensity;
		const int ttl = 10;
		const float xspeed = std::cos(orientation) * speed;
		const float yspeed = std::sin(orientation) * speed;

		m_particles.push_back(std::make_unique<SpriteParticle>(m_p, orientation,
			xspeed, yspeed, turn, gravity, ttl, Gfx::PARTICLE));
	}
}

void ParticleGenerator::update()
{
	for(auto& particle : m_particles) {
		particle->update();
	}

	auto end = std::remove_if(m_particles.begin(), m_particles.end(), [](const auto& p) { return 0 >= p->ttl(); });
	m_particles.erase(end, m_particles.end());
}

void ParticleGenerator::draw(const float dt) const
{
	for(auto& particle : m_particles) {
		particle->draw(dt, *m_draw);
	}
}


DrawPit::DrawPit(IDraw& draw, float dt, Point shake, bool show_result,
                 bool debug_overlay, bool debug_highlight)
	:
	m_draw(&draw), m_dt(dt), m_shake(shake), m_show_result(show_result),
	m_debug_overlay(debug_overlay), m_debug_highlight(debug_highlight),
	m_pit(nullptr)
{
	enforce(0.f <= dt);
	enforce(1.f >= dt);
}

void DrawPit::run(const Pit& pit)
{
	m_pit = &pit;

	// restrict drawing area to pit
	m_draw->clip({ static_cast<int>(pit.loc().x), static_cast<int>(pit.loc().y), PIT_W, PIT_H });

	for(auto& physical : pit.contents()) {
		if(Block* b = dynamic_cast<Block*>(&*physical)) {
			block(*b);
		}
		else if(Garbage* g = dynamic_cast<Garbage*>(&*physical)) {
			garbage(*g);
		}
	}

	if(m_debug_overlay)
		debug_overlay();

	if(m_debug_highlight) {
		// draw the highlighted row for debugging
		Point top_left{0, static_cast<float>(pit.highlight_row() * ROW_H)};
		highlight(top_left, PIT_W, ROW_H, 200, 200, 0, 150);
	}

	if(!m_show_result) {
		cursor(pit.cursor());
	}

	m_draw->unclip(); // unrestrict drawing
}

void DrawPit::debug_overlay() const
{
	for(auto& physical : m_pit->contents()) {
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			Block::State state = block->block_state();
			size_t frame = 0;
			if(Block::State::FALL == state) frame = 1;
			if(Block::State::BREAK == state) frame = 2;
			if(Color::FAKE == block->col) frame = 3;
			Point loc = translate(block_loc(*block));
			m_draw->gfx(loc, Gfx::PITVIEW, frame);
		}
		else if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			Physical::State state = garbage->physical_state();
			size_t frame = 4;
			if(Physical::State::FALL == state) frame = 5;
			Point loc = translate(garbage_loc(*garbage));
			m_draw->gfx(loc, Gfx::PITVIEW, frame);
		}
	}
}

void DrawPit::block(const Block& block) const
{
	if(Color::FAKE == block.col) return;

	float time = block.eta();
	Block::State state = block.block_state();
	Gfx gfx = Gfx::BLOCK_BLUE + (block.col - Color::BLUE);
	BlockFrame frame = BlockFrame::REST;

	if(Block::State::PREVIEW == state) {
		frame = BlockFrame::PREVIEW;
	}

	if(Block::State::BREAK == state) {
		assert(time >= 0.f); // an expired breaking physical should be dead instead
		int begin = static_cast<int>(BlockFrame::BREAK_BEGIN);
		int end = static_cast<int>(BlockFrame::BREAK_END);
		frame = static_cast<BlockFrame>((size_t)begin + static_cast<size_t>(time) % ((size_t)end - begin));
		// TODO: use the following for single full break anim
		// frame = time * frames / (BLOCK_BREAK_TIME + 1);
	}

	Point bloc = block_loc(block);
	Point draw_loc = translate(bloc);
	m_draw->gfx(draw_loc, gfx, static_cast<size_t>(frame));

	if(block.chaining) {
		assert(time >= 0.f); // resting blocks should not be chaining
		uint8_t colv = 255 * int(time) % 2;
		highlight(bloc, BLOCK_W, BLOCK_H, colv, colv, colv, 150);
	}
}

void DrawPit::garbage(const Garbage& garbage) const
{
	Point draw_loc = translate(garbage_loc(garbage));
	float time = garbage.eta();
	size_t frame = 0;

	// Animation, for a garbage block, primarily means the part where it dissolves
	// and turns into small blocks.
	if(Physical::State::BREAK == garbage.physical_state()) {
		assert(time >= 0.f); // an expired breaking physical should be dead instead
		frame = 1 + static_cast<size_t>(time) % 5;
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

		m_draw->gfx(piece_loc, tile, frame);
	}

	// preview upcoming blocks from garbage dissolve
	if(Physical::State::BREAK == garbage.physical_state()) {
		const RowCol rc = RowCol{garbage.rc().r + garbage.rows() - 1, garbage.rc().c};
		auto loot_it = garbage.loot();

		for(int x = 0; x < garbage.columns() - garbage.eta() / 10; x++) {
			draw_loc = translate(from_rc(RowCol{rc.r, rc.c + x}));
			Gfx gfx = Gfx::BLOCK_BLUE + (*loot_it++ - Color::BLUE);
			m_draw->gfx(draw_loc, gfx, static_cast<size_t>(BlockFrame::REST));
		}
	}
}

void DrawPit::cursor(const Cursor& cursor) const
{
	RowCol rc = cursor.rc;
	float x = static_cast<float>(rc.c*COL_W - (CURSOR_W-2*COL_W)/2);
	float y = static_cast<float>(rc.r*ROW_H - (CURSOR_H-ROW_H)/2);
	Point loc = translate({x, y});

	size_t frame = (cursor.time / CURSOR_FRAME_TIME) % CURSOR_FRAMES;
	m_draw->gfx(loc, Gfx::CURSOR, frame);
}

void DrawPit::highlight(Point top_left, int width, int height,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
{
	Point loc = translate(top_left);
	m_draw->highlight({ static_cast<int>(loc.x), static_cast<int>(loc.y), width, height }, { r, g, b, a });
}

Point DrawPit::block_loc(const Block& block) const noexcept
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
				loc.y -= h * DrawPit::BLOCK_BOUNCE_H / LAND_TIME;
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

Point DrawPit::garbage_loc(const Garbage& garbage) const noexcept
{
	Point loc = from_rc(garbage.rc());

	if(Physical::State::FALL == garbage.physical_state()) {
		loc.y -= garbage.eta() * ROW_HEIGHT / FALL_SPEED;
	}

	return loc;
}

Point DrawPit::translate(Point p) const noexcept
{
	return m_pit->transform(p, m_dt).offset(m_shake.x, m_shake.y);
}


Stage::Stage(const GameState& state, IDraw& draw)
	:
	m_state(&state),
	m_draw(&draw),
	m_bonus_relay(*this),
	m_sound_relay(),
	m_shake_relay(*this)
{
	enforce(2 == state.pit().size()); // different player number not supported yet

	Point lbanner_loc{LPIT_LOC.offset((PIT_W - BANNER_W) / 2., (PIT_H - BANNER_H) / 2.)};
	m_sobs.push_back({Banner(lbanner_loc), BonusIndicator(LBONUS_LOC)});

	Point rbanner_loc{RPIT_LOC.offset((PIT_W - BANNER_W) / 2., (PIT_H - BANNER_H) / 2.)};
	m_sobs.push_back({Banner(rbanner_loc), BonusIndicator(RBONUS_LOC)});
}

void Stage::update()
{
	for(auto& sob : m_sobs)
		sob.bonus.update();

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

void Stage::draw(float dt) const
{
	enforce(dt >= 0.f);
	enforce(dt <= 1.f);

	draw_background();

	if(m_state) {
		DrawPit draw_pit{*m_draw, dt, m_shake, m_show_result, m_show_pit_debug_overlay, m_show_pit_debug_highlight};

		for(size_t i = 0; i < m_sobs.size(); ++i) {
			draw_pit.run(*m_state->pit()[i]);
			draw_bonus(m_sobs[i].bonus, dt);

			if(m_show_result) {
				draw_banner(m_sobs[i].banner, dt);
			}
		}
	}

	tint();
}

void Stage::fade(float black_fraction)
{
	m_black_fraction = black_fraction;
}

void Stage::show_result(int winner)
{
	enforce(0 <= winner);
	enforce(winner < m_sobs.size());

	// remember display mode
	m_show_result = true;

	// configure banner frames
	for(size_t i = 0; i < m_sobs.size(); i++) {
		BannerFrame frame = (i == winner) ? BannerFrame::WIN : BannerFrame::LOSE;
		m_sobs[i].banner.frame = frame;
	}
}

void Stage::toggle_pit_debug_overlay()
{
	m_show_pit_debug_overlay = !m_show_pit_debug_overlay;
}

void Stage::toggle_pit_debug_highlight()
{
	m_show_pit_debug_highlight = !m_show_pit_debug_highlight;
}

void Stage::shake(float strength) noexcept
{
	m_shake = m_shake.offset(0.f, strength);
}

void Stage::subscribe_to(evt::GameEventHub& hub)
{
	// The relays all have internal state (like tick counters) that we want to
	// reset at the start of a game. -> recreate them.
	m_bonus_relay = evt::BonusRelay{*this};
	m_sound_relay = evt::DupeFiltered<evt::SoundRelay>{};
	m_shake_relay = evt::DupeFiltered<evt::ShakeRelay>{*this};

	// BUG! Due to lack of RAII wrapping, these relays will not be properly
	// unsubscribed from the hub if this is being called from the GameScreen
	// constructor and one of the subscriptions throws an exception.
	hub.subscribe(m_bonus_relay);
	hub.subscribe(m_sound_relay);
	hub.subscribe(m_shake_relay);
}

void Stage::unsubscribe_from(evt::GameEventHub& hub)
{
	hub.unsubscribe(m_bonus_relay);
	hub.unsubscribe(m_sound_relay);
	hub.unsubscribe(m_shake_relay);
}

void Stage::draw_background() const
{
	m_draw->gfx(0, 0, Gfx::BACKGROUND);
}

void Stage::draw_bonus(const BonusIndicator& bonus, float dt) const
{
	Point origin = bonus.origin();

	int combo = 0;
	uint8_t combo_fade = 0;
	int chain = 0;
	uint8_t chain_fade = 0;

	bonus.get_indication(combo, combo_fade, chain, chain_fade);

	for(int i = 0; i < combo; i++) {
		Point star_loc = origin.offset(0, static_cast<float>(-BONUS_H * (i + 1)));
		m_draw->gfx(star_loc, Gfx::BONUS, static_cast<int>(BonusFrame::COMBO), combo_fade);
	}

	for(int i = 0; i < chain; i++) {
		Point star_loc = origin.offset(static_cast<float>(BONUS_W), static_cast<float>(-BONUS_H * (i + 1)));
		m_draw->gfx(star_loc, Gfx::BONUS, static_cast<int>(BonusFrame::CHAIN), chain_fade);
	}
}

void Stage::draw_banner(const Banner& banner, float dt) const
{
	Point loc = banner.loc;
	m_draw->gfx(loc, Gfx::BANNER, static_cast<size_t>(banner.frame));
}

void Stage::tint() const
{
	m_draw->rect({ 0, 0, CANVAS_W, CANVAS_H }, { 0, 0, 0, static_cast<uint8_t>(m_black_fraction * 255) });
}
