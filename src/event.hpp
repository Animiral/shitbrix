/**
 * gameevent.hpp
 * Defines the IEventObserver interface and events through which director objects
 * communicate in-game occurrences to other modules.
 */
#pragma once

#include <algorithm>
#include "stage.hpp"

namespace evt
{

/**
 * Holds the data fields common to all types of events.
 */
struct Trivia
{
	long game_time; //!< time of the game state in which the event happened
	int player; //!< index of the player associated with the event
};

/**
 * Event that occurs when the cursor has been moved.
 */
struct CursorMoves
{
	Trivia trivia; //!< common information
};

/**
 * Event that occurs when two blocks are beginning to swap places.
 */
struct Swap
{
	Trivia trivia; //!< common information
};

/**
 * Event that occurs when a match, consisting of >=3 blocks, has occurred.
 */
struct Match
{
	Trivia trivia; //!< common information
	int combo; //!< combo counter, >= 3
	bool chaining; //!< chain indicator: whether a chaining block was involved
};

/**
 * Event that occurs when a chain has finished.
 * A chain is finished when no blocks are chaining (all of them have come to rest).
 * Even a single match causes a chain event, albeit with a counter of 0.
 */
struct Chain
{
	Trivia trivia; //!< common information
	int counter; //!< chain counter: how many chaining matches there were
};

/**
 * Event that occurs when a physical has finished falling down and lands on something below.
 */
struct PhysicalLands
{
	Trivia trivia; //!< common information
	const Physical& physical;
};

/**
 * Event that occurs when a block has finished breaking and will be removed.
 */
struct BlockDies
{
	Trivia trivia; //!< common information
};

/**
 * Event that occurs when a block of garbage has finished breaking and is going to
 * shrink or disappear.
 */
struct GarbageDissolves
{
	Trivia trivia; //!< common information
	long game_time;
};

/**
 * Event that occurs when a game round ends.
 */
struct GameOver
{
	Trivia trivia; //!< common information
};

/**
 * Interface for transmission of game event information.
 * Game logic routines in director.cpp produce the event and notify this by
 * calling one of the overloads of fire() with the type of event that occurred.
 * Different modules implement event handlers by inheritance from this interface.
 * The default implementation is not to do anything with the event.
 */
class IEventObserver
{

public:

	virtual ~IEventObserver() =0;

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
	 * Signal that a chain has finished.
	 */
	virtual void fire(Chain chained) {}

	/**
	 * Signal that a physical object has arrived from falling down.
	 */
	virtual void fire(PhysicalLands lands) {}

	/**
	 * Signal that a block has finished breaking and will be removed.
	 */
	virtual void fire(BlockDies died) {}

	/**
	 * Signal that a block of garbage has finished breaking and is going to
	 * shrink or disappear.
	 */
	virtual void fire(GarbageDissolves dissolved) {}

	/**
	 * Signal that the game is ending.
	 */
	virtual void fire(GameOver ended) {}

};

inline IEventObserver::~IEventObserver() {}

/**
 * A pseudo-handler for game events that forwards them to other handlers.
 */
class GameEventHub : public IEventObserver
{

public:

	void subscribe(IEventObserver& handler);
	void unsubscribe(IEventObserver& handler);

	virtual void fire(CursorMoves event) override { fire_all(event); }
	virtual void fire(Swap event) override { fire_all(event); }
	virtual void fire(Match event) override { fire_all(event); }
	virtual void fire(Chain event) override { fire_all(event); }
	virtual void fire(PhysicalLands event) override { fire_all(event); }
	virtual void fire(BlockDies event) override { fire_all(event); }
	virtual void fire(GarbageDissolves event) override { fire_all(event); }

private:

	template<typename Event>
	void fire_all(Event event)
	{
		for(auto handler : m_handlers)
			handler->fire(event);
	}

	std::vector<IEventObserver*> m_handlers;

};

/**
 * A pseudo-handler for game events that forwards them to a
 * subsequent handler only in a strictly ascending order of game_time.
 * It suppresses all late or repeat events.
 * This behavior filters events from re-calculation of game state.
 */
template<typename EventObserver>
class DupeFiltered : public IEventObserver
{

public:

	template<typename... Args>
	explicit DupeFiltered(Args&&... args) : m_next(std::forward<Args>(args)...) {}

	virtual void fire(CursorMoves event) override { fire_next(event); }
	virtual void fire(Swap event) override { fire_next(event); }
	virtual void fire(Match event) override { fire_next(event); }
	virtual void fire(Chain event) override { fire_next(event); }
	virtual void fire(PhysicalLands event) override { fire_next(event); }
	virtual void fire(BlockDies event) override { fire_next(event); }
	virtual void fire(GarbageDissolves event) override { fire_next(event); }

private:

	template<typename Event>
	void fire_next(Event event)
	{
		if(event.trivia.game_time > m_cutoff) {
			m_cutoff = event.trivia.game_time;
			static_cast<IEventObserver&>(m_next).fire(event);
		}
	}

	EventObserver m_next; //!< successor event handler
	long m_cutoff = 0; //!< time of last observed event

};

/**
 * This glue class connects combo and chain events reported by the director (logic)
 * with the BonusIndicator display class.
 */
class BonusRelay : public IEventObserver
{

public:

	explicit BonusRelay(Stage& stage);

	virtual void fire(evt::Match event) override;
	virtual void fire(evt::Chain event) override;

private:

	Stage* m_stage;

};

/**
 * A handler for game events that cause sound outputs.
 */
class SoundRelay : public IEventObserver
{

public:

	virtual void fire(CursorMoves event) override;
	virtual void fire(Swap event) override;
	virtual void fire(Match event) override;
	// virtual void fire(Chain event) override;
	virtual void fire(PhysicalLands event);
	virtual void fire(BlockDies event);
	virtual void fire(GarbageDissolves event);

};

}
