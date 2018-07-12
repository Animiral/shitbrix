#include "event.hpp"
#include "context.hpp"
#include "stage.hpp"
#include "audio.hpp"

namespace evt
{

void GameEventHub::subscribe(IEventObserver& handler)
{
	m_handlers.push_back(&handler);
}

void GameEventHub::unsubscribe(IEventObserver& handler)
{
	auto it = std::remove(m_handlers.begin(), m_handlers.end(), &handler);
	m_handlers.erase(it, m_handlers.end());
}


BonusRelay::BonusRelay(Stage& stage)
	: m_stage(&stage)
{}

void BonusRelay::fire(evt::Match event)
{
	if(event.combo > 3)
		m_stage->sobs().at(event.trivia.player).bonus.display_combo(event.combo);
}

void BonusRelay::fire(evt::Chain event)
{
	if(event.counter > 0)
		m_stage->sobs().at(event.trivia.player).bonus.display_chain(event.counter + 1);
}


void SoundRelay::fire(CursorMoves event)
{
}

void SoundRelay::fire(Swap event)
{
	the_context.audio->play(Snd::SWAP);
}

void SoundRelay::fire(Match event)
{
	the_context.audio->play(Snd::MATCH);
}

void SoundRelay::fire(PhysicalLands event)
{
	the_context.audio->play(Snd::LANDING);
}

void SoundRelay::fire(BlockDies event)
{
	the_context.audio->play(Snd::BREAK);
}

void SoundRelay::fire(GarbageDissolves event)
{
	the_context.audio->play(Snd::BREAK);
}

void ShakeRelay::fire(evt::PhysicalLands lands)
{
	if(const Garbage* garbage = dynamic_cast<const Garbage*> (&lands.physical)) {
		m_stage->shake(garbage->rows() * SHAKE_SCALE);
	}
}

}
