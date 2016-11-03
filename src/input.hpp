/**
 * input.hpp
 * Functions for converting user actions into game actions.
 */
#pragma once

#include "globals.hpp"

/**
 * A ControllerSink accepts input from an (imagined) controller.
 */
class IControllerSink
{
	public: virtual void input(ControllerInput input) =0;
};

/**
 * Reads keys from the keyboard and converts them into ControllerInputs.
 * These inputs are then sent to the keyboard’s sink object.
 *
 * By default, this keyboard collects the following inputs:
 *  * \[RETURN]: reset key
 *  * \[ESC]: quit key
 *  * Player 1: arrow keys + [Z]
 *  * Player 2: numpad 8456 + [0]
 *  * [D], [H]: toggle debug view on pits
 *
 * The keys can currently not be remapped.
 */
class Keyboard
{

public:
	Keyboard(IControllerSink& sink) : m_sink(sink) {}
	void poll(); //!< read events from the keyboard buffer, send to sink

private:
	IControllerSink& m_sink;

};
