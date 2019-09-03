#include "game_loop.hpp"
#include "replay.hpp"
#include "arbiter.hpp"
#include "game.hpp"
#include "error.hpp"
#include "context.hpp"
#include "configuration.hpp"
#include <fstream>
#include <random> // DEBUG

GameLoop::GameLoop()
: m_input_devices(),
  m_screen_factory(),
  m_screen(nullptr)
{
	const Configuration& configuration = *the_context.configuration;

	if(NetworkMode::SERVER == configuration.network_mode ||
	   NetworkMode::WITH_SERVER == configuration.network_mode) {
		auto server_channel = make_server_channel(configuration.port);
		auto server_protocol = std::make_unique<ServerProtocol>(std::move(server_channel));
		auto game = std::make_unique<ServerGame>(move(server_protocol));
		m_server.reset(new ServerThread(std::move(game)));
		m_screen_factory.set_server(m_server.get());
		auto draw = std::make_unique<NoDraw>();
		m_screen_factory.set_draw(move(draw));
	}
	else {
		auto draw = std::make_unique<SdlDraw>(the_context.sdl->renderer(), *the_context.assets);
		m_screen_factory.set_draw(move(draw));
	}

	// configure player control
	if(configuration.player_number.has_value()) {
		const int player_number = *configuration.player_number;
		if(2 <= player_number) {
			throw GameException("Cannot control player "
				+ std::to_string(player_number)
				+ ". More than two players are currently not yet supported.");
		}
		m_input_devices.set_player_number(player_number);
	}

	// attach joystick input
	if(configuration.joystick_number.has_value()) {
		const int joystick_number = *configuration.joystick_number;
		const int joysticks_count = SDL_NumJoysticks();
		if(joystick_number < 0 || joystick_number >= joysticks_count) {
			throw GameException("Joystick "
				+ std::to_string(joystick_number)
				+ " not found. There are "
				+ std::to_string(joysticks_count) + " joysticks.");
		}
		JoystickPtr joystick(SDL_JoystickOpen(joystick_number));
		sdlok(joystick.get());
		m_input_devices.set_joystick(std::move(joystick));
	}

	next_screen();
}

void GameLoop::game_loop()
{
	Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
	Uint64 freq = SDL_GetPerformanceFrequency();
	long tick = 0; // current logic tick counter
	Uint64 next_logic = t0 + freq / TPS; // time for next logic update

	while (m_screen) {
		// draw frames as long as logic is up to date
		Uint64 now = SDL_GetPerformanceCounter();
		while (now < next_logic) {
			float fraction = 1.0f - static_cast<float>((next_logic - now) * TPS) / freq;
			assert(fraction >= 0);
			assert(fraction <= 1);

			m_screen->draw(fraction);
			now = SDL_GetPerformanceCounter();

			// yield CPU if we have the time
			if(now < next_logic) {
				Uint64 wait = (next_logic - now) * 1000L / freq; // in ms
				assert(wait <= std::numeric_limits<Uint32>::max());
				SDL_Delay(static_cast<Uint32>(wait));
				now = SDL_GetPerformanceCounter();
			}
		}

		// get different sources of input
		const auto inputs = m_input_devices.poll();
		for(auto i : inputs) {
			// Debug functionality: take control of a certain player.
			// F2 key: take control of player 0
			// F3 key: take control of player 1
			if(Button::DEBUG2 == i.button && ButtonAction::DOWN == i.action)
				m_input_devices.set_player_number(0);
			else if(Button::DEBUG3 == i.button && ButtonAction::DOWN == i.action)
				m_input_devices.set_player_number(1);

			m_screen->input(i);
		}


		if(m_game) m_game->poll();

		// run one frame of local logic
		m_screen->update();

		if(m_screen->done()) {
			next_screen();
			t0 = SDL_GetPerformanceCounter();
			tick = 0;
			next_logic = t0 + freq / TPS;
		}
		else {
			tick++;
			next_logic = t0 + (tick + 1) * freq / TPS;
		}
	}

	Log::info("Game exit.");
}

void GameLoop::next_screen()
{
	if(nullptr != m_screen)
		m_screen->stop();

	if(nullptr == m_screen) {
		NetworkMode mode = the_context.configuration->network_mode;
		if(NetworkMode::SERVER == mode) {
			m_server_screen = m_screen_factory.create_server();
			m_screen = m_server_screen.get();
		}
		else {
			if(!the_context.configuration->server_url.has_value())
				throw GameException("Client mode requires server_url configuration.");

			if(NetworkMode::LOCAL == mode) {
				m_game = std::make_unique<LocalGame>();
			}
			else {
				auto client_channel = make_client_channel(
					the_context.configuration->server_url->c_str(),
					the_context.configuration->port); // network implementation
				auto client_protocol = std::make_unique<ClientProtocol>(std::move(client_channel));
				m_game = std::make_unique<ClientGame>(move(client_protocol));
			}

			m_screen_factory.set_game(m_game.get());

			// If we want to play back a replay, feed it all to the client
			// and let the normal timing in the game loop take care of it.
			auto replay_path = the_context.configuration->replay_path;
			if(replay_path.has_value() &&
			   !std::filesystem::is_regular_file(replay_path.value())) {
				Log::error("Replay not found: %s", replay_path->u8string().c_str());
				replay_path.reset();
			}

			if(replay_path.has_value()) {
				std::ifstream stream{replay_path.value()};
				Journal journal = replay_read(stream);
				GameMeta meta = journal.meta();
				meta.winner = NOONE; // this is currently necessary to prevent early exit
				m_game->game_reset(meta.players);
				m_game->game_start();
				for(InputDiscovered id : journal.inputs()) {
					m_game->game_input(id.input);
				}
				m_game_screen = m_screen_factory.create_game();
				m_screen = m_game_screen.get();

				// We do not copy-save the same game again in replay mode.
				the_context.configuration->autorecord = false;
			}
			else {
				m_menu_screen = m_screen_factory.create_menu();
				m_screen = m_menu_screen.get();
			}
		}

		// debug
		//DrawPink pink_draw(m_sdl_factory, 255, 0, 255);
		//m_pink_screen = std::make_unique<PinkScreen>(std::move(pink_draw));
		//m_screen = m_pink_screen.get();
	} else
	if(ServerScreen* serv = dynamic_cast<ServerScreen*>(m_screen)) {
		m_server_screen.release();
		m_screen = nullptr;
	} else
	if(MenuScreen* menu = dynamic_cast<MenuScreen*>(m_screen)) {
		if(MenuScreen::Result::PLAY == menu->result()) {
			m_game->game_start(); // create game state from meta info
			m_game_screen = m_screen_factory.create_game();
			m_transition_screen = m_screen_factory.create_transition(*menu, *m_game_screen);
			m_screen = m_transition_screen.get();
		} else
		if(MenuScreen::Result::QUIT == menu->result()) {
			m_menu_screen.release();
			m_screen = nullptr;
		}
	} else
	if(GameScreen* game = dynamic_cast<GameScreen*>(m_screen)) {
		if(the_context.configuration->replay_path.has_value()) {
			// After a replay, just exit.
			// NOTE: if we did not, we would have to restore the autorecord config flag.
			m_game_screen.release();
			m_screen = nullptr;
		}
		else {
			// Go back to menu
			m_menu_screen = m_screen_factory.create_menu();
			m_transition_screen = m_screen_factory.create_transition(*game, *m_menu_screen);
			m_screen = m_transition_screen.get();
		}
	} else
	if(TransitionScreen* transition = dynamic_cast<TransitionScreen*>(m_screen)) {
		m_screen = &transition->successor();
		m_transition_screen.release();
		// BUG! We keep the predecessor screen around unnecessarily.
	} else
	if(PinkScreen* pink = dynamic_cast<PinkScreen*>(m_screen)) {
		if(m_pink_screen.get() == pink) {
			m_creme_screen = m_screen_factory.create_pink(250, 220, 220);
			m_transition_screen = m_screen_factory.create_transition(*pink, *m_creme_screen);
			m_screen = m_transition_screen.get();
		}
		else {
			m_pink_screen = m_screen_factory.create_pink(255, 0, 255);
			m_transition_screen = m_screen_factory.create_transition(*pink, *m_pink_screen);
			m_screen = m_transition_screen.get();
		}
	}
	else {
		assert(false); // unknown type of m_screen
	}
}
