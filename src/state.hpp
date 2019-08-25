/**
 * This module contains all the basic classes of objects that we see
 * exclusively during gameplay (on the GameScreen).
 * These are objects such as Block, Pit, Cursor and the GameState itself.
 */
#pragma once

#include "globals.hpp"
#include <vector>
#include <random>
#include <memory>
#include <functional>
#include <ostream>

/**
 * Base class for game objects that can be placed in the Pit.
 * All Physical objects occupy space according to their extents (rows and columns).
 */
class Physical
{

public:

	/**
	 * Common states of Physical objects.
	 * The derived classes intentionally extend this definition
	 * by properly defining the placeholder X-states.
	 */
	enum class State { DEAD, REST, FALL, LAND, BREAK, X1, X2, X3 };

	/**
	 * Flags for tagging the Physical during logic update.
	 */
	enum Tag { TAG_NONE = 0, TAG_FALL = 1, TAG_HOT = 2, TAG_TOUCH = 4, TAG_DISSOLVE = 8, TAG_LAND = 16, TAG_ANY = 31 };

	Physical(RowCol rc, State state);
	Physical(const Physical& ) =default;
	Physical(Physical&& ) =default;
	virtual ~Physical() noexcept =default;

	/**
	 * Physicals can copy themselves.
	 */
	virtual std::unique_ptr<Physical> clone() const =0;


	RowCol rc() const noexcept { return m_rc; }
	void set_rc(RowCol rc) noexcept { m_rc = rc; }

	virtual int rows() const noexcept =0;
	virtual int columns() const noexcept =0;


	/**
	 * Returns the ticks until the estimated time of arrival of the physical.
	 *
	 * The time of arrival is the moment when the physical’s time
	 * reaches 0, often resulting in some game-logical change.
	 * The return value may not be a whole number if the object is
	 * bound to overshoot.
	 */
	float eta() const noexcept;

	/**
	 * Returns true if the Physical has just now finished its current state.
	 */
	bool is_arriving() const noexcept;
	bool is_fallible() const noexcept;

	/**
	 * Updates the Physical by one tick of game logic.
	 *
	 * Even though Physicals do not know much about the greater purposes
	 * of game logic, they do some bookkeeping of their own. Mostly, they
	 * implement a state machine with timeouts.
	 */
	void update();


	State physical_state() const noexcept { return m_state; }

	/**
	 * Change the state of the Physical object.
	 *
	 * @param state the new state to change into
	 * @param time the duration of the state until @ref is_arriving
	 * @param speed how much time to deduct from the state every @ref update
	 */
	void set_state(State state, int time = 1, int speed = 1) noexcept;

	/**
	 * Add more time to the current state of the object and let it arrive again.
	 */
	void continue_state(int time_bonus) noexcept;

	bool has_tag(Tag tag) const noexcept { return m_tag & tag; }

	void set_tag(Tag tag) noexcept { m_tag = static_cast<Tag>(m_tag | tag); }

	void un_tag(Tag tag) noexcept { m_tag = static_cast<Tag>(m_tag & ~tag); }

	void clear_tags() noexcept { m_tag = TAG_NONE; }

protected:

	RowCol m_rc;    //!< row/col position, - is UP, + is DOWN
	State m_state;  //!< current state

	/**
	 * Template method for subclass tick update implementation.
	 */
	virtual void update_impl() {}

	/**
	 * Template method for subclass state logic implementation.
	 */
	virtual void set_state_impl(State state, int time, int speed) noexcept {}

private:

	int m_time;     //!< number of steps until we consider a state switch
	int m_speed;    //!< number of steps per tick
	Tag m_tag;      //!< informational tags bitfield

};

/**
 * Single block, comes in 6 colors
 */
class Block : public Physical
{

public:

	/*
	 * Block states.
	 *  * DEAD: should be removed from the pit asap as it is an error to logic update() a dead block
	 *  * REST: the block is inactive and stationary. Only resting blocks can match.
	 *  * FALL: on its way down the pit at FALL_SPEED
	 *  * LAND: for a short period of time, after its fall stops, the block holds out on matches & can be swapped
	 *  * BREAK: the block has been matched and is in the process of destruction
	 *  * SWAP_LEFT: the block is moving to the left by swapping
	 *  * SWAP_RIGHT: the block is moving to the right by swapping
	 *  * PREVIEW: init state. (Partially) visible, but not yet subject to matches and swapping
	 */
	enum class State { DEAD, REST, FALL, LAND, BREAK, SWAP_LEFT, SWAP_RIGHT, PREVIEW };

	Color col; // color
	bool chaining; // Whether this block is chaining (falling down from a match)

	Block(Color col, RowCol rc, State state);
	Block(const Block& ) =default;
	Block(Block&& ) =default;

	virtual ~Block() noexcept =default;

	virtual std::unique_ptr<Physical> clone() const override { return std::make_unique<Block>(*this); }

	virtual int rows() const noexcept override { return 1; }
	virtual int columns() const noexcept override { return 1; }

	State block_state() const noexcept { return static_cast<State>(m_state); }
	void set_state(Physical::State state, int time = 1, int speed = 1) noexcept;
	void set_state(State state, int time = 1, int speed = 1) noexcept;

	bool is_swappable() const noexcept;
	bool is_matchable() const noexcept;

private:

	BlockFrame m_anim;  // current animation frame

	/**
	 * Block-specific tick update implementation.
	 */
	virtual void update_impl() override;

	/**
	 * Block-specific state logic implementation.
	 */
	virtual void set_state_impl(Physical::State state, int, int) noexcept override;
	
};

/*
 * Allow operator- on Color
 */
int operator-(Color lhs, Color rhs) noexcept;

/**
 * Comparison predicate for ordering blocks bottom-to-top.
 */
bool y_greater(const Block& lhs, const Block& rhs) noexcept;

/**
 * Type of the blocks hidden in a Garbage brick for the player to break and discover.
 */
using Loot = std::vector<Color>;

/**
 * Garbage block.
 * This block is a bit like the common blocks in that it occupies some space
 * in the pit. Garbage blocks span multiple spaces. They never spawn from the
 * bottom, always falling from above.
 */
class Garbage : public Physical
{

public:

	/**
	 * Construct a Garbage block of the given dimensions.
	 * When dissolved row-by-row, new blocks emerge.
	 * @param rc top-left corner coordinate of the garbage
	 * @param columns number of columns occupied by garbage
	 * @param rows number of rows occupied by garbage
	 * @param loot vector of blocks hidden in the garbage, size == columns*rows.
	 */
	Garbage(RowCol rc, int columns, int rows, Loot loot);
	Garbage(const Garbage& ) =default;
	Garbage(Garbage&& ) =default;
	virtual ~Garbage() noexcept =default;

	virtual std::unique_ptr<Physical> clone() const override { return std::make_unique<Garbage>(*this); }

	virtual int rows() const noexcept override { return m_rows; }
	virtual int columns() const noexcept override { return m_columns; }

	/**
	 * Read the blocks that can be freed next from this garbage by dissolving it.
	 * The returned iterator points to one color for each column from left to right.
	 * The iterator becomes invalid when the garbage shrinks.
	 */
	Loot::const_iterator loot() const;

	/**
	 * Reduce the size of the garbage by one row as it is being dissolved.
	 * The eliminated row is always the bottom one.
	 * @return the number of remaining rows
	 */
	int shrink() noexcept;

private:

	int m_columns;  //!< width of this garbage in blocks
	int m_rows;     //!< height of this garbage in blocks
	Loot m_loot; //!< row-major: bottom-to-top, left-to-right

};


/**
 * As part of the game data in the Pit, the Cursor is the player's input location.
 */
struct Cursor
{
	RowCol rc;
	int time;  //!< animation frame timer
};


/**
 * A pit is the playing area where one player’s blocks fall down.
 * The collection of pits in a game forms the complete game state.
 * The pit owns and updates its contained blocks and garbage.
 * It remembers where blocks are in a sparse matrix.
 * It also handles scrolling.
 */
class Pit
{

public:

	explicit Pit(Point loc) noexcept;
	Pit(const Pit& rhs);
	Pit& operator=(const Pit& rhs);

	Point loc() const noexcept { return m_loc; }

	// Pit-internal storage type
	using PhysVec = std::vector<std::unique_ptr<Physical>>;

	/**
	 * Full access to the Pit’s contents.
	 * This method saves lots of boilerplate code. In exchange, it breaks
	 * encapsulation in every way imaginable. Use with caution!
	 * Do not modify the container! Do not replace the contents!
	 */
	PhysVec& contents() noexcept { return m_contents; }

	/**
	 * Full access to the const Pit’s contents.
	 */
	const PhysVec& contents() const noexcept { return m_contents; }

	/**
	 * Run the given function on every piece of type P in the Pit.
	 */
	template<typename P = Physical, typename Func>
	void for_all(Physical::Tag tag, Func func) const
	{
		for(auto it = m_contents.begin(), e = m_contents.end(); it != e; ++it)
		{
			auto& physical = **it;
			P* p = dynamic_cast<P*>(&physical);
			if(p && physical.has_tag(tag))
				func(*p);
		}
	}

	/**
	 * Return the object contained in the Pit at the given location.
	 *
	 * @return a pointer to the object or nullptr if the space is empty
	 */
	Physical* at(RowCol rc) const noexcept;

	/**
	 * Return the Block contained in the Pit at the given location.
	 *
	 * @return a pointer to the Block or nullptr if there is no Block at rc
	 */
	Block* block_at(RowCol rc) const noexcept;

	/**
	 * Return the Garbage contained in the Pit at the given location.
	 *
	 * @return a pointer to the Garbage or nullptr if there is no Garbage at rc
	 */
	Garbage* garbage_at(RowCol rc) const noexcept;

	/**
	 * Return true if at least one resting physical overflows the allowed space in the pit.
	 */
	bool is_full() const noexcept;

	/**
	 * Create a new Block with the specified properties in the Pit.
	 * Caution! This may invalidate all existing references to Blocks in the Pit.
	 *
	 * @return a reference to the created Block
	 */
	Block& spawn_block(Color color, RowCol rc, Block::State state);

	/**
	 * Create a new Garbage with the specified dimensions and loot inside.
	 * Caution! This may invalidate all existing references to Garbage in the Pit.
	 *
	 * @return a reference to the created Garbage
	 */
	Garbage& spawn_garbage(RowCol rc, int columns, int rows, Loot loot);

	/**
	 * Return true if it is acceptable to move the object
	 * one row down, based on spaces blocked.
	 */
	bool can_fall(Physical& physical) const noexcept;

	/**
	 * Move the object one row down.
	 * If the object cannot fall (because something is in the way), throw
	 * a GameException.
	 */
	void fall(Physical& physical);

	/**
	 * Swap the locations of the two Blocks.
	 */
	void swap(Block& left, Block& right);

	/**
	 * Remove dead Physicals from the Pit to clean it up.
	 * Caution! This may invalidate all existing references to Physicals in the Pit.
	 */
	void remove_dead();

	/**
	 * Remove all tags from all physicals in the pit.
	 */
	void untag_all() noexcept;

	/**
	 * Reduce the size of the referenced Garbage by one row from the bottom.
	 * If that one row was the entire size of the Garbage, it is removed
	 * completely.
	 * Caution! This may invalidate all existing references to Garbage in the Pit.
	 *
	 * @return a pointer to the reduced Garbage, or nullptr if the Garbage is gone
	 */
	Garbage* shrink(Garbage& garbage);

	const Cursor& cursor() const noexcept { return m_cursor; };

	/**
	 * Attempt to move the cursor in the given direction.
	 * The cursor will only move until it hits the edge of the accessible area.
	 * further moves do nothing.
	 */
	void cursor_move(Dir dir) noexcept;

	/**
	 * Sets the want_raise flag for block raise mode. If raise mode is on, the pit
	 * will scroll upwards at an accelerated speed, revealing more block material
	 * in a short time.
	 * Once the want_raise flag is @c true, the pit will immediately accelerate
	 * scrolling. It will stop only when @stop_raise gets called while the
	 * want_raise flag is set to @c false.
	 */
	void set_raise(bool raise);

	/**
	 * If the raise intention flag is @c false, discontinue raise mode.
	 * The BlockDirector calls this when the next whole row of blocks turns from
	 * preview to normal. Until then, just a short tap of the raise button
	 * activates the accelerated scrolling.
	 */
	void stop_raise();

	/**
	 * Returns the number of the top accessible row in the pit
	 */
	int top() const noexcept;

	/**
	 * Returns the number of the bottom accessible row in the pit
	 */
	int bottom() const noexcept;

	int peak() const noexcept;

	/**
	 * Increase the chain counter and return the new value.
	 */
	int do_chain() noexcept { return ++m_chain; }

	/**
	 * Return the value of the chain counter and reset it to 0.
	 */
	int finish_chain() noexcept { int chain = m_chain; m_chain = 0; return chain; }

	/**
	 * Return the fraction of recovery time left.
	 */
	float recovery() const noexcept { return static_cast<float>(m_recovery) / RECOVERY_TIME; }

	/**
	 * Decrease recovery time towards 0 and return the new value.
	 * Recovery time is used to stop scrolling while and after blocks break.
	 */
	int do_recovery() noexcept { return (m_recovery > 0) ? --m_recovery : 0; }

	/**
	 * Set recovery time to the maximum value.
	 */
	void replenish_recovery() noexcept;

	/**
	 * Return the fraction of panic time left.
	 */
	float panic() const noexcept { return static_cast<float>(m_panic) / PANIC_TIME; }

	/**
	 * Decrease panic time towards 0 and return the new value.
	 * Panic time is used to stop scrolling briefly when the pit is full.
	 */
	int do_panic() noexcept { return (m_panic > 0) ? --m_panic : 0; }

	/**
	 * Set recovery time to the maximum value.
	 */
	void replenish_panic() noexcept { m_panic = PANIC_TIME; }

	int highlight_row() const noexcept { return m_highlight_row; }

	void stop() noexcept { m_enabled = false; }
	void start() noexcept { m_enabled = true; }
	void set_speed(int delta) { m_speed = delta; }

	/**
	 * Put a debug highlight on a row
	 */
	void highlight(int row) noexcept;

	/**
	 * The origin {0,0} location of all pit-related objects corresponds with row 0, column 0.
	 * We have to transform the object into the pit and from there, apply the pit scrolling.
	 */
	Point transform(Point point, float dt=0.f) const noexcept;

	void update();

private:

	using PhysMap = std::unordered_map<RowCol, Physical*, RowColHash>;

	Point m_loc;     //!< draw location, upper left corner
	Cursor m_cursor; //!< player cursor
	bool m_want_raise; //!< whether the pit should persist in accelerated scrolling
	bool m_raise;    //!< whether the pit should scroll in new blocks as fast as possible
	bool m_enabled;  //!< whether or not to scroll the pit on update()

	int m_scroll;    //!< y-offset in points for view on pit contents
	int m_speed;     //!< per-update delta for m_scroll in points
	int m_peak;      //!< highest blocked row (may be above visible space)
	int m_chain;     //!< chain counter
	int m_recovery;  //!< recover time pool; scrolling stops after a quality match
	int m_panic;     //!< panic time pool; the player has this many ticks left until game over

	PhysVec m_contents; // list of all blocks in the pit
	PhysMap m_content_map; // sparse matrix of blocked spaces

	int m_highlight_row;

	void refresh_peak() noexcept; //!< Search for the new m_peak
	void fall_block(Block& block); //!< Move the Block to the to-location.
	void fall_garbage(Garbage& garbage); //!< Move the Garbage to the to-location.
	void fill_area(Physical& physical); //!< Mark the area as occupied.
	void clear_area(const Physical& physical); //!< Mark the area as not occupied.
	void assign_basic(const Pit& rhs); //!< Copy basic members from rhs, used in impl of copy&move
	PhysVec copy_contents() const; //!< Return a deep copy of m_contents.
	void make_content_map(); //!< (Re-)build content map from m_contents.

};

/**
 * Holds the whole game state information at one specific point in game time.
 */
class GameState
{

public:

	explicit GameState(GameMeta meta);
	GameState(const GameState& rhs);
	GameState(GameState&& rhs);

	GameState& operator=(GameState rhs) noexcept;

	using PitVector = std::vector<std::unique_ptr<Pit>>;

	const PitVector& pit() const noexcept { return m_pit; };
	long game_time() const noexcept { return m_game_time; }

	void update();

	/**
	 * Given the number of one player in the game, returns the target opponent.
	 * If the given player loses, the opponent wins.
	 * If the given player produces a combo or chain, the opponent receives garbage.
	 */
	int opponent(int player) const noexcept;

private:

	PitVector m_pit; //!< state by player number
	long m_game_time; //!< tick counter

};

/**
 * Write a list of the complete pit contents to the stream.
 */
void debug_print_pit(std::ostream& stream, const Pit& pit);

/**
 * Write an ASCII-art depiction of the Pit to the stream.
 * This visualization does not depict Physical states or countdowns.
 */
void debug_asciiart_pit(std::ostream& stream, const Pit& pit);

/**
 * Write an ASCII-art depiction of all Pits to the stream.
 * This visualization does not depict Physical states or countdowns.
 */
void debug_asciiart_state(std::ostream& stream, const GameState& state);
