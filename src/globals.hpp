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
	BANNER
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

// Allow prefix operator++ on BlockFrame
BlockFrame& operator++(BlockFrame& frame);

/**
 * Direction, used for player input & moving cursor
 */
enum class Dir { NONE, LEFT, RIGHT, UP, DOWN };

/**
 * Represents a screen location in canvas pixels.
 * {0,0} top left - {CANVAS_W,CANVAS_H} bottom right
 */
struct Point
{
	float x, y;
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
};

std::ostream& operator<<(std::ostream& stream, RowCol rc);

const int FPS = 60; // aspired-to number of drawn and displayed frames per second
const int TPS = 30; // fixed number of logic ticks per second (game speed)

// Canvas pixel sizes and locations of objects
const int CANVAS_W = 640; // width of drawing canvas in pixels
const int CANVAS_H = 480; // width of drawing canvas in pixels
const int BLOCK_W = 40; // width of one little colored block
const int BLOCK_H = 40; // height of one little colored block
const int CURSOR_W = 88; // width of the cursor texture
const int CURSOR_H = 48; // height of the cursor texture
const Point LPIT_LOC = { 32, 48 };
const Point RPIT_LOC = { 368, 48 };
const int PIT_COLS = 6; // number of blocks that fit in a pit next to each other
const int PIT_W = PIT_COLS*BLOCK_W; // width of the pit in canvas pixels
const int PIT_H = 10*BLOCK_H; // height of the pit in canvas pixels
const int BANNER_W = 200; // width of the win/lose banner in canvas pixels
const int BANNER_H = 140; // height of the win/lose banner in canvas pixels

// Gameplay constants
const float FALL_SPEED = 7; // max. pixels per update that a falling block moves down
const float SCROLL_SPEED = 1; // .4f; // pixels per update that the pit moves up

// drawing order for objects
constexpr int SCREEN_Z = 1;
constexpr int PIT_Z = 2;
constexpr int BLOCK_Z = 3;
constexpr int PITVIEW_Z = 4;
constexpr int CURSOR_Z = 5;
constexpr int BANNER_Z = 6;

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
