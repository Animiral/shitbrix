/**
 * input.hpp
 * Functions for converting user actions into game actions.
 */
#pragma once

#include "globals.hpp"
#include "sdl_helper.hpp"
#include <optional>
#include <variant>
#include <vector>

/**
 * This is an input originally performed by a player.
 * Holds one in-game action and the number of the player who pressed it.
 */
struct PlayerInput
{
	static const long TIME_ASAP = -1; //!< this input should be part of the next update

	long game_time;      //!< time when this input takes effect
	int player;          //!< 0-based player index
	GameButton button;
	ButtonAction action;

	/**
	 * Since @c PlayerInputs frequently need to be sent over the network or stored
	 * in a replay file, they have a canonical string representation.
	 */
	std::string to_string() const;

	/**
	 * Return the @c PlayerInput from the string representation.
	 * @throw GameException if the string is not recognized.
	 */
	static PlayerInput from_string(std::string input_string);
};

/**
 * This is an arbiter input, which, unlike PlayerInput, is triggered by the
 * rules implementation and not by the player.
 * Holds one directive from the arbiter to introduce another row of
 * blocks in PREVIEW state.
 */
struct SpawnBlockInput
{
	long game_time;         //!< time when this input takes effect
	int player;             //!< 0-based player index
	int row;                //!< which row to place the new blocks in
	std::array<Color, PIT_COLS> colors; //!< new block colors from left to right

	/**
	 * Since @c SpawnBlockInput frequently need to be sent over the network or stored
	 * in a replay file, they have a canonical string representation.
	 */
	std::string to_string() const;

	/**
	 * Return the @c SpawnBlockInput from the string representation.
	 * @throw GameException if the string is not recognized.
	 */
	static SpawnBlockInput from_string(std::string input_string);
};

/**
 * This is an arbiter input, which, unlike PlayerInput, is triggered by the
 * rules implementation and not by the player.
 * Holds one directive from the arbiter to introduce a garbage brick.
 */
struct SpawnGarbageInput
{
	long game_time;         //!< time when this input takes effect
	int player;             //!< 0-based player index of the victim
	int rows;               //!< how many rows the block spans
	int columns;            //!< how many columns the block spans
	RowCol rc;              //!< where to place the garbage
	std::vector<Color> loot; //!< contained block colors in order

	/**
	 * Since @c SpawnGarbageInput frequently need to be sent over the network or stored
	 * in a replay file, they have a canonical string representation.
	 */
	std::string to_string() const;

	/**
	 * Return the @c SpawnGarbageInput from the string representation.
	 * @throw GameException if the string is not recognized.
	 */
	static SpawnGarbageInput from_string(std::string input_string);
};

/**
 * Inputs are all information that, given our implementation of game rules,
 * steers the game from the initial state to the final state.
 *
 * Inputs originate from the player (button presses) or from the Arbiter
 * (spawning blocks based on authoritative RNG rolls).
 * The three separate types of input are combined in class Input like
 * in a variant.
 *
 * They are recorded in the Journal, written to and read from replays and
 * sent over the network. Thus an Input has a string representation.
 *
 * For reconstrucing the course of a game in order, every input holds the
 * time value when it is to be applied.
 */
class Input
{

public:

	Input() = default;
	Input(const Input& ) = default;
	Input(Input&& ) = default;

	Input& operator=(const Input& ) = default;
	Input& operator=(Input&& ) = default;

	/**
	 * Construct a player-based input.
	 */
	explicit Input(PlayerInput input) noexcept;

	/**
	 * Construct an input for spawning blocks.
	 */
	explicit Input(SpawnBlockInput input) noexcept;

	/**
	 * Construct an input for spawning garbage.
	 */
	explicit Input(SpawnGarbageInput input) noexcept;

	/**
	 * Construct an input from its string representation.
	 */
	explicit Input(std::string source);

	bool operator==(const Input& rhs) const noexcept;

	/**
	 * Return the string representation of the Input.
	 */
	explicit operator std::string() const;

	/**
	 * Return the time value of the input.
	 */
	long game_time() const;

	/**
	 * Retrieve the contained input of the given type.
	 */
	template<typename SomeInput>
	const SomeInput& get() const
	{
		return std::get<SomeInput>(m_impl);
	}

	/**
	 * Invoke the callable visitor with the input of the contained type.
	 */
	template<typename Visitor>
	auto visit(Visitor&& vis) const
	{
		return std::visit(std::forward<Visitor>(vis), m_impl);
	}

private:

	using Impl = std::variant<PlayerInput, SpawnBlockInput, SpawnGarbageInput>;
	Impl m_impl;

};

/**
 * The InputDevices read inputs from the available peripherals: keyboard and joysticks.
 *
 * By default, this keyboard collects the following inputs:
 *  * \[RETURN]: reset key
 *  * \[ESC]: quit key
 *  * Player 1: arrow keys + [Z]
 *  * Player 2: numpad 8456 + [0]
 *  * [D], [H]: toggle debug view on pits
 *
 * The keys can currently not be remapped.
 * BUG: This implementation reads all SDL events, when it can only handle
 *      those which are inputs. Might need refactoring.
 */
class InputDevices
{

public:

	void set_player_number(std::optional<int> player_number) { m_player_number = player_number; }
	void set_joystick(JoystickPtr joystick) { m_joystick = std::move(joystick); }

	/**
	 * Read actions from the keyboard buffer and return them in order.
	 */
	std::vector<ControllerAction> poll();

private:

	std::optional<int> m_player_number;
	JoystickPtr m_joystick; //!< Optional SDL joystick object
	uint8_t m_joy_hat = 0; //!< last known joystick hat position

};

/**
 * Translate a controller input to a game input.
 * Not all controller inputs are game inputs.
 * If the controller input cannot be mapped, return an empty optional.
 */
std::optional<PlayerInput> controller_to_input(ControllerAction input) noexcept;
