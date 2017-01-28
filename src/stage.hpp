/**
 * stage.hpp
 * This module contains all the basic classes of objects that we see
 * exclusively during gameplay (on the GameScreen).
 * These are objects such as Block, Pit, Cursor and Stage itself.
 */
#pragma once

#include "globals.hpp"
#include <memory>
#include <vector>
#include <map>
#include <random>

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
	enum class State { DEAD, REST, FALL, LAND, BREAK, X1, X2 };


	Physical(RowCol rc, State state);
	virtual ~Physical() noexcept =default;


	Point loc() const noexcept { return m_loc; }
	RowCol rc() const noexcept { return m_rc; }

	/**
	 * Changes the Physical’s logical location while maintaining its draw position,
	 * now relative to the new rc.
	 */
	void set_rc(RowCol rc);

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
	virtual void set_state(State state);

protected:

	Point m_loc;    //!< logical location, upper left corner relative to view (not necessarily sprite draw location)
	Point m_offset; //!< x/y offset from draw center of r/c location
	Point m_target; //!< target location - where the block really wants to be while it’s busy with an animation like SWAP
	RowCol m_rc;    //!< row/col position, - is UP, + is DOWN

	State m_state;  //!< current state

	/**
	 * Template method for subclass tick update implementation.
	 */
	virtual void update_impl() {}

	/**
	 * Set the time and speed until the state is finished.
	 */
	void set_timeout(int time, int speed = 1);

private:

	int m_time;     //!< number of steps until we consider a state switch
	int m_speed;    //!< number of steps per tick

};

/**
 * Single block, comes in 6 colors
 */
class Block : public Physical
{

public:

	enum class Color { FAKE, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };

	/*
	 * Block states.
	 *  * DEAD: should be removed from the pit asap as it is an error to logic update() a dead block
	 *  * REST: the block is inactive and stationary. Only resting blocks can match.
	 *  * FALL: on its way down the pit at FALL_SPEED
	 *  * LAND: for a short period of time, after its fall stops, the block holds out on matches & can be swapped
	 *  * BREAK: the block has been matched and is in the process of destruction
	 *  * SWAP: the block is moving to another location by swapping
	 *  * PREVIEW: init state. (Partially) visible, but not yet subject to matches and swapping
	 */
	enum class State { DEAD, REST, FALL, LAND, BREAK, SWAP, PREVIEW };

	Color col; // color
	bool chaining; // Whether this block is chaining (falling down from a match)

	Block(Color col, RowCol rc, State state);
	virtual ~Block() noexcept =default;

	virtual int rows() const noexcept override { return 1; }
	virtual int columns() const noexcept override { return 1; }
	virtual void set_state(Physical::State state) override;

	State block_state() const noexcept { return static_cast<State>(m_state); }

	/**
	 * Starts the swapping state & animation for this block.
	 * This function replaces set_state(State::SWAP) because of the additional
	 * information that must be conveyed in the target parameter.
	 */
	void swap_toward(RowCol target) noexcept;

	bool is_swappable() const noexcept;
	bool is_matchable() const noexcept;

private:

	BlockFrame m_anim;  // current animation frame

	/**
	 * Block-specific tick update implementation.
	 */
	virtual void update_impl() override;

	/**
	 * Update this swapping block
	 */
	void swap();

	/**
	 * Update this falling block
	 */
	void fall();

	/**
	 * Update this landing block
	 */
	void land();

	/**
	 * Update this breaking block
	 */
	void dobreak();
	
};

/*
 * Allow operator- on Block::Color
 */
int operator-(Block::Color lhs, Block::Color rhs) noexcept;

/**
 * Comparison predicate for ordering blocks bottom-to-top.
 */
bool y_greater(const Block& lhs, const Block& rhs) noexcept;

/**
 * Garbage block.
 * This block is a bit like the common blocks in that it occupies some space
 * in the pit. Garbage blocks span multiple spaces. They never spawn from the
 * bottom, always falling from above.
 */
class Garbage : public Physical
{

public:

	Garbage(RowCol rc, int columns, int rows);
	virtual ~Garbage() noexcept =default;

	virtual int rows() const noexcept override { return m_rows; }
	virtual int columns() const noexcept override { return m_columns; }
	virtual void set_state(State state) override;

	int shrink() { return --m_rows; }

private:

	int m_columns;  // width of this garbage in blocks
	int m_rows;     // height of this garbage in blocks

	/**
	 * Garbage-specific tick update implementation.
	 */
	virtual void update_impl() override;

	void fall();
	void land();
	void dobreak();

};

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit owns and updates its contained blocks and garbage.
 * It remembers where blocks are in a sparse matrix.
 * It also handles scrolling.
 */
class Pit
{

public:

	Pit(Point loc) noexcept;

	Point loc() const noexcept { return m_loc; }

	/**
	 * Pit-internal storage class
	 */
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
	 * Create a new Block with the specified properties in the Pit.
	 * Caution! This may invalidate all existing references to Blocks in the Pit.
	 *
	 * @return a reference to the created Block
	 */
	Block& spawn_block(Block::Color color, RowCol rc, Block::State state);

	/**
	 * Create a new Garbage with the specified dimensions.
	 * Caution! This may invalidate all existing references to Garbage in the Pit.
	 *
	 * @return a reference to the created Garbage
	 */
	Garbage& spawn_garbage(RowCol rc, int columns, int rows);

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
	void swap(Block& left, Block& right) noexcept;

	/**
	 * Remove dead Physicals from the Pit to clean it up.
	 * Caution! This may invalidate all existing references to Physicals in the Pit.
	 */
	void remove_dead();

	/**
	 * Reduce the size of the referenced Garbage by one row from the bottom.
	 * If that one row was the entire size of the Garbage, it is removed
	 * completely.
	 * Caution! This may invalidate all existing references to Garbage in the Pit.
	 *
	 * @return a pointer to the reduced Garbage, or nullptr if the Garbage is gone
	 */
	Garbage* shrink(Garbage& garbage);

	/**
	 * Remove all objects from the Pit.
	 */
	void clear();

	/**
	 * Returns the number of the top accessible row in the pit
	 */
	int top() const noexcept;

	/**
	 * Returns the number of the bottom accessible row in the pit
	 */
	int bottom() const noexcept;
	int peak() const noexcept;
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

	using PhysMap = std::map<RowCol, Physical*>;

	Point m_loc;     // draw location, upper left corner
	bool m_enabled;  // whether or not to scroll the pit on update()
	int m_scroll;    // y-offset in points for view on pit contents
	int m_speed;     // per-update delta for m_scroll in points
	int m_peak;      // highest blocked row (may be above visible space)
	PhysVec m_contents; // list of all blocks in the pit
	PhysMap m_content_map; // sparse matrix of blocked spaces

	int m_highlight_row;

	void refresh_peak() noexcept; //!< Search for the new m_peak
	void fall_block(Block& block); //!< Move the Block to the to-location.
	void fall_garbage(Garbage& garbage); //!< Move the Garbage to the to-location.
	void fill_area(Physical& physical); //!< Mark the area as occupied.
	void clear_area(const Physical& physical); //!< Mark the area as not occupied.

};

class Cursor
{

public:

	RowCol rc;
	int time;

	Cursor(RowCol rc) : rc(rc), time(0) {}

	void update() { time++; }

};

enum class BannerFrame : size_t { WIN=0, LOSE=1 };

class Banner
{

public:

	Banner(Point loc, BannerFrame frame) : loc(loc), frame(frame) {}

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
 * The Stage owns all its contained objects.
 */
class Stage
{

public:

	Stage() {}
	Stage(const Stage& ) =delete;

	//! Helper struct for stage contents (1 player)
	struct PitCursor
	{
		PitCursor(Point loc, Point bonus_loc);

		Pit pit;
		Cursor cursor;
		BonusIndicator bonus;
	};

	using PitsVector = std::vector<std::unique_ptr<PitCursor>>;

	/**
	 * Add a pit to the stage to be displayed at the given point coordinates.
	 */
	PitCursor& add_pit(Point loc, Point bonus_loc);
	void update();

	PitsVector& pits() { return m_pits; }
	const PitsVector& pits() const { return m_pits; }

private:

	PitsVector m_pits;

};

class StageBuilder
{

public:

	Pit* left_pit;
	Cursor* left_cursor;
	BonusIndicator* left_bonus;

	Pit* right_pit;
	Cursor* right_cursor;
	BonusIndicator* right_bonus;

	std::unique_ptr<Stage> construct();

};
