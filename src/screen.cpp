#include "screen.hpp"
#include "replay.hpp"
#include "configuration.hpp"
#include "game.hpp"
#include "draw.hpp"
#include "audio.hpp"
#include "error.hpp"
#include <array>
#include <string>
#include <fstream>
#include <sstream>
#include <random>
#include <cassert>

IScreen::IScreen(IDraw& draw) : m_draw(&draw) {}

IScreen::~IScreen() = default;

void IScreen::draw(float dt)
{
	draw_impl(dt);
	m_draw->render();
}


ScreenFactory::ScreenFactory(const GlobalContext& context) noexcept
	: m_context(&context)
{}

IScreen* ScreenFactory::create_default()
{
	enforce(nullptr == m_draw);
	enforce(nullptr == m_game);

	const Configuration& configuration = *m_context->configuration;

	// Set up server thread if applicable
	if(NetworkMode::SERVER == configuration.network_mode ||
	   NetworkMode::WITH_SERVER == configuration.network_mode) {
		auto server_channel = make_server_channel(configuration.port);
		auto server_protocol = std::make_unique<ServerProtocol>(std::move(server_channel));
		auto factory = std::make_unique<ServerGameFactory>(*server_protocol);
		auto sever_game = std::make_unique<ServerGame>(move(factory), move(server_protocol));
		m_server = std::make_unique<ServerThread>(std::move(sever_game));
	}

	// Server setup? done already
	if(NetworkMode::SERVER == configuration.network_mode) {
		m_draw = std::make_unique<NoDraw>();
		m_server_screen = std::make_unique<ServerScreen>(*m_draw, *m_server);
		return m_server_screen.get();
	}

	// Set up drawing
	m_draw = std::make_unique<SdlDraw>(the_context.sdl->renderer(), *the_context.assets);

	m_menu_screen = std::make_unique<MenuScreen>(*m_draw, *m_context);
	return m_menu_screen.get();
}

IScreen* ScreenFactory::create_next(IScreen& predecessor)
{
	// We use our internal knowledge about the different possible types of screen
	// to determine the proper result/followup from each one.
	// NOTE: In this implementation, we keep the predecessor screen around unnecessarily.

	if(ServerScreen* serv = dynamic_cast<ServerScreen*>(&predecessor)) {
		return nullptr;
	} else
	if(MenuScreen * menu = dynamic_cast<MenuScreen*>(&predecessor)) {
		if(MenuScreen::Result::PLAY == menu->result()) {
			// Set up the appropriate game object
			if(NetworkMode::LOCAL == m_context->configuration->network_mode) {
				auto factory = std::make_unique<LocalGameFactory>();
				m_game = std::make_shared<LocalGame>(move(factory));
			}
			else {
				if(!m_context->configuration->server_url.has_value())
					throw GameException("Client mode requires server_url configuration.");

				auto client_channel = make_client_channel(
					the_context.configuration->server_url->c_str(),
					the_context.configuration->port); // network implementation
				auto client_protocol = std::make_unique<ClientProtocol>(std::move(client_channel));
				auto factory = std::make_unique<ClientGameFactory>();
				m_game = std::make_shared<ClientGame>(move(factory), move(client_protocol));
			}

			// If we want to play back a replay, feed it all to the client
			// and let the normal timing in the game loop take care of it.
			auto replay_path = m_context->configuration->replay_path;
			if(replay_path.has_value() &&
				!std::filesystem::is_regular_file(replay_path.value())) {
				Log::error("Replay not found: %s", replay_path->u8string().c_str());
				replay_path.reset();
			}

			if(replay_path.has_value()) {
				std::ifstream stream{ replay_path.value() };
				Journal journal = replay_read(stream);
				GameMeta meta = journal.meta();
				meta.winner = NOONE; // this is currently necessary to prevent early exit
				m_game->game_reset(meta.players);
				m_game->game_start();
				for(Input input : journal.inputs()) {
					m_game->game_input(input);
				}

				// We do not copy-save the same game again in replay mode.
				m_game_screen = std::make_unique<GameScreen>(*m_draw, m_game, m_server.get()); // m_screen_factory.create_game();
				m_game_screen->set_autorecord(false);
				return m_game_screen.get();
			}

			m_pregame_screen = std::make_unique<PregameScreen>(*m_draw, m_game);
			return m_pregame_screen.get();
		}
		else
		if(MenuScreen::Result::QUIT == menu->result()) {
			return nullptr;
		}
	} else
	if(PregameScreen* pregame = dynamic_cast<PregameScreen*>(&predecessor)) {
		if(PregameScreen::Result::PLAY == pregame->result()) {
			m_game_screen.release(); // first delete previous instance
			m_game_screen = std::make_unique<GameScreen>(*m_draw, m_game);
			m_game_screen->set_autorecord(m_context->configuration->autorecord);
			m_transition_screen = std::make_unique<TransitionScreen>(*m_draw, *pregame, *m_game_screen);
			return m_transition_screen.get();
		} else
		if(PregameScreen::Result::QUIT == pregame->result()) {
			m_menu_screen = std::make_unique<MenuScreen>(*m_draw, *m_context);
			m_transition_screen = std::make_unique<TransitionScreen>(*m_draw, *pregame, *m_menu_screen);
			return m_transition_screen.get();
		}
	} else
	if(GameScreen* game = dynamic_cast<GameScreen*>(&predecessor)) {
		if(the_context.configuration->replay_path.has_value()) {
			// After a replay, just exit.
			// NOTE: if we did not, we would have to restore the autorecord config flag.
			return nullptr;
		}
		else {
			// Go back to menu
			m_pregame_screen.release(); // first delete previous instance
			m_pregame_screen = std::make_unique<PregameScreen>(*m_draw, m_game);
			m_transition_screen = std::make_unique<TransitionScreen>(*m_draw, *game, *m_pregame_screen);
			return m_transition_screen.get();
		}
	} else
	if(TransitionScreen* transition = dynamic_cast<TransitionScreen*>(&predecessor)) {
		return &transition->successor();
	} else
	if(PinkScreen* pink = dynamic_cast<PinkScreen*>(&predecessor)) {
		if(m_pink_screen.get() == pink) {
			m_creme_screen = std::make_unique<PinkScreen>(*m_draw, 250, 220, 220);
			m_transition_screen = std::make_unique<TransitionScreen>(*m_draw, *pink, *m_creme_screen);
			return m_transition_screen.get();
		}
		else {
			m_pink_screen = std::make_unique<PinkScreen>(*m_draw, 255, 0, 255);
			m_transition_screen = std::make_unique<TransitionScreen>(*m_draw, *pink, *m_pink_screen);
			return m_transition_screen.get();
		}
	}

	// unknown type of screen
	assert(false);
	return nullptr;
}


PinkScreen::PinkScreen(IDraw& draw, uint8_t r, uint8_t g, uint8_t b) noexcept
	: IScreen(draw), m_r(r), m_g(g), m_b(b)
{}

void PinkScreen::draw_impl(float dt)
{
	m_draw->rect(0, 0, CANVAS_W, CANVAS_H, m_r, m_g, m_b, ALPHA_OPAQUE);
}


MenuScreen::MenuScreen(IDraw& draw, const GlobalContext& context)
	: IScreen(draw),
	m_context(&context),
	m_choice_font(*context.sdl, context.assets->charset(), CHOICE_OUTLINE_COLOR, CHOICE_FILL_COLOR),
	m_active_font(*context.sdl, context.assets->charset(), ACTIVE_OUTLINE_COLOR, ACTIVE_FILL_COLOR),
	m_active_menu(&m_menu[0]),
	m_active(0),
	m_done(false),
	m_result()
{
}

void MenuScreen::update()
{
}

bool MenuScreen::done() const
{
	return m_done;
}

void MenuScreen::input(ControllerAction cinput)
{
	if(ButtonAction::DOWN == cinput.action) {
		switch(cinput.button) {

		case Button::UP:
			if(m_active > 0) {
				m_active--;
				m_context->audio->play(Snd::CHOOSE);
			}
			break;

		case Button::DOWN:
			if((size_t)m_active + 1 < m_active_menu->choice.size()) {
				m_active++;
				m_context->audio->play(Snd::CHOOSE);
			}
			break;

		case Button::A:
			dochoice(m_active_menu->choice[m_active].action);
			break;

		case Button::B:
			dochoice(m_active_menu->choice.back().action); // last choice = back/quit
			break;

		case Button::QUIT:
			m_done = true;
			m_result = Result::QUIT;
			break;

		}
	}
}

void MenuScreen::draw_impl(float dt)
{
	m_draw->gfx(0, 0, Gfx::MENUBG);

	for(int i = 0; i < m_active_menu->choice.size(); i++) {
		if(i == m_active) {
			m_draw->text_fixed(80, 100 + i * BITMAP_FONT_LINEHEIGHT, m_active_font, m_active_menu->choice[i].label);
		}
		else {
			m_draw->text_fixed(60, 100 + i * BITMAP_FONT_LINEHEIGHT, m_choice_font, m_active_menu->choice[i].label);
		}
	}

	if(m_active_menu == &m_menu[1]) { // configuration menu
		std::array<const char*, 4> modes{"Local Game", "Client Mode", "Server Mode", "Host Game"};
		m_draw->text_fixed(360, 100, m_choice_font,
			modes[(size_t)m_context->configuration->network_mode]);

		m_draw->text_fixed(360, 100 + BITMAP_FONT_LINEHEIGHT, m_choice_font,
			m_context->configuration->autorecord ? "Auto-Record Replays" : "No Auto-Record");
	}
}

const wrap::Color MenuScreen::CHOICE_OUTLINE_COLOR{ 111, 31, 148, 255 };
const wrap::Color MenuScreen::CHOICE_FILL_COLOR{ 198, 247, 242, 255 };
const wrap::Color MenuScreen::ACTIVE_OUTLINE_COLOR{ 121, 51, 200, 255 };
const wrap::Color MenuScreen::ACTIVE_FILL_COLOR{ 108, 200, 200, 255 };

void MenuScreen::dochoice(MenuAction action)
{
	Configuration& conf = *m_context->configuration;

	switch(action) {

	case MenuAction::GOMAIN:
		m_active_menu = &m_menu[0];
		m_active = 0;
		m_context->audio->play(Snd::DECLINE);
		break;

	case MenuAction::GOPLAY:
		m_done = true;
		m_result = Result::PLAY;
		m_context->audio->play(Snd::START);
		break;

	case MenuAction::GOCONFIG:
		m_active_menu = &m_menu[1];
		m_active = 0;
		m_context->audio->play(Snd::CONFIRM);
		break;

	case MenuAction::GOQUIT:
		m_done = true;
		m_result = Result::QUIT;
		m_context->audio->play(Snd::DECLINE);
		break;

	case MenuAction::TOGGLE_MODE:
		switch(conf.network_mode) {
		case NetworkMode::LOCAL: conf.network_mode = NetworkMode::CLIENT; break;
		case NetworkMode::CLIENT: conf.network_mode = NetworkMode::WITH_SERVER; break;
		default: case NetworkMode::WITH_SERVER: conf.network_mode = NetworkMode::LOCAL; break;
		}
		m_context->audio->play(Snd::CONFIRM);
		break;

	case MenuAction::TOGGLE_AUTORECORD:
		conf.autorecord = !conf.autorecord;
		m_context->audio->play(Snd::CONFIRM);
		break;

	default:
		assert(0);

	}
}

const MenuScreen::SubMenu MenuScreen::m_menu[] =
{
	{
		{
			{ "Start Game", MenuScreen::MenuAction::GOPLAY },
			{ "Configure", MenuScreen::MenuAction::GOCONFIG },
			{ "Quit", MenuScreen::MenuAction::GOQUIT }
		}
	},
	{
		{
			{ "Game Mode", MenuScreen::MenuAction::TOGGLE_MODE },
			{ "Auto-Record Replay", MenuScreen::MenuAction::TOGGLE_AUTORECORD },
			{ "Back", MenuScreen::MenuAction::GOMAIN }
		}
	}
};

PregameScreen::PregameScreen(IDraw& draw, std::shared_ptr<IGame> game)
	:
	IScreen(draw),
	m_time(0),
	m_done(false),
	m_result(),
	m_draw(&draw),
	m_game(move(game))
{
	enforce(nullptr != m_game);

	Log::info("PregameScreen turn on.");

	// When the game starts, this screen is finished.
	m_game->after_start([this] {
		m_result = Result::PLAY;
		m_done = true;
	});
}

PregameScreen::~PregameScreen() noexcept
{
	m_game->after_start(nullptr); // unregister my handler (potential dangling this ptr)
}

void PregameScreen::update()
{
	m_game->poll();
	m_time++;
}

void PregameScreen::input(ControllerAction cinput)
{
	if(ButtonAction::DOWN == cinput.action) {
		if(Button::A == cinput.button) {
			m_game->game_reset(2);
			m_game->game_start();
		} else
		if(Button::QUIT == cinput.button) {
			m_result = Result::QUIT;
			m_done = true;
		}
	}
}

void PregameScreen::draw_impl(float dt)
{
	m_draw->gfx(0, 0, Gfx::TITLE);
}


GameScreen::GameScreen(IDraw& draw, std::shared_ptr<IGame> game, ServerThread* server) :
	IScreen(draw),
	m_phase(Phase::INTRO),
	m_time(0),
	m_done(false),
	m_stage(new Stage(game->state(), *m_draw)),
	m_game(move(game)),
	m_server(server)
{
	assert(m_stage);
	enforce(nullptr != m_game);
	enforce(m_game->switches().ingame);

	Log::info("GameScreen turn on.");

	// prepare to clear stage's dangling state pointer whenever necessary
	m_game->before_reset([this] {
		// preserve the replay before it is gone
		autorecord_replay();
		m_stage->set_state(nullptr);
		m_done = true;
		m_game->before_reset(nullptr); // don't call this handler twice
	});
	m_stage->subscribe_to(m_game->hub());
}

GameScreen::~GameScreen() noexcept
{
	// The network, which can outlive this screen, must not be left with a
	// dangling pointer to our member relay.
	if(m_game->switches().ingame) {
		m_stage->unsubscribe_from(m_game->hub());
	}

	m_game->before_reset(nullptr); // unregister my handler (potential dangling this ptr)
}

void GameScreen::update()
{
	// check pause
	if(0 == m_game->switches().speed)
		return;

	advance_tick();
}

void GameScreen::input(ControllerAction cinput)
{
	// At the moment, a game reset means that the game state becomes unusable.
	// -> no more inputs when we're finished.
	if(m_done)
		return;

	// Generally, inputs to the game screen are given to the game object.
	// From there, it might be sent over the network and acknowledged by the
	// server. In any case, the input will finally arrive in the Journal,
	// from which we get them back to display the game on the screen.
	enforce(Button::NONE != cinput.button);

	switch(cinput.button) {
		case Button::LEFT:
		case Button::RIGHT:
		case Button::UP:
		case Button::DOWN:
		case Button::A:
		case Button::B:
		{
			// Forward game input to the network (or other input handler).
			// PlayerInput arrives in the m_phase only after a round trip through
			// the Journal, which consists of server-approved inputs.
			std::optional<PlayerInput> oinput = controller_to_input(cinput);
			if(oinput.has_value()) {
				// TODO: network should assign the actual input time
				oinput->game_time = m_time + 1; // input applies to next frame
				m_game->game_input(Input{oinput.value()});
			}
		}
			break;

		case Button::PAUSE:
			// this is a toggle
			if(ButtonAction::DOWN != cinput.action)
				break;

			if(0 == m_game->switches().speed)
				m_game->set_speed(1);
			else
				m_game->set_speed(0);
			break;

		case Button::RESET:
		{
			// In replay playback mode, there is no reset (only quit).
			if(the_context.configuration->replay_path.has_value())
				break;

			// Only reset once
			if(ButtonAction::DOWN != cinput.action)
				break;

			m_game->game_reset(2);
		}
			break;

		case Button::QUIT:
			autorecord_replay();
			m_done = true;
			break;

		case Button::DEBUG1:
			// this is a toggle
			if(ButtonAction::DOWN != cinput.action)
				break;

			m_stage->toggle_pit_debug_overlay();
			m_stage->toggle_pit_debug_highlight();
			break;

		case Button::DEBUG2:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode)
				advance_tick();
			break;

		case Button::DEBUG3:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode)
				for(int i = 0; i < 8; i++) advance_tick();
			break;

		case Button::DEBUG4:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode) {
				m_game->director().debug_no_gameover ^= true;
				// debug_print_pit(stage->pits()[0]->pit);
			}
			break;

		case Button::DEBUG5:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode) {
				m_game->director().debug_spawn_garbage(6, 2);
				// debug_print_pit(stage->pits()[1]->pit);
			}
			break;

		case Button::NONE:
		default:
			assert(false);

	}
}

void GameScreen::draw_impl(float dt)
{
	m_stage->draw(dt);
}

void GameScreen::advance_tick()
{
	m_game->poll();

	// logic-independent stage effects
	m_stage->update();

	// At the moment, a game reset means that the game state becomes unusable.
	// -> no more updates when we're finished.
	if(!m_game->switches().ingame)
		return;

	m_time++;

	switch(m_phase) {

	case Phase::INTRO:
		update_intro();
		break;

	case Phase::PLAY:
		update_play();
		break;

	}
}

void GameScreen::update_intro()
{
	float black_fraction = 1.f - (static_cast<float>(m_time) / INTRO_TIME);
	m_stage->fade(black_fraction);

	if(INTRO_TIME <= m_time) {
		m_phase = Phase::PLAY;
		m_time = m_game->state().game_time();
	}
}

void GameScreen::update_play()
{
	// detect game over
	const int winner = m_game->journal().meta().winner;
	if(NOONE != winner) {
		m_phase = Phase::RESULT;
		m_stage->show_result(winner);
		autorecord_replay();
		return; // skip the usual; we don't need more game logic
	}

	// run game logic until the target time, considering even retcon inputs
	m_game->synchronurse(m_time);
}

void GameScreen::autorecord_replay() const
{
	if(m_autorecord)
		replay_write(m_game->journal());
}

ServerScreen::ServerScreen(IDraw& draw, ServerThread& server) noexcept
	: IScreen(draw), m_server(&server), m_done(false)
{
}

ServerScreen::~ServerScreen() noexcept
{
	try {
		m_server->exit();
	}
	catch(const std::exception& ex) {
		show_error(ex);
	}
	catch(...) {
		Log::error("Unknown exception occurred.");
	}
}

void ServerScreen::input(ControllerAction cinput)
{
	if(Button::QUIT == cinput.button)
		m_done = true;
}


TransitionScreen::TransitionScreen(IDraw& draw, IScreen& predecessor, IScreen& successor)
	:
	IScreen(draw),
	m_predecessor(predecessor),
	m_successor(successor),
	m_predecessor_canvas(draw.create_canvas()),
	m_successor_canvas(draw.create_canvas()),
	m_time(0)
{}

void TransitionScreen::update()
{
	m_predecessor.update();
	m_successor.update();
	m_time++;
}

void TransitionScreen::draw_impl(float dt)
{
	m_predecessor_canvas->use_as_target();
	m_predecessor.draw_impl(dt);

	m_successor_canvas->use_as_target();
	m_successor.draw_impl(dt);

	int progress_px = CANVAS_W * m_time / TRANSITION_TIME;

	// swipe transition: successor screen enters from the left.
	m_draw->reset_target();

	m_draw->clip(0, 0, progress_px, CANVAS_H);
	m_successor_canvas->draw();

	m_draw->clip(progress_px, 0, CANVAS_W-progress_px, CANVAS_H);
	m_predecessor_canvas->draw();

	m_draw->unclip();
}

