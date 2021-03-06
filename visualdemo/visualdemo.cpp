#include "visualdemo.hpp"
#include "stage.hpp"
#include "configuration.hpp"
#include "error.hpp"
#include <cassert>
#include <sstream>

namespace
{

std::unique_ptr<IGame> make_game()
{
	auto game = std::make_unique<LocalGame>(std::make_unique<LocalGameFactory>());
	const Rules rules;
	game->game_reset(2, rules, false);
	game->game_start();
	return move(game);
}

}

VisualDemo::VisualDemo() :
	m_game(make_game()),
	m_pit(*m_game->state().pit().at(0)),
	m_draw(new SdlDraw(the_context.sdl->renderer(), *the_context.assets)),
	m_stage(m_game->state(), *m_draw)
{
}

void VisualDemo::put_block(RowCol rc, Color color, Block::State state)
{
	m_pit.spawn_block(color, rc, state);
}

void VisualDemo::common_setup()
{
	// 1 preview row, 2 normal rows, 1 half row, match-ready
	put_block({0, 0}, Color::BLUE);
	put_block({0, 1}, Color::RED);
	put_block({0, 2}, Color::YELLOW);
	put_block({0, 3}, Color::GREEN);
	put_block({0, 4}, Color::PURPLE);
	put_block({0, 5}, Color::ORANGE);

	put_block({-1, 0}, Color::ORANGE);
	put_block({-1, 1}, Color::BLUE);
	put_block({-1, 2}, Color::RED);
	put_block({-1, 3}, Color::YELLOW);
	put_block({-1, 4}, Color::GREEN);
	put_block({-1, 5}, Color::PURPLE);

	put_block({-2, 0}, Color::BLUE);
	put_block({-2, 1}, Color::RED);
	put_block({-2, 2}, Color::YELLOW);
	put_block({-2, 3}, Color::GREEN);
	put_block({-2, 4}, Color::PURPLE);
	put_block({-2, 5}, Color::ORANGE);

	put_block({-3, 2}, Color::RED);
	put_block({-3, 3}, Color::YELLOW);
	put_block({-3, 4}, Color::GREEN);
}

namespace
{

// helper function for generating non-random loot for garbage bricks
std::vector<Color> rainbow(size_t count)
{
	std::vector<Color> result;

	for(int i = 0; i < count; i++)
		result.push_back(static_cast<Color>((i % 6) + 1));

	return result;
}

}

void VisualDemo::scenario_dissolve_garbage()
{
	common_setup();

	auto& garbage = m_pit.spawn_garbage(RowCol{-5, 0}, 6, 2, rainbow(12)); // chain garbage
	garbage.set_state(Physical::State::REST);

	// 3 in a row
	const_cast<Cursor&>(m_pit.cursor()).rc = {-2,2};
	m_game->director().apply_input(Input(PlayerInput{0, 0, GameButton::SWAP, ButtonAction::DOWN}));

	// ticks until block landed, garbage has shrunk, blocks have fallen down
	const int DISSOLVE_T = SWAP_TIME + DISSOLVE_TIME + 2;
	run_game_ticks(DISSOLVE_T);

	// signal to user that test-case time is up
	checkpoint();

	const int DEMO_T = 500; // observation ticks
	run_game_ticks(DEMO_T);
}

void VisualDemo::scenario_match_horizontal()
{
	common_setup();

	m_pit.spawn_block(Color::RED, RowCol{-3, 0}, Block::State::REST);
	m_pit.spawn_block(Color::RED, RowCol{-4, 2}, Block::State::REST);
	const_cast<Cursor&>(m_pit.cursor()).rc = {-4,1};
	m_game->director().apply_input(Input(PlayerInput{0, 0, GameButton::SWAP, ButtonAction::DOWN}));

	// wait until block has swapped above the gap
	const int SWAP_T = SWAP_TIME;
	run_game_ticks(SWAP_T);

	// signal to user that test-case time is up
	checkpoint();

	// wait until block lands and matches
	const int FALL_T = (BLOCK_H + FALL_SPEED - 1) / FALL_SPEED;
	run_game_ticks(FALL_T);

	// signal to user that test-case time is up
	checkpoint();

	const int BREAK_T = BREAK_TIME;
	run_game_ticks(BREAK_T);

	// signal to user that test-case time is up
	checkpoint();

	const int DEMO_T = 200; // observation ticks
	run_game_ticks(DEMO_T);
}

void VisualDemo::scenario_fall_after_shrink()
{
	common_setup();

	auto& garbage = m_pit.spawn_garbage({-6,0}, 6, 2, rainbow(12)); // chain garbage
	garbage.set_state(Physical::State::REST);

	// vertical match just under the garbage
	m_pit.spawn_block(Color::YELLOW, RowCol{-4, 2}, Block::State::REST);

	// 3 in a row
	const_cast<Cursor&>(m_pit.cursor()).rc = {-3,2};
	m_game->director().apply_input(Input(PlayerInput{0, 0, GameButton::SWAP, ButtonAction::DOWN}));

	// ticks until blocks swapped, garbage shrunk, blocks have started to fall down
	const int DISSOLVE_T = SWAP_TIME + DISSOLVE_TIME + 2;
	run_game_ticks(DISSOLVE_T);

	// signal to user that test-case time is up
	checkpoint();

	const int DEMO_T = 500; // observation ticks
	run_game_ticks(DEMO_T);
}

void VisualDemo::scenario_chaining_garbage()
{
	common_setup();

	const int GARBAGE_COLS = 6;
	auto& garbage = m_pit.spawn_garbage(RowCol{-5, 0}, GARBAGE_COLS, 2, rainbow(GARBAGE_COLS * 2)); // chain garbage
	garbage.set_state(Physical::State::REST);
	const_cast<Cursor&>(m_pit.cursor()).rc = {-2, 2};
	// match yellow blocks vertically
	m_game->director().apply_input(Input(PlayerInput{0, 0, GameButton::SWAP, ButtonAction::DOWN}));

	// ticks until block landed, garbage has shrunk, blocks have fallen down
	const int DISSOLVE_T = SWAP_TIME + DISSOLVE_TIME;
	run_game_ticks(DISSOLVE_T);

	// signal to user that test-case time is up
	checkpoint();

	const int DEMO_T = 500; // observation ticks
	run_game_ticks(DEMO_T);
}

void VisualDemo::scenario_panic()
{
	common_setup();

	// complete the test scenario with a block pillar almost to the top
	put_block(RowCol{-4, 3}, Color::RED);
	put_block(RowCol{-5, 3}, Color::YELLOW);
	put_block(RowCol{-6, 3}, Color::GREEN);
	put_block(RowCol{-7, 3}, Color::PURPLE);
	put_block(RowCol{-8, 3}, Color::ORANGE);

	// time it takes for the orange block to reach the top of the pit
	const int TIME_TO_FULL = ROW_HEIGHT / SCROLL_SPEED;

	// discover more blocks and fix them not to match instantly
	run_game_ticks(1);
	m_pit.block_at({1, 2})->col = Color::GREEN;

	// moment before panic
	run_game_ticks(TIME_TO_FULL-1);
	checkpoint();

	// enter panic

	// before panic depleted
	run_game_ticks(PANIC_TIME);
	checkpoint();

	// really over
	run_game_ticks(1);
	checkpoint();

	const int DEMO_T = 500; // observation ticks
	run_game_ticks(DEMO_T);
}

void VisualDemo::scenario_desync()
{
	// this scenario is based on a desynchronization between client and server

	common_setup();
}

void VisualDemo::input(InputFlags& flags)
{
	SDL_Event event;

	flags.step = false;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {

		case SDL_QUIT: flags.abort = true; break;

		case SDL_KEYDOWN:
			if(!event.key.repeat) {
				switch(event.key.keysym.sym) {
					case SDLK_ESCAPE: flags.abort = !flags.abort; break;
					case SDLK_SPACE: flags.pause = !flags.pause; break;
					case SDLK_LCTRL: flags.step = true; break;
				}
			}
			break;
		}
	}
}

void VisualDemo::checkpoint() noexcept
{
	if(!m_indicator.r) { m_indicator.r = 255; return; }
	if(!m_indicator.g) { m_indicator.g = 255; return; }
	if(!m_indicator.b) { m_indicator.b = 255; return; }
	if(!m_indicator.a) { m_indicator.a = 255; return; }
}

void VisualDemo::run_game_ticks(int ticks)
{
	SDL_Renderer& renderer = the_context.sdl->renderer();
	SDL_Rect indicator_rect{400, 20, 40, 40};

	for(int t = 0; t < ticks; t++) {
		if(m_input.pause && !m_input.step) {
			t--;
		} else {
			m_game->synchronurse(t);

			// clear for next frame
			sdlok(SDL_RenderClear(&renderer));
			m_stage.draw(0); // leave finale open for us to draw our indicator
			sdlok(SDL_SetRenderDrawColor(&renderer, m_indicator.r, m_indicator.g, m_indicator.b, SDL_ALPHA_OPAQUE));
			sdlok(SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_NONE));
			sdlok(SDL_RenderFillRect(&renderer, &indicator_rect)); // draw indicator
			sdlok(SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_ADD));
			SDL_RenderPresent(&renderer); // finish rendering
		}

		input(m_input);
		if(m_input.abort)
			return;

		SDL_Delay(SLEEP_MS);
	}
}

/**
	* Continue with the game until the time when the input should be applied.
	* Then apply the input.
	*/
void VisualDemo::run_and_input(PlayerInput input)
{
	// can only apply inputs in the future
	assert(input.game_time > m_game->state().game_time());

	// caution! Inputs for time N+1 are applied when the state time is N.
	run_game_ticks(input.game_time - m_game->state().game_time() - 1);
	m_game->director().apply_input(Input{input});
	run_game_ticks(1);
}

/**
	* Continue with the game until the time when the checkpoint should be
	* taken. Then return a copy of the game state.
	*/
GameState VisualDemo::run_and_checkpoint(long target_time)
{
	// can only continue towards the future
	assert(target_time >= m_game->state().game_time());

	run_game_ticks(target_time - m_game->state().game_time());
	return m_game->state();
}

Options::Options(int argc, const char* argv[])
	: m_scenario_nr(int_option(argc, argv, "--scenario"))
{
}

// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
const char* Options::str_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	const char** itr = std::find(argv, end, option);
	if (itr != end && ++itr != end)
	{
	    return *itr;
	}
	return nullptr;
}

bool Options::bool_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	return std::find(argv, end, option) != end;
}

int Options::int_option(int argc, const char* argv[], const std::string& option)
{
	auto end = argv + argc;
	const char** itr = std::find(argv, end, option);
	if (itr != end && ++itr != end)
	{
	    std::istringstream sstr(*itr);
	    int value = 0;
	    sstr >> value;
	    return value;
	}
	return 0;
}


int main(int argc, char* argv[])
{
	Options options(argc, const_cast<const char**>(argv));

	// need to set up global context
	Configuration configuration;
	configuration.launch_mode = LaunchMode::LOCAL;
	configuration.joystick_number = 0;
	configuration.autorecord = false;
	configuration.log_path = "visualdemo.log";

	configure_context(configuration);
	VisualDemo demo;

	switch(options.scenario_nr()) {
		default:
		case 0:
			demo.scenario_dissolve_garbage();
			break;

		case 1:
			demo.scenario_match_horizontal();
			break;

		case 2:
			demo.scenario_fall_after_shrink();
			break;

		case 3:
			demo.scenario_chaining_garbage();
			break;

		case 4:
			demo.scenario_panic();
			break;

		case 5:
			demo.scenario_desync();
			break;

		case 6:
			demo.draw_demo();
			break;
	}

	return 0;
}
