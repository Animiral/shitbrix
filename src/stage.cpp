#include "stage.hpp"
#include "state.hpp"
#include "event.hpp"
#include "error.hpp"

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


Stage::Stage(const GameState& state) :
	m_bonus_relay(*this),
	m_sound_relay(),
	m_shake_relay(*this),
	m_state(&state)
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
