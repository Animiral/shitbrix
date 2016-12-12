/**
 * globals.hpp
 * General global definitions without dependencies.
 * Every other header may include this header.
 */

#pragma once

// check all the asserts everywhere
#ifndef SDL_ASSERT_LEVEL
#define SDL_ASSERT_LEVEL 3
#endif

#include <algorithm>
#include <exception>
#include <iostream> // debug stuff

/**
 * IDs for all the gfx assets.
 * One gfx can refer to several frames or states of the object.
 */
enum class Gfx
{
	BACKGROUND = 0,
	BLOCK_BLUE,
	BLOCK_RED,
	BLOCK_YELLOW,
	BLOCK_GREEN,
	BLOCK_PURPLE,
	BLOCK_ORANGE,
	PITVIEW,  // debug gfx
	CURSOR,
	BANNER,
	GARBAGE
};

/**
 * IDs for all the sound effect assets.
 */
enum class Snd
{
	SWAP = 0, // swap blocks (click)
	BREAK,    // break blocks (splat)
	MATCH,    // match blocks (ding)
	CONFIRM,  // menu confirm (cheerful ding)
	DECLINE,  // menu decline (disappointed ding)
	START,    // game start (shot or fireworks launch)
	END,      // game end (alarming crumble)
	RESULT    // game over (cheer)
};

// Allow operator+ on Gfx
Gfx operator+(Gfx gfx, int delta);

enum class BlockFrame : size_t
{
	REST = 0,
	PREVIEW = 1,
	BREAK_BEGIN = 2, // sequence of break anim
	BREAK_END = 6    // 1-past-end index
};

enum class GarbageFrame : size_t
{
	TOP_LEFT = 0,
	TOP,
	TOP_RIGHT,
	MID_LEFT,
	MID,
	MID_RIGHT,
	LOW_LEFT,
	LOW,
	LOW_RIGHT
};

// Allow prefix operator++ on BlockFrame
BlockFrame& operator++(BlockFrame& frame);

/**
 * Direction, used for moving cursor
 */
enum class Dir { NONE, LEFT, RIGHT, UP, DOWN };

/**
 * All input actions that the game accepts at any point from one source,
 * after key mapping from the original input device (e.g. keyboard).
 * Direction values can be cast to and from Dir.
 */
enum class Button
{
	NONE,          // no button was pressed
	LEFT, RIGHT, UP, DOWN, // directional pad
	A, B,          // standard action buttons
	PAUSE,         // pause the game
	RESET,         // restart the game
	QUIT,          // exit the game
	DEBUG1,        // debug functions
	DEBUG2,
	DEBUG3,
	DEBUG4,
	DEBUG5
};

/**
 * Enumeration of possible input actions by one player.
 * These are also the possible actions from a replay file.
 * Direction values can be cast to and from Dir.
 * All values can be cast to and from Button.
 */
enum class GameButton { NONE, LEFT, RIGHT, UP, DOWN, SWAP, RAISE };

constexpr int NOONE = -1; // not-a player id

/**
 * Holds one button input and the number of the player who pressed it.
 */
struct ControllerInput
{
	int player; // 0-based player index
	Button button;
};

/**
 * Holds one in-game action and the number of the player who pressed it.
 */
struct GameInput
{
	int player; // 0-based player index
	GameButton button;
};

/**
 * Represents a screen location in canvas pixels.
 * {0,0} top left - {CANVAS_W,CANVAS_H} bottom right
 */
struct Point
{
	float x, y;

	Point offset(float dx, float dy) const { return Point{x+dx, y+dy}; }
};

/**
 * Represents a block-sized space in one of the pits.
 * row 0 = base line (lowest line at the start)
 * row -9 = top of screen at the start
 * column 0 = leftmost column
 */
struct RowCol
{
	int r, c;

	// Order function required by std::map to use as key type and
	// BlockDirector::update() sort. Sorts bottom-to-top.
	bool operator<(const RowCol& rhs) const { return (r == rhs.r) ? c > rhs.c : r > rhs.r; }

	bool operator==(const RowCol& rhs) const { return r == rhs.r && c == rhs.c; }
};

std::ostream& operator<<(std::ostream& stream, RowCol rc);

constexpr int FPS = 60; // aspired-to number of drawn and displayed frames per second
constexpr int TPS = 30; // fixed number of logic ticks per second (game speed)

constexpr const char* APP_NAME = "shitbrix";
constexpr int AUDIO_SAMPLES = 4096;

// Canvas pixel sizes and locations of objects
constexpr int CANVAS_W = 640; // width of drawing canvas in pixels
constexpr int CANVAS_H = 480; // width of drawing canvas in pixels
constexpr int BLOCK_W = 40; // width of one little colored block
constexpr int BLOCK_H = 40; // height of one little colored block
constexpr int GARBAGE_W = BLOCK_W/2; // width of one drawable piece of garbage
constexpr int GARBAGE_H = BLOCK_H/2; // height of one drawable piece of garbage
constexpr int CURSOR_W = 88; // width of the cursor texture
constexpr int CURSOR_H = 48; // height of the cursor texture

constexpr Point LPIT_LOC = { 32, 48 };
constexpr Point RPIT_LOC = { 368, 48 };
constexpr int PIT_COLS = 6; // number of blocks that fit in a pit next to each other
constexpr int PIT_ROWS = 10; // number of blocks that fit in a pit on top of each other
constexpr int COL_W = BLOCK_W; // width of a single column in the pit
constexpr int ROW_H = BLOCK_H; // height of a single row in the pit
constexpr int PIT_W = PIT_COLS*COL_W; // width of the pit in canvas pixels
constexpr int PIT_H = PIT_ROWS*ROW_H; // height of the pit in canvas pixels

constexpr int BANNER_W = 200; // width of the win/lose banner in canvas pixels
constexpr int BANNER_H = 140; // height of the win/lose banner in canvas pixels

// Gameplay constants
constexpr float FALL_SPEED = 7; // max. pixels per update that a falling block moves down
constexpr float SCROLL_SPEED = 1; // .4f; // pixels per update that the pit moves up

Point from_rc(RowCol rc); // conversion to pit-relative coordinates

/**
 * Insert an element into a container in a specified sorted order.
 * The elements in the container must already be ordered.
 */
template< class Container, class Elem = typename Container::value_type, class Order = std::less<Elem> >
void ordered_insert(Container& container, Elem&& elem, Order order = Order())
{
	auto it = std::upper_bound(std::begin(container), std::end(container), elem, order);
	container.insert(it, std::forward<Elem>(elem));
}

/**
 * General exception for errors that occur in the game.
 */
class GameException : public std::exception
{
public:
	GameException(const char* what = "") : m_what(what) {}
	GameException(const GameException& rhs) =default;
	GameException(GameException&& rhs) =default;
	virtual const char* what() const noexcept override { return m_what; }
private:
	const char* m_what;
};

void game_assert(bool condition, const char* what);
