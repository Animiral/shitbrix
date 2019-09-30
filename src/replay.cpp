/**
 * Definitions for replay facilities.
 */

#include "replay.hpp"
#include "error.hpp"
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <cassert>

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

ReplayRecord ReplayRecord::make_input(Input input) noexcept
{
	ReplayRecord record;
	record.type = Type::INPUT;
	record.input = input;
	return record;
}


Journal::Journal(GameMeta meta, GameState state0)
: m_meta(meta), m_checkpoint({state0}), m_earliest_undiscovered(NO_UNDISCOVERED)
{
}

namespace
{

/**
 * Helper for searching the journal's sorted inputs with the standard libary.
 */
struct CompareInputTime
{
	bool operator()(const Input& input, long time) noexcept
	{
		return input.game_time() < time;
	}
	bool operator()(long time, const Input& input) noexcept
	{
		return time < input.game_time();
	}
};

}

InputSpan Journal::get_inputs(long game_time) noexcept
{
	enforce(0 < game_time);

	return equal_range(m_inputs.begin(), m_inputs.end(), game_time, CompareInputTime{});
}

void Journal::add_input(Input input)
{
	Log::trace("Journal add_input: %s.", std::string(input).c_str());

	const long itime = input.game_time();
	enforce(itime > 0);

	if(m_earliest_undiscovered > itime)
		m_earliest_undiscovered = itime;

	// ordered insert of the input into the record
	const auto after = upper_bound(m_inputs.begin(), m_inputs.end(), itime, CompareInputTime{});
	m_inputs.insert(after, input);

	// prune checkpoints to maintain integrity
	auto is_obsolete = [itime](const GameState& s) { return s.game_time() >= itime; };
	auto obs_it = std::remove_if(m_checkpoint.begin(), m_checkpoint.end(), is_obsolete);
	m_checkpoint.erase(obs_it, m_checkpoint.end());
}

void Journal::retract(long time)
{
	struct IsRetractable
	{
		bool operator()(const PlayerInput& ) { return false; }
		bool operator()(const SpawnBlockInput& ) { return true; }
		bool operator()(const SpawnGarbageInput& ) { return true; }
	};

	auto is_retractable = [time](Input i) { return i.game_time() > time && i.visit(IsRetractable{}); };
	auto new_end = std::remove_if(m_inputs.begin(), m_inputs.end(), is_retractable);
	m_inputs.erase(new_end, m_inputs.end());

	// we have "undiscovered" the potential inputs that we might want to generate again.
	m_earliest_undiscovered = time + 1;
}

void Journal::set_winner(int winner) noexcept
{
	enforce(winner == NOONE || (winner >= 0 && winner < m_meta.players));
	m_meta.winner = winner;
}

void Journal::add_checkpoint(GameState&& checkpoint)
{
	Log::trace("Journal add_checkpoint(time=%d).", checkpoint.game_time());

	assert(m_checkpoint.size() > 0);
	// we should only ever insert new checkpoints if there is new history
	assert(checkpoint.game_time() > m_checkpoint.back().game_time());

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

}

void replay_stream(std::ostream& stream, const Journal& journal)
{
	stream << replay_record_type_string(ReplayRecord::Type::START) << "\n";

	GameMeta meta = journal.meta();
	stream << replay_record_type_string(ReplayRecord::Type::META)
	       << " " << meta.to_string() << "\n";

	for(const auto& input : journal.inputs()) {
		stream << replay_record_type_string(ReplayRecord::Type::INPUT)
		       << " " << std::string(input) << "\n";
	}
}

void replay_write(const Journal& journal)
{
	using clock = std::chrono::system_clock;
	auto now = clock::now();
	std::time_t time_now = clock::to_time_t(now);
	struct tm ltime = *std::localtime(&time_now);

	if(0 != errno)
		throw GameException("Failed to get localtime for journal file name.");

	if(!std::filesystem::is_directory("replay"))
		return; // creating the replay directory is the user's opt-in

	std::ostringstream time_stream;
	time_stream << std::put_time(&ltime, "replay/%Y-%m-%d_%H-%M.txt");
	std::filesystem::path path{time_stream.str()};

	// We never overwrite autorecords.
	if(std::filesystem::exists(path)) {
		// Backup plan: include seconds.
		time_stream.str("");
		time_stream << std::put_time(&ltime, "replay/%Y-%m-%d_%H-%M-%S.txt");
		path = time_stream.str();
	}

	if(!std::filesystem::exists(path)) {
		std::ofstream stream(path);
		replay_stream(stream, journal);
	}

	// If the seconds-precision path already exists, we prefer the earlier
	// file as it is more likely to contain a full game.
}

namespace
{

ReplayRecord::Type string_to_replay_record_type(const std::string& type_string)
{
	if("start" == type_string) return ReplayRecord::Type::START;
	else if("meta" == type_string) return ReplayRecord::Type::META;
	else if("input" == type_string) return ReplayRecord::Type::INPUT;
	else throw ReplayException("Invalid record type string: \"" + type_string + "\"");
}

}

Journal replay_read(std::istream& stream)
{
	// Replay contents
	GameMeta meta{0, 0, true};
	std::vector<Input> inputs;

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
			{
				tokenizer >> std::ws;
				std::string meta_string;
				std::getline(tokenizer, meta_string);

				try {
					meta = GameMeta::from_string(meta_string);
				}
				catch(GameException ex) {
					throw ReplayException("Failed to parse meta.",
						std::make_unique<GameException>(std::move(ex)));
				}
			}
			break;

		case ReplayRecord::Type::INPUT:
			{
				tokenizer >> std::ws;
				std::string input_string;
				std::getline(tokenizer, input_string);

				Input input;

				try {
					input = Input(input_string);
				}
				catch(GameException ex) {
					throw ReplayException("Failed to parse input.",
					                      std::make_unique<GameException>(std::move(ex)));
				}

				if(input.game_time() < prev_time)
					throw ReplayException("Inputs out of order.");

				inputs.push_back(input);
				prev_time = input.game_time();
			}
			break;

		default:
			// TODO: ReplayException unknown replay record type
			assert(false);

		}
	}

	if(stream.bad())
		throw ReplayException("Something went wrong in reading from replay.");

	// separate meta-data from input data
	GameState state0{meta};
	Journal journal{meta, std::move(state0)};

	for(Input gi : inputs)
		journal.add_input(gi);

	return journal;
}
