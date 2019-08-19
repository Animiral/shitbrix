#pragma once

#include "event.hpp"

class Journal;
class GameState;

/**
 * Abstract representation of a generator of block colors.
 * These colors (or, in the future, other properties) are used to spawn blocks
 * into the game with desirable guarantees, such as not immediately matching
 * from spawn.
 * The live implementation should be based on random rolls. We keep it abstract
 * to allow for non-random implementations for debugging and testing.
 */
class IColorSupplier
{

public:

	virtual ~IColorSupplier() = 0;

	/**
	 * Return the next color of a block coming out on the stack from below.
	 * TODO: this function must generate blocks not to auto-match instantly.
	 */
	virtual Color next_spawn() noexcept = 0;

	/**
	 * Return the next color of a block emerging as a result of dissolving garbage.
	 * TODO: this function must not generate three same-colored blocks in a row.
	 */
	virtual Color next_emerge() noexcept = 0;

	/**
	 * Suppliers can copy themselves.
	 */
	virtual std::unique_ptr<IColorSupplier> clone() const = 0;

};

/**
 * Maintains a sequence of block colors spawned deterministically out of an
 * initial seed. This allows us to see what color blocks to introduce next,
 * as well as reconstruct the whole history of spawned block colors for
 * replay and netplay purposes.
 */
class RandomColorSupplier : public IColorSupplier
{

public:

	/**
	 * Construct the supplier with the given seed, which deterministically produces
	 * the same block colors every time.
	 * The blocks are mixed up by the given player number.
	 */
	explicit RandomColorSupplier(unsigned seed, int player);

	virtual Color next_spawn() noexcept override;
	virtual Color next_emerge() noexcept override;
	virtual std::unique_ptr<IColorSupplier> clone() const override { return std::make_unique<RandomColorSupplier>(*this); }

private:

	//std::vector<Color> m_record; // required later for rules refinement
	std::minstd_rand m_generator;

};

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
class IArbiter : public evt::IEventObserver
{

public:

	virtual ~IArbiter() noexcept =0;

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
class LocalArbiter : public IArbiter
{

public:

	explicit LocalArbiter(GameState& state, Journal& journal,
		std::unique_ptr<IColorSupplier> color_supplier);

	virtual void fire(evt::Match match) override;
	virtual void fire(evt::Chain chain) override;
	virtual void fire(evt::Starve starve) override;

private:

	GameState* m_state; //!< active game state
	Journal* m_journal; //!< active game record
	std::unique_ptr<IColorSupplier> m_color_supplier; //!< rng component

	/**
	 * Insert a new input into the journal that triggers the appropriate
	 * garbage throw.
	 */
	void input_garbage(long game_time, int victim, int columns, int rows, bool right_side);

};
