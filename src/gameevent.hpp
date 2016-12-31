/**
 * gameevent.hpp
 * Defines the IGameEvent interface and events through which director objects
 * communicate in-game occurrences to other modules.
 */
#pragma once

namespace evt
{

/**
 * Event that occurs when the cursor has been moved.
 */
struct CursorMoves {};

/**
 * Event that occurs when two blocks are beginning to swap places.
 */
struct Swap {};

/**
 * Event that occurs when a match, consisting of >=3 blocks, has occurred.
 */
struct Match
{
	int combo; //!< combo counter, >= 3
	int chain; //!< chain counter, >= 1
};

/**
 * Event that occurs when a block has finished breaking and will be removed.
 */
struct BlockDies {};

/**
 * Event that occurs when a block of garbage has finished breaking and is going to
 * shrink or disappear.
 */
struct GarbageDissolves {};

/**
 * Interface for transmission of game event information.
 * Game logic routines in director.cpp sample/notice the event and fire it by
 * calling one of the overloads of fire() with the type of event that occurred.
 * Different modules implement event handlers by inheritance from this interface.
 * The default implementation is not to do anything with the event.
 */
class IGameEvent
{

public:

	virtual ~IGameEvent() =0;

	/**
	 * Signal that the cursor has been moved.
	 */
	virtual void fire(CursorMoves moved) {}

	/**
	 * Signal that two blocks are beginning to swap places.
	 */
	virtual void fire(Swap swapped) {}

	/**
	 * Signal that a match, consisting of >=3 blocks, has occurred.
	 */
	virtual void fire(Match matched) {}

	/**
	 * Signal that a block has finished breaking and will be removed.
	 */
	virtual void fire(BlockDies died) {}

	/**
	 * Signal that a block of garbage has finished breaking and is going to
	 * shrink or disappear.
	 */
	virtual void fire(GarbageDissolves dissolved) {}

};

inline IGameEvent::~IGameEvent() {}

/**
 * A pseudo-handler for GameEvents that forwards them to other handlers.
 */
class GameEventHub : public IGameEvent
{

public:

	void append(IGameEvent& event)
	{
		m_handlers.push_back(event);
	}

	virtual void fire(CursorMoves event) override { fire_all(event); }
	virtual void fire(Swap event) override { fire_all(event); }
	virtual void fire(Match event) override { fire_all(event); }
	virtual void fire(BlockDies event) override { fire_all(event); }
	virtual void fire(GarbageDissolves event) override { fire_all(event); }

private:

	template<typename Event>
	void fire_all(Event event)
	{
		for(auto& handler : m_handlers)
			handler.get().fire(event);
	}

	std::vector<std::reference_wrapper<IGameEvent>> m_handlers;

};

}
