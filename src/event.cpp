/**
 * Definitions for event classes.
 */

#include "event.hpp"

namespace evt
{

void GameEventHub::append(IGameEvent& event)
{
	m_handlers.push_back(event);
}

void GameEventHub::fire(CursorMoves event) { fire_all(event); }
void GameEventHub::fire(Swap event) { fire_all(event); }
void GameEventHub::fire(Match event) { fire_all(event); }
void GameEventHub::fire(Chain event) { fire_all(event); }
void GameEventHub::fire(BlockDies event) { fire_all(event); }
void GameEventHub::fire(GarbageDissolves event) { fire_all(event); }

template<typename Event>
void GameEventHub::fire_all(Event event)
{
	for(auto& handler : m_handlers)
		handler.get().fire(event);
}


SoundEffects::SoundEffects()
: m_context(nullptr)
{}

void SoundEffects::fire(Swap event)
{
	game_assert(m_context, "missing context for sounds");
	m_context->play(Snd::SWAP);
}

void SoundEffects::fire(Match event)
{
	game_assert(m_context, "missing context for sounds");
	m_context->play(Snd::MATCH);
}

void SoundEffects::fire(BlockDies event)
{
	game_assert(m_context, "missing context for sounds");
	m_context->play(Snd::BREAK);
}

void SoundEffects::fire(GarbageDissolves event)
{
	game_assert(m_context, "missing context for sounds");
	m_context->play(Snd::BREAK);
}

}
