#pragma once

#include "game.hpp"
#include "director.hpp"
#include "event.hpp"
#include "state.hpp"
#include "arbiter.hpp"
#include "replay.hpp"
#include "error.hpp"

Rules::Rules(std::unique_ptr<IArbiter> arb)
	: block_director(new BlockDirector()),
	event_hub(new evt::GameEventHub()),
	arbiter(move(arb))
{
	block_director->set_handler(*event_hub);

	if(arbiter)
		event_hub->subscribe(*arbiter);
}

Rules::Rules(Rules&& rhs) noexcept
	: block_director(move(rhs.block_director)),
	event_hub(move(rhs.event_hub)),
	arbiter(move(rhs.arbiter))
{
	block_director->set_handler(*event_hub);
}

Rules& Rules::operator=(Rules&& rhs) noexcept
{
	block_director = std::move(rhs.block_director);
	event_hub = std::move(rhs.event_hub);
	arbiter = move(rhs.arbiter);
	block_director->set_handler(*event_hub);
	return *this;
}

Rules::~Rules() noexcept = default;


GameData::GameData(std::unique_ptr<GameState> state, std::unique_ptr<Journal> journal,
	               std::unique_ptr<IArbiter> arbiter) :
	dials(), state(move(state)), journal(move(journal)), rules(move(arbiter))
{
	enforce(nullptr != this->state);
	enforce(nullptr != this->journal);

	rules.block_director->set_state(*this->state);
}

GameData::~GameData() noexcept = default;
