#include "game_loop.hpp"

GameLoop::GameLoop(Options options)
: m_options(std::move(options)),
  m_assets(),
  m_audio(Sdl::instance().audio(), m_assets),
  m_screen_factory(m_options, m_assets, m_audio),
  m_screen(),
  m_keyboard()
{
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
			SDL_assert((fraction >= 0) && (fraction <= 1));

			m_screen->draw(fraction);
			now = SDL_GetPerformanceCounter();

			// yield CPU if we have the time
			if(now < next_logic) {
				Uint64 wait = (next_logic - now) * 1000L / freq; // in ms
				SDL_assert(wait <= std::numeric_limits<Uint32>::max());
				SDL_Delay(static_cast<Uint32>(wait));
				now = SDL_GetPerformanceCounter();
			}
		}

		// TODO: handle all different sources of input
		// Should the input object send events directly to the IControllerSink,
		// like it does currently, or should the input object return a list of
		// queued inputs which are then passed on by some controller object?
		m_keyboard.poll();

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
}

void GameLoop::next_screen()
{
	if(nullptr == m_screen) {
		m_menu_screen = m_screen_factory.create_menu();
		m_screen = m_menu_screen.get();

		// debug
		//PinkDraw pink_draw(m_sdl_factory, 255, 0, 255);
		//m_pink_screen = std::make_unique<PinkScreen>(std::move(pink_draw));
		//m_screen = m_pink_screen.get();
	} else
	if(MenuScreen* menu = dynamic_cast<MenuScreen*>(m_screen)) {
		if(MenuScreen::Result::PLAY == menu->result()) {
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
		m_menu_screen = m_screen_factory.create_menu();
		m_transition_screen = m_screen_factory.create_transition(*game, *m_menu_screen);
		m_screen = m_transition_screen.get();
	} else
	if(TransitionScreen* transition = dynamic_cast<TransitionScreen*>(m_screen)) {
		m_screen = &transition->successor();
		m_transition_screen.release();
		// BUG! We keep the predecessor screen around unnecessarily.
	} else
	if(PinkScreen* pink = dynamic_cast<PinkScreen*>(m_screen)) {
		if(m_pink_screen.get() == pink) {
			PinkDraw creme_draw(250, 220, 220);
			m_creme_screen = std::make_unique<PinkScreen>(std::move(creme_draw));
			m_transition_screen = m_screen_factory.create_transition(*pink, *m_creme_screen);
			m_screen = m_transition_screen.get();
		}
		else {
			PinkDraw pink_draw(255, 0, 255);
			m_pink_screen = std::make_unique<PinkScreen>(std::move(pink_draw));
			m_transition_screen = m_screen_factory.create_transition(*pink, *m_pink_screen);
			m_screen = m_transition_screen.get();
		}
	}
	else {
		SDL_assert(false);
	}

	m_keyboard.set_sink(*m_screen);
}
