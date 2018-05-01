/**
 * Definitions for replay facilities.
 */

#include "replay.hpp"
#include "error.hpp"
#include <vector>
#include <sstream>
#include <cctype>

ReplayRecord ReplayRecord::make_start() noexcept
{
	ReplayRecord record;
	record.type = Type::START;
	return record;
}

ReplayRecord ReplayRecord::make_meta(GameMeta meta) noexcept
{
	ReplayRecord record;
	record.type = Type::META;
	record.meta = std::move(meta);
	return record;
}

ReplayRecord ReplayRecord::make_input(GameInput input) noexcept
{
	ReplayRecord record;
	record.type = Type::INPUT;
	record.input = input;
	return record;
}


Journal::Journal(GameMeta meta, GameState&& state0)
: m_meta(meta), m_checkpoint({state0}), m_earliest_undiscovered(1)
{
}

//void Journal::reproduce(long target_time, GameState& state)
//{
//	enforce(m_sink); // cannot reproduce events without a sink set
//
//	// restore the latest checkpoint before the game_time
//	auto checkpoint_after = [](const GameState& s, long t) { return s.game_time() > t; };
//	auto it = std::lower_bound(m_checkpoint.rbegin(), m_checkpoint.rend(), target_time, checkpoint_after);
//	enforce(m_checkpoint.rend() != it); // no earlier checkpoint exists in this Journal
//	state = *it;
//
//	// all checkpoints after that become invalid
//	auto checkpoint_invalid = [target_time](const GameState& s) { return s.game_time() > target_time; };
//	m_checkpoint.erase(std::remove_if(m_checkpoint.begin(), m_checkpoint.end(), checkpoint_invalid), m_checkpoint.end());
//
//	// compile a list of inputs in chronological order
//	std::vector<GameInput> inputs;
//	auto input_before = [](const GameInput& a, const GameInput& b) { return a.game_time < b.game_time; };
//	for(const ReplayRecord& record : m_events) {
//		if(ReplayRecord::Type::INPUT == record.type &&
//		   record.input.game_time >= state.game_time() &&
//		   record.input.game_time < target_time) {
//			inputs.insert(std::upper_bound(inputs.begin(), inputs.end(), record.input, input_before), record.input);
//		}
//	}
//
//	// TODO: sent meta-events
//	// Right now, we only send input events because there is still some work to do to separate meta-info from inputs in the replay.
//
//	// send the inputs to my sink
//	long game_time = state.game_time();
//	for(const GameInput& input : inputs) {
//		if(game_time + CHECKPOINT_INTERVAL <= input.game_time) {
//			m_checkpoint.push_back(state);
//			game_time = input.game_time;
//		}
//		m_sink->do_event(ReplayRecord::make_input(input));
//	}
//}

namespace
{

auto greater_time(long cutoff) noexcept
{
	return [cutoff](const InputDiscovered& id) { return id.input.game_time > cutoff; };
}

}

GameInputSpan Journal::discover_inputs(long start_time, long end_time) noexcept
{
	enforce(start_time <= end_time);

	auto begin = std::find_if(m_inputs.begin(), m_inputs.end(), greater_time(start_time - 1));
	auto end = std::find_if(begin, m_inputs.end(), greater_time(end_time));

	for(auto it = begin; it != end; ++it)
		it->discovered = true;

	return {begin, end};
}

void Journal::add_input(GameInput input)
{
	enforce(input.game_time > 0);

	m_inputs.push_back(InputDiscovered{input, false});
}

void Journal::set_winner(int winner) noexcept
{
	enforce(winner == GameMeta::WINNER_UNDECIDED ||
	        (winner >= 0 && winner < m_meta.players));
	m_meta.winner = winner;
}

void Journal::add_checkpoint(GameState&& checkpoint)
{
	assert(m_checkpoint.size() > 0);
	// for the time being, we can insert checkpoints only in order
	enforce(checkpoint.game_time() > m_checkpoint.back().game_time());

	m_checkpoint.emplace_back(checkpoint);
}

const GameState& Journal::checkpoint_before(long game_time) const
{
	enforce(game_time > 0);

	const auto end = m_checkpoint.rend();
	const auto it = std::find_if(m_checkpoint.rbegin(), end,
		[game_time](const GameState& s) { return s.game_time() < game_time; });

	assert(it != end);
	return *it;
}

namespace
{

const char* replay_record_type_string(ReplayRecord::Type type) noexcept
{
	switch(type) {
		case ReplayRecord::Type::START: return "start";
		case ReplayRecord::Type::META: return "meta";
		case ReplayRecord::Type::INPUT: return "input";
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
		case ButtonAction::UP: return "release";
		case ButtonAction::DOWN: return "press";
		default: assert(false); return nullptr;
	}
}

}

void replay_write(std::ostream& stream, const Journal& journal)
{
	stream << replay_record_type_string(ReplayRecord::Type::START) << "\n";

	GameMeta meta = journal.meta();
	stream << replay_record_type_string(ReplayRecord::Type::META)
	       << " " << meta.players
	       << " " << meta.seed
	       << " " << meta.winner << "\n";

	for(const auto& id : journal.inputs()) {
		stream << replay_record_type_string(ReplayRecord::Type::INPUT)
		       << " " << id.input.game_time
		       << " " << id.input.player
		       << " " << game_button_string(id.input.button)
		       << " " << button_action_string(id.input.action)
		       << "\n";
	}
}

namespace
{

ReplayRecord::Type string_to_replay_record_type(const std::string& str)
{
	if("start" == str) return ReplayRecord::Type::START;
	else if("meta" == str) return ReplayRecord::Type::META;
	else if("input" == str) return ReplayRecord::Type::INPUT;
	else throw ReplayException("Invalid event type string.");
}

GameButton string_to_game_button(const std::string& str)
{
	if("left" == str) return GameButton::LEFT;
	else if("right" == str) return GameButton::RIGHT;
	else if("up" == str) return GameButton::UP;
	else if("down" == str) return GameButton::DOWN;
	else if("swap" == str) return GameButton::SWAP;
	else if("raise" == str) return GameButton::RAISE;
	else throw ReplayException("Invalid game button string.");
}

ButtonAction string_to_button_action(const std::string& str)
{
	if("release" == str) return ButtonAction::UP;
	else if("press" == str) return ButtonAction::DOWN;
	else throw ReplayException("Invalid button action string.");
}

}

Journal replay_read(std::istream& stream)
{
	// Replay contents
	GameMeta meta{0, 0};
	std::vector<GameInput> input;

	// We read only the first game replay. Therefore, we must read
	// everything following the first start-record.
	int start_count = 0;
	int prev_time = 0; // time of previous input, for order check

	while(stream && (stream.eof() || stream.peek(), !stream.eof()) && (start_count <= 1)) {
		std::string line;

		if(!std::getline(stream, line)) {
			throw ReplayException("Failed to read from replay.");
		}

		ReplayRecord record;
		std::string type_str;
		std::istringstream tokenizer(line);
		tokenizer >> type_str;

		ReplayRecord::Type type = string_to_replay_record_type(type_str);

		switch(type) {

		case ReplayRecord::Type::START:
			start_count++;
			break;

		case ReplayRecord::Type::META:
			tokenizer >> meta.players >> meta.seed >> meta.winner;
			break;

		case ReplayRecord::Type::INPUT:
			{
				int game_time;
				int player;
				std::string button_str;
				std::string action_str;

				tokenizer >> game_time >> player >> button_str >> action_str;
				GameButton button = string_to_game_button(button_str);
				ButtonAction action = string_to_button_action(action_str);

				if(game_time < prev_time)
					throw ReplayException("Inputs out of order.");

				input.push_back(GameInput{game_time, player, button, action});
				prev_time = game_time;
			}
			break;

		default:
			assert(false);

		}
	}

	if(stream.bad())
		throw ReplayException("Something went wrong in reading from replay.");

	// separate meta-data from input data
	Journal journal{meta, GameState{meta}};

	for(GameInput gi : input)
		journal.add_input(gi);

	return journal;
}
