/**
 * input.hpp
 * Functions for converting user actions into game actions.
 */
#pragma once

#include "globals.hpp"
#include "replay.hpp"
#include <fstream>
#include <memory>

/**
 * A ControllerSink accepts input from an (imagined) controller.
 */
class IControllerSink
{
	public: virtual void input(ControllerInput input) =0;
};

/**
 * A GameInputSink accepts game actions as input.
 */
class IGameInputSink
{
	public: virtual void input(GameInput input) =0;
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

	/**
	 * Read events from the keyboard buffer, send to sink.
	 */
	void poll();

	/**
	 * Inject the keyboard’s controller sink dependency.
	 */
	void set_game_sink(IControllerSink* sink) noexcept { m_sink = sink; }

private:

	IControllerSink* m_sink;

};

/**
 * The GameInputMixer watches a variety of input sources such as
 * local controllers and replay files, as well as network players
 * (in the future).
 * From these inputs, it decides which ones are relevant and forwards
 * them to the appropriate sink.
 * For example, if a replay is being played back, the mixer ignores
 * regular controller inputs.
 */
class GameInputMixer : public IControllerSink
{

public:

	GameInputMixer(const char* replay_file);

	/**
	 * Inject the mixer’s game input sink dependency.
	 */
	void set_game_sink(IGameInputSink* game_sink) noexcept { m_game_sink = game_sink; }

	/**
	 * Inject the mixer’s replay sink dependency.
	 */
	void set_replay_sink(IReplaySink* replay_sink) noexcept { m_replay_sink = replay_sink; }

	virtual void input(ControllerInput input) override;
	void update(long game_time); // required for replay

private:

	IGameInputSink* m_game_sink;
	IReplaySink* m_replay_sink;

	std::ifstream replay_stream;
	std::unique_ptr<Replay> replay; // optional
	ReplayEvent next_event;

};
