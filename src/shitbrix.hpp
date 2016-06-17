/**
 * shitbrix.hpp
 * General global definitions without dependencies.
 * Every other header may include this header.
 */

#pragma once

#include <exception>

const int FPS = 60; // aspired-to number of drawn and displayed frames per second
const int TPS = 30; // fixed number of logic ticks per second (game speed)

// Canvas pixel sizes of objects
const int CANVAS_W = 640; // width of drawing canvas in pixels
const int CANVAS_H = 480; // width of drawing canvas in pixels
const int BLOCK_W = 40; // width of one little colored block
const int BLOCK_H = 40; // height of one little colored block

/**
 * IDs for all the gfx assets.
 */
enum class Gfx
{
	BACKGROUND = 0,
	BLOCK_BLUE,
	BLOCK_RED,
	BLOCK_YELLOW,
	BLOCK_GREEN,
	BLOCK_PURPLE,
	BLOCK_ORANGE
};

/**
 * Represents a screen location in canvas pixels.
 * {0,0} top left - {CANVAS_W,CANVAS_H} bottom right
 */
struct Point
{
	int x, y;
};

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

/**
 * Check the condition and, if false, throw a GameException with the given message.
 */
void game_assert(bool condition, const char* what)
{
	if(!condition) {
		throw GameException(what);
	}
}
