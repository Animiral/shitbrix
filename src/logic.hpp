/**
 * Implementation of high-level functions to examine and manipulate game objects.
 * These functions are used by the Director to implement the game logic.
 */
#pragma once

#include "state.hpp"
#include <unordered_set>

/**
 * Examines the pit for matching blocks from a sequence of “hot” blocks
 * which have just been moved or landed. They are passed to the MatchBuilder via ignite(). 
 * Returns all detected matching blocks (3 or more in a row from a hot block) in result().
 * The combo() specifies the number of blocks resolved at the same time.
 */
class MatchBuilder
{

private:

	/**
	 * Hashing helper struct to enable a std::unordered_set of blocks and garbage
	 */
	struct PhysHash
	{
		size_t operator()(const Physical& phys) const noexcept { return RowColHash{}(phys.rc()); }
	};

	/**
	 * Equality helper struct to enable a std::unordered_set of blocks and garbage
	 */
	struct PhysEqual
	{
		bool operator()(const Physical& lhs, const Physical& rhs) const noexcept { return lhs.rc() == rhs.rc(); }
	};

public:

	using BlockSet = std::unordered_set<std::reference_wrapper<Block>, PhysHash, PhysEqual>;
	using GarbageSet = std::unordered_set<std::reference_wrapper<Garbage>, PhysHash, PhysEqual>;

	MatchBuilder(const Pit& pit) : pit(pit), m_chaining(false) {}

	void ignite(Block& block);
	const BlockSet& result() { return m_result; }

	int combo() { return static_cast<int>(m_result.size()); }
	bool chaining() { return m_chaining; }

private:

	const Pit& pit;
	BlockSet m_result;
	bool m_chaining;

	bool match_at(RowCol rc, Color color);
	void insert(RowCol rc);

};

using PhysicalRefVec = std::vector<std::reference_wrapper<Physical>>;
using BlockRefVec = std::vector<std::reference_wrapper<Block>>;
using GarbageRefVec = std::vector<std::reference_wrapper<Garbage>>;

/**
 * This class implements building-block routines to examine and manipulate
 * objects in the given game state.
 *
 * While the game elements implement their own behavior to a degree (e.g. a
 * block will continuously fall down on its own), the Logic can look at the
 * object's surroundings and identify key features, such as landing blocks.
 *
 * It helps in the transition of the game state by manipulating object tags
 * and behavioral states.
 *
 * The Director then puts these building-block routines to use every update.
 */
class Logic
{

public:

	explicit Logic(Pit& pit) : m_pit(pit) {}

	/**
	 * Mark all objects at the given location and above as potentially falling.
	 */
	void trigger_falls(RowCol rc, bool chaining) const;

	/**
	 * Look at the pit contents and determine if any of the contents fulfill
	 * specific criteria.
	 *
	 * @param[out] chaining whether any block is currently marked as chaining
	 * @param[out] breaking whether any block is currently being dissolved
	 * @param[out] full whether any resting physical is up against the pit top
	 * @param[out] starving whether the bottom+1 row is empty based on scrolling
	 */
	void examine_pit(bool& chaining, bool& breaking, bool& full, bool& starving) const noexcept;

	/**
	 * Classify Physicals whose states are “running out”.
	 * For example, an object’s internal timer can run out while they are falling,
	 * indicating that they have reached their target location.
	 *
	 * New blocks in *preview* state appear at the bottom of the pit as it scrolls.
	 * As they arrive in the cursor-accessible area of the pit,
	 * the previous previews become normal blocks at rest.
	 * In this instant, they are tagged as *hot*.
	 *
	 * @param[out] dead_physical whether there are new dead physicals
	 * @param[out] dead_block whether there are new dead blocks
	 * @param[out] dead_sound whether there are non-fake dead blocks
	 * @param[out] chainstop whether a chain might be finished
	 * @param[out] new_row whether the bottom of blocks becomes active
	 */
	void examine_finish(bool& dead_physical, bool& dead_block, bool& dead_sound,
	                    bool& chainstop, bool& new_row) const;

	/**
	 * Shrink or remove expired garbage blocks.
	 * As a result, some physicals may be tagged with TAG_FALL.
	 */
	void convert_garbage() const;

	/**
	 * All physicals tagged with TAG_FALL now actually enter the *fall*
	 * state if possible.
	 * Successful fallers can not match and therefore have TAG_HOT removed.
	 */
	void handle_fallers() const;

	/**
	 * All matching blocks and all adjacent garbage bricks enter the *break* state.
	 *
	 * @param pit Pit object
	 * @param[out] have_match Flag which indicates true if there is at least one match
	 * @param[out] combo Counter for the number of blocks matched
	 * @param[out] chaining Flag which indicates true if there is a match involving chaining blocks
	 * @param[out] chainstop Flag which indicates true if chaining blocks have come to rest
	 */
	void handle_hots(bool& have_match, int& combo, bool& chaining, bool& chainstop) const;

private:

	Pit& m_pit;

	/**
	 * Mark the garbage and any other garbage it touches with the TAG_TOUCH tag.
	 */
	void touch_garbage(Garbage& garbage) const;

};
