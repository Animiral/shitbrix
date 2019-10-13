/**
 * Definitions for on-screen objects and presentation.
 */
#pragma once

#include "globals.hpp"
#include "event.hpp"
#include "text.hpp"

class IDraw;

enum class BannerFrame : size_t { WIN=0, LOSE=1 };

class Banner
{

public:

	Banner(Point loc) : loc(loc), frame(BannerFrame::LOSE) {}

	Point loc;
	BannerFrame frame;

};

/**
 * The BonusIndicator is an on-screen display element that showcases recent
 * gameplay events for one player.
 * It displays a red star for every block in the last combo and a gold star
 * for every chaining match in the last chain.
 * The bonus is indicated for DISPLAY_TIME ticks and fades after another FADE_TIME
 * ticks. A more recent bonus can interrupt the display and cause it to
 * display the newer value.
 * The BonusIndicator does not itself draw to the screen. It mainly keeps
 * track of timing.
 */
class BonusIndicator
{

public:

	/**
	 * The origin of the BonusIndicator is its bottom left point.
	 */
	BonusIndicator(Point origin) noexcept
	: m_origin(origin), m_combo_time(0), m_chain_time(0),
	  m_combo(0), m_chain(0) {}

	void display_combo(int combo) noexcept;
	void display_chain(int chain) noexcept;

	Point origin() const noexcept { return m_origin; }

	/**
	 * Retrieve the indicator values for combos and chains.
	 * @param[out] combo combo intensity (number of stars displayed)
	 * @param[out] combo_fade draw alpha value (255: full, 0: nothing)
	 * @param[out] chain chain intensity (number of stars displayed)
	 * @param[out] chain_fade draw alpha value (255: full, 0: nothing)
	 */
	void get_indication(int& combo, uint8_t& combo_fade, int& chain, uint8_t& chain_fade) const noexcept;

	void update() noexcept;

	static const int DISPLAY_TIME = 40;
	static const int FADE_TIME = 15;

private:

	Point m_origin;
	int m_combo_time; //!< positive: show combo; negative: fade combo
	int m_chain_time; //!< positive: show chain; negative: fade chain
	int m_combo;
	int m_chain;

};

/**
 * Base class for different types of particles.
 *
 * Particles are short-lived objects that support liveness and eye candy.
 */
class IParticle
{

public:

	/**
	 * Construct the IParticle using the specified movement.
	 */
	explicit IParticle(Point p, float orientation, float xspeed, float yspeed, float turn, float gravity, int ttl);
	IParticle(const IParticle&) noexcept = default;
	IParticle(IParticle&&) noexcept = default;
	IParticle& operator=(const IParticle&) noexcept = default;
	IParticle& operator=(IParticle&&) noexcept = default;

	virtual ~IParticle() noexcept = default;

	Point p() const noexcept { return m_p; }
	float orientation() const noexcept { return m_orientation; }
	int ttl() const noexcept { return m_ttl; }

	/**
	 * Update the particle according to its movement properties.
	 *
	 * @throw EnforceException if the particle is expired (ttl=0)
	 */
	void update();

	/**
	 * Draw the particle to the screen.
	 */
	virtual void draw(float dt, IDraw& draw) const = 0;

protected:

	/**
	 * Derived-specific update action.
	 */
	virtual void update_impl() {}

private:

	Point m_p; //!< position
	float m_orientation; //!< heading of the graphic
	float m_xspeed, m_yspeed; //!< delta per tick (independent of orientation)
	float m_turn, m_gravity; //!< turning effect on orientation and accelerating effect on yspeed
	int m_ttl; //!< time to live

};

/**
 * A SpriteParticle is a type of particle that is shown using a bitmap animation.
 */
class SpriteParticle : public IParticle
{

public:

	/**
	 * Construct the SpriteParticle using the specified movement and gfx spritesheet.
	 */
	explicit SpriteParticle(Point p, float orientation, float xspeed, float yspeed,
		float turn, float gravity, int ttl, Gfx gfx);
	SpriteParticle(const SpriteParticle&) noexcept = default;
	SpriteParticle(SpriteParticle&&) noexcept = default;
	SpriteParticle& operator=(const SpriteParticle&) noexcept = default;
	SpriteParticle& operator=(SpriteParticle&&) noexcept = default;

	virtual ~SpriteParticle() noexcept = default;

	virtual void draw(float dt, IDraw& draw) const override;

protected:

	/**
	 * Derived-specific update action.
	 */
	virtual void update_impl() override;

private:

	Gfx m_gfx; //!< display graphics id
	size_t m_frame; //!< animation frame counter

};

/**
 * A TrailParticle is a type of particle drawn using colored lines,
 * like a spark flying from a candle.
 */
class TrailParticle : public IParticle
{

public:

	using Palette = std::array<wrap::Color, TRAIL_PARTICLE_MAXLEN>;

	/**
	 * Construct the SpriteParticle using the specified movement and gfx spritesheet.
	 */
	explicit TrailParticle(Point p, float orientation, float xspeed, float yspeed,
		float turn, float gravity, int ttl, Palette palette);
	TrailParticle(const TrailParticle&) noexcept = default;
	TrailParticle(TrailParticle&&) noexcept = default;
	TrailParticle& operator=(const TrailParticle&) noexcept = default;
	TrailParticle& operator=(TrailParticle&&) noexcept = default;

	virtual ~TrailParticle() noexcept = default;

	size_t length() const noexcept { return m_length; }

	virtual void draw(float dt, IDraw& draw) const override;

protected:

	/**
	 * Derived-specific update action.
	 */
	virtual void update_impl() override;

private:

	std::array<Point, TRAIL_PARTICLE_MAXLEN> m_trail; //!< memory of points visited
	Palette m_palette; //!< colors of the particle trail
	size_t m_length; //!< length of the particle trail

};

/**
 * A source and container of particles which are spawned sprinkling with some
 * random properties.
 */
class ParticleGenerator
{

public:

	explicit ParticleGenerator(Point p, int density, float intensity, IDraw& draw);

	void set_p(Point p) noexcept { m_p = p; }

	/**
	 * Generate more particles according to the generator's density.
	 */
	void trigger();

	/**
	 * Run update logic on all contained particles.
	 */
	void update();

	/**
	 * Draw all contained particles to the screen.
	 */
	void draw(float dt) const;

private:

	Point m_p; //!< spawn location of all generated particles
	int m_density; //!< number of particles spawned per trigger
	float m_intensity; //!< influences speed, gravity and ttl
	IDraw* m_draw; //!< drawing object

	std::vector<std::unique_ptr<IParticle>> m_particles;

};

/**
 * This helper class only draws the contents of Pits to the screen.
 */
class DrawPit
{

public:

	explicit DrawPit(IDraw& draw, float dt, Point shake, bool show_result,
		bool m_debug_overlay = false, bool m_debug_highlight = false);

	/**
	 * Draw the given pit.
	 */
	void run(const Pit& pit);

private:

	IDraw* m_draw; //!< draw object
	float m_dt; //!< fraction since start of logic tick
	Point m_shake{0,0}; //!< shake effect offset
	bool m_show_result; //!< when false, draw the cursor (ingame) - when true, draw the banners
	bool m_debug_overlay = false; //!< whether to draw the pit debug overlay or not
	bool m_debug_highlight = false; //!< whether to draw the pit debug highlight or not
	mutable const Pit* m_pit; //!< shortcut instead of argument passing

	void debug_overlay() const;
	void block(const Block& block) const;

	/**
	 * Draw the garbage brick.
	 *
	 * While a Garbage’s rc is always set to point at the lower left space that
	 * it occupies, its loc points to the top left corner of the displayed array
	 * of graphics.
	 */
	void garbage(const Garbage& garbage) const;
	void cursor(const Cursor& cursor) const;
	void highlight(Point top_left, int width, int height,
	               uint8_t r, uint8_t g, uint8_t b, uint8_t a) const;

	Point block_loc(const Block& block) const noexcept;
	Point garbage_loc(const Garbage& garbage) const noexcept;
	Point translate(Point p) const noexcept;

	// animation constants
	static constexpr float BLOCK_BOUNCE_H = 10.f; // height of a block’s bouncing animation when it lands
	static constexpr int CURSOR_FRAME_TIME = 4; // how many sceen frames to display one cursor frame
	static constexpr int CURSOR_FRAMES = 4; // number of available cursor frames
};

/**
 * Stage is a container for on-screen objects.
 * Rather than with the gameplay aspects, the objects managed by the Stage
 * are concerned with the presentation of the game, e.g. turning events
 * emitted by the logic into display artifacts and sound effects.
 */
class Stage
{

public:

	explicit Stage(const GameState& state, IDraw& draw);
	Stage(const Stage& ) =delete;

	/**
	 * Helper struct for stage contents (per player).
	 * These objects must also be updated regularly,
	 * but independently of the game logic.
	 */
	struct StageObjects
	{
		Banner banner;
		BonusIndicator bonus;
	};

	using SobVector = std::vector<StageObjects>;

	void update();

	/**
	 * Draw all game-related content to the screen.
	 */
	void draw(float dt) const;

	/**
	 * Set the intensity of black for the fade-in blend effect.
	 */
	void fade(float black_fraction);

	/**
	 * Change displayed information to the result configuration, with win/lose banners.
	 */
	void show_result(int winner);

	/**
	 * Show or hide the debug info on the pits.
	 */
	void toggle_pit_debug_overlay();

	/**
	 * Show or hide the debug highlight on the pits.
	 */
	void toggle_pit_debug_highlight();

	/**
	 * Nudge the game screen to make everything shake. Cumulative effect.
	 */
	void shake(float strength) noexcept;

	/**
	 * Subscribe stage event handlers to the given event hub.
	 */
	void subscribe_to(evt::GameEventHub& hub);

	/**
	 * Unsubscribe stage event handlers from the given event hub.
	 */
	void unsubscribe_from(evt::GameEventHub& hub);

	void set_state(const GameState* state) { m_state = state; }
	const GameState& state() const { return *m_state; }
	SobVector& sobs() { return m_sobs; }
	const SobVector& sobs() const { return m_sobs; }

	Point m_shake{0,0}; //!< shake effect offset

private:

	const GameState* m_state;
	IDraw* m_draw;
	SobVector m_sobs;
	evt::BonusRelay m_bonus_relay;
	evt::DupeFiltered<evt::SoundRelay> m_sound_relay;
	evt::DupeFiltered<evt::ShakeRelay> m_shake_relay;

	bool m_show_result = false; //!< when false, draw the cursor (ingame) - when true, draw the banners
	bool m_show_pit_debug_overlay = false; //!< whether to draw the pit debug overlay or not
	bool m_show_pit_debug_highlight = false; //!< whether to draw the pit debug highlight or not

	float m_black_fraction = 1.f; //!< fade fraction for the intro phase fade-in blend effect
	Point m_pitloc{0,0}; //!< point location of the current pit, translate sprites
	uint8_t m_alpha = 255;

	// drawing implementation routines
	void draw_background() const;
	void draw_bonus(const BonusIndicator& bonus, float dt) const;
	void draw_banner(const Banner& banner, float dt) const;

	/**
	 * Apply the configured m_fade value to the screen.
	 */
	void tint() const;

};
