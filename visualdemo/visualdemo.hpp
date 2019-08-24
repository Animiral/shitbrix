/**
 * A quick&dirty visualizer for game scenarios.
 * It works on specific predetermined situations which are
 * hardcoded into the program and selected with the --scenario N
 * option.
 * The implementation uses only the bare basics of infrastructure
 * required to run the game scenario and display it.
 * Supports ESC for quitting, SPACE for pause/unpause, CTRL for framestep.
 */
#pragma once

#include <memory>
#include <SDL.h>
#include "draw.hpp"
#include "game.hpp"
#include "state.hpp"
#include "director.hpp"
#include "input.hpp"

// don't use SDL main macro
#undef main

class VisualDemo
{

public:
	
	VisualDemo(GameState state);

	void put_block(RowCol rc, Color color = Color::BLUE, Block::State state = Block::State::REST);

	//! Create some blocks to work with
	void common_setup();

	void scenario_panic();
	void scenario_dissolve_garbage();
	void scenario_match_horizontal();
	void scenario_fall_after_shrink();
	void scenario_chaining_garbage();
	void scenario_desync();

private:

	struct InputFlags
	{
		bool pause, step, abort;
	};

	GameState m_state;
	Pit& m_pit;
	Stage m_stage;
	DrawGame m_draw;
	Rules m_rules;
	SDL_Color m_indicator = {0, 0, 0, 0};
	InputFlags m_input{true, true, false};

	const Uint32 SLEEP_MS = 50; // 20 FPS

	static void input(InputFlags& flags);

	//! Signal to the user that some important point
	//! has been reached in the current scenario
	void checkpoint() noexcept;

	void run_game_ticks(int ticks);

	/**
	 * Continue with the game until the time when the input should be applied.
	 * Then apply the input.
	 */
	void run_and_input(PlayerInput input);

	/**
	 * Continue with the game until the time when the checkpoint should be
	 * taken. Then return a copy of the game state.
	 */
	GameState run_and_checkpoint(long target_time);
};

std::unique_ptr<VisualDemo> construct_demo();

class Options
{

public:
	Options(int argc, const char* argv[]);

	const int scenario_nr() const noexcept { return m_scenario_nr; }

private:
	const int m_scenario_nr;

	// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
	const char* str_option(int argc, const char* argv[], const std::string& option);
	bool bool_option(int argc, const char* argv[], const std::string& option);
	int int_option(int argc, const char* argv[], const std::string& option);

};
