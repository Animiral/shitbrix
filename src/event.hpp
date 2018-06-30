/**
 * gameevent.hpp
 * Defines the IGameEvent interface and events through which director objects
 * communicate in-game occurrences to other modules.
 */
#pragma once

#include <algorithm>
#include "stage.hpp"

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
	int player; //!< index of the player who performed the match
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
	int player; //!< index of the player who performed the chain
	int counter; //!< chain counter: how many chaining matches there were
};

/**
 * Event that occurs when a physical has finished falling down and lands on something below.
 */
struct PhysicalLands
{
	const Physical& physical;
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
 * Event that occurs when a game round ends.
 */
struct GameOver
{
	int winner;
};

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

inline IGameEvent::~IGameEvent() {}

/**
 * A pseudo-handler for GameEvents that forwards them to other handlers.
 */
class GameEventHub : public IGameEvent
{

public:

	void subscribe(IGameEvent& handler);
	void unsubscribe(IGameEvent& handler);

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

	std::vector<IGameEvent*> m_handlers;

};

/**
 * This glue class connects combo and chain events reported by the director (logic)
 * with the BonusIndicator display class.
 */
class BonusRelay : public IGameEvent
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
class SoundRelay : public IGameEvent
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
