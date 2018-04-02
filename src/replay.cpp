/**
 * Definitions for replay facilities.
 */

#include "replay.hpp"
#include <vector>
#include <sstream>
#include <cctype>

ReplayEvent ReplayEvent::make_set(std::string name, std::string value) noexcept
{
	ReplayEvent event;
	event.type = Type::SET;
	event.set_name = move(name);
	event.set_value = value;
	return event;
}

ReplayEvent ReplayEvent::make_start() noexcept
{
	ReplayEvent event;
	event.type = Type::START;
	return event;
}

ReplayEvent ReplayEvent::make_input(GameInput input) noexcept
{
	ReplayEvent event;
	event.type = Type::INPUT;
	event.input = input;
	return event;
}

ReplayEvent ReplayEvent::make_end() noexcept
{
	ReplayEvent event;
	event.type = Type::END;
	return event;
}


Journal::Journal(GameMeta meta, IReplaySink* sink)
: m_meta(meta), m_checkpoint({GameState(meta)}), m_sink(sink)
{
	std::ostringstream stream;
	stream << meta.seed;
	do_event(ReplayEvent::make_set("rng_seed", stream.str()));
}

void Journal::reproduce(long target_time, GameState& state)
{
	enforce(m_sink); // cannot reproduce events without a sink set

	// restore the latest checkpoint before the game_time
	auto checkpoint_after = [](const GameState& s, long t) { return s.game_time() > t; };
	auto it = std::lower_bound(m_checkpoint.rbegin(), m_checkpoint.rend(), target_time, checkpoint_after);
	enforce(m_checkpoint.rend() != it); // no earlier checkpoint exists in this Journal
	state = *it;

	// all checkpoints after that become invalid
	auto checkpoint_invalid = [target_time](const GameState& s) { return s.game_time() > target_time; };
	m_checkpoint.erase(std::remove_if(m_checkpoint.begin(), m_checkpoint.end(), checkpoint_invalid), m_checkpoint.end());

	// compile a list of inputs in chronological order
	std::vector<GameInput> inputs;
	auto input_before = [](const GameInput& a, const GameInput& b) { return a.game_time < b.game_time; };
	for(const ReplayEvent& event : m_events) {
		if(ReplayEvent::Type::INPUT == event.type &&
		   event.input.game_time >= state.game_time() &&
		   event.input.game_time < target_time) {
			inputs.insert(std::upper_bound(inputs.begin(), inputs.end(), event.input, input_before), event.input);
		}
	}

	// TODO: sent meta-events
	// Right now, we only send input events because there is still some work to do to separate meta-info from inputs in the replay.

	// send the inputs to my sink
	long game_time = state.game_time();
	for(const GameInput& input : inputs) {
		if(game_time + CHECKPOINT_INTERVAL <= input.game_time) {
			m_checkpoint.push_back(state);
			game_time = input.game_time;
		}
		m_sink->do_event(ReplayEvent::make_input(input));
	}
}

void Journal::poll(long target_time) const
{
	enforce(m_sink); // cannot poll events without a sink

	for(const ReplayEvent& event : m_events) {
		if(ReplayEvent::Type::INPUT == event.type && event.input.game_time == target_time)
			m_sink->do_event(event);
	}
}

namespace
{

const char* replay_event_type_string(ReplayEvent::Type type) noexcept
{
	switch(type) {
		case ReplayEvent::Type::SET: return "set";
		case ReplayEvent::Type::START: return "start";
		case ReplayEvent::Type::INPUT: return "input";
		case ReplayEvent::Type::END: return "end";
		default: assert(false); return nullptr;
	}
}

const char* game_button_string(GameButton button) noexcept
{
	switch(button) {
		case GameButton::NONE: return "none";
		case GameButton::LEFT: return "left";
		case GameButton::RIGHT: return "right";
		case GameButton::UP: return "up";
		case GameButton::DOWN: return "down";
		case GameButton::SWAP: return "swap";
		case GameButton::RAISE: return "raise";
		default: assert(false); return nullptr;
	}
}

const char* button_action_string(ButtonAction action) noexcept
{
	switch(action) {
		case ButtonAction::UP: return "up";
		case ButtonAction::DOWN: return "down";
		default: assert(false); return nullptr;
	}
}

}

void replay_write(std::ostream& stream, const Journal& journal)
{
	for(const ReplayEvent& event : journal.events()) {
		const ReplayEvent::Type event_type = event.type;

		stream << replay_event_type_string(event_type);

		switch(event_type) {
			case ReplayEvent::Type::SET:
				stream << " " << event.set_name << " " << event.set_value;
				break;

			case ReplayEvent::Type::INPUT:
				{
					stream << " " << event.input.game_time
					       << " " << event.input.player
					       << " " << game_button_string(event.input.button)
					       << " " << button_action_string(event.input.action);
				}
				break;

			case ReplayEvent::Type::START:
			case ReplayEvent::Type::END:
				break; // no parameters
		}

		stream << "\n";
	}
}

namespace
{

ReplayEvent::Type string_to_replay_event_type(const std::string& str)
{
	if("set" == str) return ReplayEvent::Type::SET;
	else if("start" == str) return ReplayEvent::Type::START;
	else if("input" == str) return ReplayEvent::Type::INPUT;
	else if("end" == str) return ReplayEvent::Type::END;
	else throw GameException("Invalid event type string.");
}

GameButton string_to_game_button(const std::string& str)
{
	if("left" == str) return GameButton::LEFT;
	else if("right" == str) return GameButton::RIGHT;
	else if("up" == str) return GameButton::UP;
	else if("down" == str) return GameButton::DOWN;
	else if("swap" == str) return GameButton::SWAP;
	else if("raise" == str) return GameButton::RAISE;
	else throw GameException("Invalid game button string.");
}

ButtonAction string_to_button_action(const std::string& str)
{
	if("up" == str) return ButtonAction::UP;
	else if("down" == str) return ButtonAction::DOWN;
	else throw GameException("Invalid button action string.");
}

}

Journal replay_read(std::istream& stream)
{
	// read and parse all events
	std::vector<ReplayEvent> event;

	while(stream && (stream.eof() || stream.peek(), !stream.eof())) {
		std::string line;

		if(!std::getline(stream, line)) {
			throw GameException("Failed to read from replay.");
		}

		ReplayEvent event;
		std::string type_str;
		std::istringstream tokenizer(line);
		tokenizer >> type_str;

		ReplayEvent::Type type = string_to_replay_event_type(type_str);

		switch(type) {

		case ReplayEvent::Type::SET:
			{
				std::string set_name;
				std::string set_value;
				tokenizer >> set_name;

				while(std::isspace(tokenizer.peek()))
					tokenizer.ignore();

				std::getline(tokenizer, set_value);
				event = ReplayEvent::make_set(set_name, set_value);
			}
			break;

		case ReplayEvent::Type::START:
			event = ReplayEvent::make_start();
			break;

		case ReplayEvent::Type::INPUT:
			{
				int game_time;
				int player;
				std::string button_str;
				std::string action_str;

				tokenizer >> game_time >> player >> button_str >> action_str;
				GameButton button = string_to_game_button(button_str);
				ButtonAction action = string_to_button_action(action_str);
				GameInput input{game_time, player, button, action};

				event = ReplayEvent::make_input(input);
			}
			break;

		case ReplayEvent::Type::END:
			event = ReplayEvent::make_end();
			break;

		default:
			assert(false);

		}
	}

	if(stream.bad())
		throw GameException("Something went wrong in reading from replay.");

	// separate meta-data from input data
	GameMeta meta{2, 0, GameMeta::WINNER_UNDECIDED};

	for(const ReplayEvent& ev : event) {
		if(ReplayEvent::Type::SET == ev.type) {
			if("rng_seed" == ev.set_name) {
				std::istringstream stream(ev.set_value);
				unsigned int rng_seed;
				stream >> rng_seed;
				meta.seed = rng_seed;
			}
			else if("winner" == ev.set_name) {
				std::istringstream stream(ev.set_value);
				int winner;
				stream >> winner;
				meta.winner = winner;
			}
		}
	}

	Journal journal{meta};

	// fill the journal with all our input events
	for(const ReplayEvent& ev : event)
		journal.do_event(ev);

	return journal;
}
