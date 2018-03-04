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
	GARBAGE_LU,
	GARBAGE_U,
	GARBAGE_RU,
	GARBAGE_L,
	GARBAGE_M,
	GARBAGE_R,
	GARBAGE_LD,
	GARBAGE_D,
	GARBAGE_RD,
	BONUS,
	MENUBG
};

/**
 * IDs for all the sound effect assets.
 */
enum class Snd
{
	SWAP = 0, // swap blocks (click)
	BREAK,    // break blocks (splat)
	MATCH,    // match blocks (ding)
	LANDING,  // smashing block landing (thump)
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

enum class BonusFrame : size_t
{
	COMBO,
	CHAIN
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

/**
 * Enumeration of the sorts of inputs that the player can perform on a button.
 * For some buttons (e.g. PAUSE), only DOWN may be registered.
 */
enum class ButtonAction { DOWN, UP };

constexpr int NOONE = -1; // not-a player id

/**
 * Holds one button input and the number of the player who pressed it.
 */
struct ControllerInput
{
	int player; // 0-based player index
	Button button;
	ButtonAction action;
};

/**
 * Holds one in-game action and the number of the player who pressed it.
 */
struct GameInput
{
	static const long TIME_ASAP = -1; //!< this input should be part of the next update

	long game_time;      //!< time when this input takes effect
	int player;          //!< 0-based player index
	GameButton button;
	ButtonAction action;
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

	bool operator==(const RowCol& rhs) const { return r == rhs.r && c == rhs.c; }
};

/**
 * Hash function for @c RowCol to use with `std::unordered_map`.
 */
struct RowColHash
{
	size_t operator()(RowCol rc) const noexcept { return (rc.c << 16) + rc.r; }
};

std::ostream& operator<<(std::ostream& stream, RowCol rc);

constexpr int TPS = 30; // fixed number of logic ticks per second (game speed)

// Gameplay constants
constexpr int PIT_COLS = 6; //!< number of blocks that fit in a pit next to each other
constexpr int PIT_ROWS = 10; //!< number of blocks that fit in a pit on top of each other
constexpr int ROW_HEIGHT = 200; //!< gameplay height of a row; determines scroll speed etc.
constexpr int FALL_SPEED = 35; //!< points per update that a falling block moves down
constexpr int SCROLL_SPEED = 5; //!< points per update that the pit moves up
constexpr int RAISE_SPEED = 15; //!< pit speed when raising the stack (max speed)
constexpr int SWAP_TIME = 6; //!< number of ticks to swap two blocks
constexpr int BREAK_TIME = 30; //!< number of ticks for a block to break
constexpr int DISSOLVE_TIME = 30; //!< number of ticks for a garbage brick to dissolve
constexpr int LAND_TIME = 20; //!< number of ticks in an object’s landing state
constexpr int RECOVERY_TIME = 50; //!< number of ticks in which scrolling stops after quality match
constexpr int PANIC_TIME = 90; //!< number of ticks until game over when the pit is full

// Presentation constants (graphics, animation, sounds)
constexpr const char* APP_NAME = "shitbrix";
constexpr int FPS = 60; //!< aspired-to number of drawn and displayed frames per second
constexpr int AUDIO_SAMPLES = 4096;

constexpr int CANVAS_W = 640; //!< width of drawing canvas in pixels
constexpr int CANVAS_H = 480; //!< height of drawing canvas in pixels
constexpr int BLOCK_W = 40; //!< width of one little colored block
constexpr int BLOCK_H = 40; //!< height of one little colored block
constexpr int GARBAGE_W = BLOCK_W/2; //!< width of one drawable piece of garbage
constexpr int GARBAGE_H = BLOCK_H/2; //!< height of one drawable piece of garbage
constexpr int CURSOR_W = 88; //!< width of the cursor texture
constexpr int CURSOR_H = 48; //!< height of the cursor texture
constexpr int BONUS_W = 16; //!< width of the combo/chain star
constexpr int BONUS_H = 16; //!< height of the combo/chain star
constexpr float SHAKE_SCALE = 10.f; //!< pixel amount of shaking per row of garbage dropped
constexpr float SHAKE_DECREASE = .6f; //!< scale factor for shake strength per frame

constexpr Point LPIT_LOC = { 32, 48 };
constexpr Point RPIT_LOC = { 368, 48 };
constexpr Point LBONUS_LOC = { 320-32-5, 400 };
constexpr Point RBONUS_LOC = { 320+5, 400 };
constexpr int COL_W = BLOCK_W; //!< pixel width of a single column in the pit
constexpr int ROW_H = BLOCK_H; //!< pixel height of a single row in the pit
constexpr int PIT_W = PIT_COLS*COL_W; //!< width of the pit in canvas pixels
constexpr int PIT_H = PIT_ROWS*ROW_H; //!< height of the pit in canvas pixels

constexpr int BANNER_W = 200; //!< width of the win/lose banner in canvas pixels
constexpr int BANNER_H = 140; //!< height of the win/lose banner in canvas pixels

constexpr int TRANSITION_TIME = 20; //!< Number of frames for screen transition

Point from_rc(RowCol rc); // conversion to pit-relative coordinates

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
