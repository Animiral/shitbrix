/**
 * A quick&dirty visualizer for game scenarios.
 * It works on specific predetermined situations which are
 * hardcoded into the program and selected with the --scenario N
 * option.
 * The implementation uses only the bare basics of infrastructure
 * required to run the game scenario and display it.
 * Supports ESC for quitting, SPACE for pause/unpause, CTRL for framestep.
 */
#include "sdl_context.hpp"
#include "draw.hpp"
#include "stage.hpp"
#include "director.hpp"
#include <sstream>

class VisualDemo
{

public:

	void SetUp()
	{
		pit = std::make_unique<Pit>(LPIT_LOC);
		cursor = std::make_unique<Cursor>(RowCol{3,3});

		dummy_pit = std::make_unique<Pit>(RPIT_LOC);
		dummy_cursor = std::make_unique<Cursor>(RowCol{3,3});

		const int SEED = 0;
		rndgen = std::make_shared<std::mt19937>(SEED);
		director = std::make_unique<BlockDirector>(*pit, rndgen);

		m_draw = std::make_unique<DrawGame>(context);
		m_draw->add_pit(*pit, *cursor);
		m_draw->add_pit(*dummy_pit, *dummy_cursor); // same thing twice for debug
		m_draw->show_cursors(false);
	}

	// virtual void TearDown() {}

	void common_setup(); // creates some blocks to work with, like BlockDirectorTest::SetUp()

	void scenario_dissolve_garbage();
	void scenario_fall_after_shrink();

private:

	SdlContext context;
	std::unique_ptr<Pit> pit;
	std::unique_ptr<Cursor> cursor;

	std::unique_ptr<Pit> dummy_pit;
	std::unique_ptr<Cursor> dummy_cursor;

	RndGen rndgen;
	std::unique_ptr<BlockDirector> director;
	std::unique_ptr<DrawGame> m_draw;

	const Uint32 SLEEP_MS = 50; // 20 FPS

	void draw()
	{
		context.drawGfx(Point{0,0}, Gfx::BACKGROUND);
		m_draw->draw_all();
		context.render();
	}

	void input(bool& abort, bool& pause, bool& step)
	{
		abort = false;
		step = false;

		SDL_Event event;

		while(SDL_PollEvent(&event)) {
			switch(event.type) {

			case SDL_QUIT:
				abort = true;
				break;

			case SDL_KEYDOWN:
				if(!event.key.repeat) {
					switch(event.key.keysym.sym) {
						case SDLK_ESCAPE:
							abort = true;
							break;

						case SDLK_SPACE:
							pause = !pause;
							break;

						case SDLK_LCTRL:
							step = true;
							break;
					}
					// ControllerInput input = key_to_controller();

					// if(input.button != Button::NONE)
					// 	m_sink.input(input);
				}
				break;
			}
		}
	}

	void run_game_ticks(int ticks)
	{
		bool pause = false;
		bool step = false;

		for(int t = 0; t < ticks; t++) {
			if(pause && !step) {
				t--;
			} else {
				director->update(context);
				pit->update(context);
				draw();
			}

			bool abort = false;
			input(abort, pause, step);
			if(abort)
				return;

			SDL_Delay(SLEEP_MS);
		}
	}
};

void VisualDemo::common_setup()
{
	// 1 preview row, 2 normal rows, 1 half row, match-ready
	pit->spawn_block(Block::Color::BLUE, RowCol{0, 0}, Block::State::REST);
	pit->spawn_block(Block::Color::RED, RowCol{0, 1}, Block::State::REST);
	pit->spawn_block(Block::Color::YELLOW, RowCol{0, 2}, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, RowCol{0, 3}, Block::State::REST);
	pit->spawn_block(Block::Color::PURPLE, RowCol{0, 4}, Block::State::REST);
	pit->spawn_block(Block::Color::ORANGE, RowCol{0, 5}, Block::State::REST);

	pit->spawn_block(Block::Color::ORANGE, RowCol{-1, 0}, Block::State::REST);
	pit->spawn_block(Block::Color::BLUE, RowCol{-1, 1}, Block::State::REST);
	pit->spawn_block(Block::Color::RED, RowCol{-1, 2}, Block::State::REST);
	pit->spawn_block(Block::Color::YELLOW, RowCol{-1, 3}, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, RowCol{-1, 4}, Block::State::REST);
	pit->spawn_block(Block::Color::PURPLE, RowCol{-1, 5}, Block::State::REST);

	pit->spawn_block(Block::Color::BLUE, RowCol{-2, 0}, Block::State::REST);
	pit->spawn_block(Block::Color::RED, RowCol{-2, 1}, Block::State::REST);
	pit->spawn_block(Block::Color::YELLOW, RowCol{-2, 2}, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, RowCol{-2, 3}, Block::State::REST);
	pit->spawn_block(Block::Color::PURPLE, RowCol{-2, 4}, Block::State::REST);
	pit->spawn_block(Block::Color::ORANGE, RowCol{-2, 5}, Block::State::REST);

	pit->spawn_block(Block::Color::RED, RowCol{-3, 2}, Block::State::REST);
	pit->spawn_block(Block::Color::YELLOW, RowCol{-3, 3}, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, RowCol{-3, 4}, Block::State::REST);
}

void VisualDemo::scenario_dissolve_garbage()
{
	common_setup();

	auto& garbage = pit->spawn_garbage(RowCol{-5, 0}, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	RowCol lrc = RowCol{-2,2};
	RowCol rrc = RowCol{-2,3};
	auto& left_block = *pit->block_at(lrc);
	auto& right_block = *pit->block_at(rrc);

	// 3 in a row
	left_block.swap_toward(rrc);
	right_block.swap_toward(lrc);
	pit->swap(left_block, right_block);

	// ticks until block landed, garbage has shrunk, blocks have fallen down
	const int DISSOLVE_T = Block::SWAP_TIME + Garbage::DISSOLVE_TIME + 2;
	run_game_ticks(DISSOLVE_T);

	// signal to user that test-case time is up
	dummy_pit->spawn_block(Block::Color::PURPLE, RowCol{-5,3}, Block::State::REST);

	const int DEMO_T = 500; // observation ticks
	run_game_ticks(DEMO_T);
}

void VisualDemo::scenario_fall_after_shrink()
{
	common_setup();

	RowCol garbage_rc{-6,0};
	auto& garbage = pit->spawn_garbage(garbage_rc, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	// vertical match just under the garbage
	pit->spawn_block(Block::Color::YELLOW, RowCol{-4, 2}, Block::State::REST);

	RowCol lrc = RowCol{-3,2};
	RowCol rrc = RowCol{-3,3};
	auto& left_block = *pit->block_at(lrc);
	auto& right_block = *pit->block_at(rrc);

	// 3 in a row
	left_block.swap_toward(rrc);
	right_block.swap_toward(lrc);
	pit->swap(left_block, right_block);

	// ticks until blocks swapped, garbage shrunk, blocks have started to fall down
	const int DISSOLVE_T = Block::SWAP_TIME + Garbage::DISSOLVE_TIME + 2;
	run_game_ticks(DISSOLVE_T);

	// signal to user that test-case time is up
	dummy_pit->spawn_block(Block::Color::PURPLE, RowCol{-5,3}, Block::State::REST);

	const int DEMO_T = 500; // observation ticks
	run_game_ticks(DEMO_T);
}

class Options
{

public:
	Options(int argc, const char* argv[])
		: m_scenario_nr(int_option(argc, argv, "--scenario"))
	{
	}

	const int scenario_nr() const { return m_scenario_nr; }

private:
	const int m_scenario_nr;

	// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
	const char* str_option(int argc, const char* argv[], const std::string& option)
	{
		auto end = argv + argc;
	    const char** itr = std::find(argv, end, option);
	    if (itr != end && ++itr != end)
	    {
	        return *itr;
	    }
	    return nullptr;
	}

	bool bool_option(int argc, const char* argv[], const std::string& option)
	{
		auto end = argv + argc;
	    return std::find(argv, end, option) != end;
	}

	int int_option(int argc, const char* argv[], const std::string& option)
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

};

int main(int argc, const char* argv[])
{
	Options options(argc, argv);
	VisualDemo demo;
	demo.SetUp();

	switch(options.scenario_nr()) {
		default:
		case 0:
			demo.scenario_dissolve_garbage();
			break;

		case 1:
			demo.scenario_fall_after_shrink();
			break;
	}
}
