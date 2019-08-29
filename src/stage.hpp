/**
 * Definitions for on-screen objects and presentation.
 */
#pragma once

#include "globals.hpp"
#include "event.hpp"

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
 * Stage is a container for on-screen objects.
 * Rather than with the gameplay aspects, the objects managed by the Stage
 * are concerned with the presentation of the game, e.g. turning events
 * emitted by the logic into display artifacts and sound effects.
 */
class Stage
{

public:

	explicit Stage(const GameState& state);
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

	const GameState& state() const { return *m_state; }
	SobVector& sobs() { return m_sobs; }
	const SobVector& sobs() const { return m_sobs; }

	Point m_shake{0,0}; //!< shake effect offset

private:

	const GameState* const m_state;
	SobVector m_sobs;
	evt::BonusRelay m_bonus_relay;
	evt::DupeFiltered<evt::SoundRelay> m_sound_relay;
	evt::DupeFiltered<evt::ShakeRelay> m_shake_relay;

};
