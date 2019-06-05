#pragma once

#include "event.hpp"

class Journal;
class GameState;
class IColorSupplier;

/**
 * The Arbiter is a special rules component which depends on a random number
 * generator. In contrast to the regular rules, which any client running the
 * game can execute, the Arbiter decisions are subject to these features:
 *
 * 1. The results of the random rolls can not be predicted by the
 *    players until the course of the game triggers them.
 * 2. Different clients do not share a common RNG implementation.
 * 3. In play with live inputs, the results are random, but in replays,
 *    they are pre-determined.
 *
 * For these reasons, the Arbiter can not be decentralized. Instead, we model
 * it as a separate source of inputs, which are the random decisions taken
 * during the game. The journal also records these decisions for the replay.
 * When the Journal rewinds the game state, it discards all arbiter decisions
 * from the obsolete time line. The Arbiter must then repeat those decisions.
 *
 * The Arbiter takes its decisions as a reaction to the demand of the game.
 * It is therefore implemented as an event observer.
 */
class IArbiter : evt::IEventObserver
{

public:

	virtual void fire(evt::Match match) override =0;
	virtual void fire(evt::Chain chain) override =0;
	virtual void fire(evt::Starve starve) override =0;

};

/*
 * This Arbiter observes combos, chains and scrolling from the local game
 * and in response, produces the appropriate arbiter inputs, which go
 * directly to the journal.
 * This implementation does not consider a server or client perspective.
 */
class LocalArbiter : IArbiter
{

public:

	explicit LocalArbiter(Journal& journal, GameState& state,
		std::unique_ptr<IColorSupplier> color_supplier);

	virtual void fire(evt::Match match) override;
	virtual void fire(evt::Chain chain) override;
	virtual void fire(evt::Starve starve) override;

private:

	Journal* m_journal; //!< active game record
	GameState* m_state; //!< active game state
	std::unique_ptr<IColorSupplier> m_color_supplier; //!< rng component

	/**
	 * Insert a new input into the journal that triggers the appropriate
	 * garbage throw.
	 */
	void input_garbage(long game_time, int victim, int columns, int rows, bool right_side);

};
