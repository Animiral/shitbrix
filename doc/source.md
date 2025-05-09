# Introduction
This file documents the software engineering design of shitbrix. It explains the different modules, classes and interactions from different perspectives.
Documentation is to be added bit by bit when an area is worked on. It is still very much incomplete.

# Orientation
Shitbrix is distributed from its git repository. The *master* branch holds the most current version. Releases are marked using tags.

## Dependencies
The application uses the following libraries:

* Google Test for unit testing
* SDL and SDL_image for user interaction and presentation
* enet for network communication

All external libraries reside in subdirectories under the `ext/` path in the form of git submodules.
Shitbrix will only compile if all these submodules are checked out.

## Build System
The supported build systems are *CMake* and *MSBuild* through Visual Studio project files.

The CMake build files include `CMakeLists.txt` in the top directory and a few helper scripts in the `cmake` directory.

Visual Studio files for shitbrix itself and all its library dependencies reside in the `msvc` directoy and subdirectories.

## Source Code
Shitbrix compiles into two to four different binaries with sources in separate directories:

* In `src`: *Shitbrix*, the library which contains all functionality of the game
* In `src/main.cpp`: *ShitbrixMain*, the platform-specific executable which unifies invocation and argument parsing
* In `test`: *ShitbrixTest*, the executable based on Google Test which runs unit tests over the main library
* In `visualdemo`: *VisualDemo*, a separate executable used for development experimentation and debugging

## Documentation
Program documentation resides in the `doc` directory.

It presently includes:

* This design document
* JavaDoc-based source code documentation (optional, can be generated separately)
* `README.md` in the top directory

In the future, it might include a user manual, a more extensive README and perhaps other pieces like HACKING.

## Assets
Game assets are separated into directories according to their type.

* In `gfx`: image files
* In `snd`: sound files

In the future, more categories of assets might include *music*, *character definitions*, *scripts*, *content packs*, *shaders* and more.

# Principles
The design goal is to (hopefully) create an application using exemplary modern C++, object-oriented principles and good practices.

If some aspect violates this ambition, it should happen for a good reason. These can include lack of knowledge and case-by-case comprehensibility.

# Architecture
The program is structured in roughly three layers:

1. Presentation: coordinates the application at a high level
2. Logic: core functionality
3. Utilities: basic definitions, helpers and abstractions

Each layer includes modules and calls functions only from itself or deeper layers. Upward communication is implemented with Inversion of Control patterns.

## Presentation Modules

* **main.cpp**: Load configuration, setup.
* **game_loop.cpp**: Poll inputs, maintain current screen.
* **screen.(cpp|hpp)**: State machine for screens and game phases.
* **stage.(cpp|hpp)**: Ingame on-screen components such as banners and game containers.
* **draw.(cpp|hpp)**: Draw objects on the screen according to their state.

## Logic Modules

* Gameplay
    * **game.(cpp|hpp)**: Ownership of game-related objects.
    * **state.(cpp|hpp)**: Gameplay object definitions.
    * **event.(cpp|hpp)**: Gameplay event definitions.
    * **logic.(cpp|hpp)**: Recognize abstract conditions like "blocks landing" from game state.
    * **director.(cpp|hpp)**: High-level operations on game state, like "spawn garbage".
    * **agent.(cpp|hpp)**: AI player agent.
* Recording
    * **replay.(cpp|hpp)**: Manipulate the game record as a series of events and write them to a `Journal`.
    * **input.(cpp|hpp)**: Convert inputs from controller, keyboard and replay into game actions.
    * **arbiter.(cpp|hpp)**: The Arbiter decides game outcomes that depend on random numbers, such as spawning block colors.

## Utility Modules

* Global Definitions
    * **globals.(cpp|hpp)**: Define constants and the base `class GameException`. This module has no dependencies and is included almost everywhere.
    * **error.(cpp|hpp)**: Error handling and logging facilities.
* Library Abstraction
    * **asset.(cpp|hpp)**: Load, destroy and map identifiers to game assets.
    * **(sdl|enet)_helper.(cpp|hpp)**: Convenience helpers for SDL and enet objects to add RAII features.
    * **sdl_context.hpp**: Facade for the SDL library.
    * **text.(cpp|hpp)**: Turn `TTF_Font`s or `SDL_Surface` bitmap charsets into colored `SDL_Texture`s for drawing text.
    * **audio.(cpp|hpp)**: Decorator for playing audio assets.
    * **network.(cpp|hpp)**: Messaging over the network.
* Object Management
    * **configuration.(cpp|hpp)**: Load and manage the settings that govern application behavior.
    * **context.(cpp|hpp)**: Ownership of global objects constructed according to configuration.

# Context
Shitbrix uses a global service locator called *context*, which is defined globally in `context.hpp`:

```
extern GlobalContext the_context;
```

This context serves as a registry for shared objects and application-wide implementations for things like logging, configuration, and system libraries. These objects are initialized at startup based on the configuration and command-line parameters and remain valid for the duration of the program.

Many classes use dependency injection via the constructor. The contents of the context are globally available in such cases to be used to fill these dependency paramters. The context itself holds ownership of the resources it keeps. 

In testing, swapping out the implementations of context objects allows us to easily set up a different environment.

# Input
There is a hierarchy of inputs.

```
+---------------+    +----------------+    +--------------+
| General Input | -> | Controller Key | -> | Player Input |
+---------------+    +----------------+    +--------------+
```

General inputs (managed by SDL) can be any key. They are either processed directly in the case of non-configurable application-level functions (i.e. ESC=quit) or fed through the controller key mapper.

Controller keys are a small set of inputs that every Screen accepts. Their function is context-specific. For example, in the menu screen, “down” means “select previous option”, while in the game screen it maps to a “player input”.

Player inputs are actions that influence the replay of a game round, for example “move cursor down”.

# Replays
A replay is an initial state plus a sequence of inputs from all players in the game that completely describes the history of one round.
Input events propagate according to the following diagram:

```
+-------------+    +-------------+    +---------+    +-------------+
| ReplayEvent | -> | PlayerInput | -> | Journal | -> | Replay File |
+-------------+    +-------------+    +---------+    +-------------+
                          |
                          v
                      +--------+
                      | Cursor |
                      +--------+
```

The replay file is split into lines, which are separated into tokens by whitespace.
Every line represents one *event*, of which there are:

 * set <name> <value> - Specifies a value for some variable with the given name.
 * start - Signifies the start of the round.
 * input <player> <key> - Produces a button/key action from the specified player.
 * end - Signifies the end of the round.

Events are preceded on every line by the timestamp (in game ticks) of the event.

Example:
```
0 set rng_seed 4711
0 set winner 1
0 start
3 input 0 left
5 input 1 up
8 input 0 raise
10 input 0 left
10 input 1 swap
20 end
```

Correct playback is guaranteed only if the timestamps are ordered ascending, and until the “end” event occurs.
Invalid names for events, set variables and keys, out-of-range player numbers and seed values cause that line to be ignored.
Only the debug build will assert for correct event lines.
If an event line is malformed, the game aborts playback of the replay immediately.

This replay format and behavior was chosen to be easy to parse and easy to extend later.

# Game Logic
The logic of the game is implemented in the *director.cpp* module.
The main class is `BlockDirector`, which receives one `update()` per tick, like the game state itself.

The game state consists of the Pit objects and their contents.
The basic game objects can handle some of their state machinery by themselves. Yet, to manage positioning, state transitions, falling and landing, spawns, matching and dissolving, the Director operates on the game state.

The components of one logic tick are:

## Spawn previews
New blocks in *preview* state appear at the bottom of the pit as it scrolls.
At the same time, the previous previews become normal blocks at rest. In this instant, they are added to the *hots* set. The hots are a set of blocks which may become part of a match in this update.

## Examine state-finish of physicals
Physical objects (blocks and garbage) execute their state until some condition occurs. For example, their internal timer runs out while they are falling, indicating that they have reached their target location.
Physicals whose states are “running out” transition to a successor state.

* If a block finishes breaking, or fake-swapping, it becomes dead.
* If a garbage finishes breaking, it is added to the *dissolvers* set. The dissolvers are a set of garbage objects which are going to shrink or disappear in this update.
* If a block arrived from swapping or falling or it rests above blocks which did, it is added to the *fallers* set. The fallers are a set of physical objects which may fall down if there is nothing in the way.
* If a block arrived from swapping or falling, it is added to the *hots* set.
* If any objects became dead, the `dead_physical` flag is set. Later, the pit might have to resume scrolling.
* If any blocks became dead, the `dead_block` flag is set. Later, the pit must clean up the remains.
* If any non-fake blocks became dead, the `dead_sound` flag is set. Later, we must emit an event for the front-end to play a block-breaking sound effect.
* If any objects go above the top row of the pit, the `panic` flag is set. This can lead to game over.

## Check for game over
When the game is over, the `over` flag is set. The game screen can later query this with `BlockDirector::over()`.

## Convert garbage
Shrink or remove expired garbage blocks from the *dissolvers* set.
All new blocks that emerge join the set of *hots*. The new blocks as well as the remaining garbage join the set of *fallers*.

## Remove dead
The pit erases all block objects which are in the *dead* state.
If the `dead_sound` flag is set, emit an event to play the break sound.

## Examine fallers
All physicals in the *fallers* set now actually enter the *fall* state if possible. Successful fallers can not match and are therefore removed from the *hots* set. Those fallers which can not fall after all transition to the *land* or *rest* states.
A block which enters the rest state resets its internal chain counter to 0.

## Examine hots
All blocks which are hot can be part of a match if they form a three-or-more same-colored row. All matching blocks and all adjacent garbage bricks enter the *break* state. All matching blocks’ chain counters are incremented.
If there is at least one match, the `have_match` flag is set. Later, we must emit an event for the front-end to play a block-match sound effect.
The combo counter for the match is the total number of blocks involved in a match in this update.
The chain counter of the match is the maximum of all the involved blocks’ internal chain counters.
These counters are emitted in a match event.

# Combos and Chains
Combos and chains are block match configurations which give extra points and benefits to the player.

They are mostly detected and handled during match-generation, implemented in `class MatchBuilder` (director.hpp).

The combo is simply the number of blocks in a single match. Everything above the minimum 3 yields a bonus.

Chains aggregate over a series of different match events. Every block that falls down as a result of a completed match or dissolved garbage is a *chaining block*. While a chaining block falls down, the player can exchange its chaining property with another block by swapping them mid-fall (“skill chains”).

Every match which involves a chaining block increases the *chain counter* of the player. When the last chaining block comes to rest, the chain is finished and yields a bonus. The magnitude of the bonus corresponds to the value of the chain counter at the time of completion of the chain. The chain counter resets to 0 afterwards.

The combos and chains detected by the MatchBuilder are exposed by its user, the BlockDirector (see chapter on Game Logic), in *game events*. A game event is a function call sent to the director’s registered game event handler. The director sends game events when a match happens and when a chain finishes, among other circumstances.

While there is at least one chaining block in the pit, the pit’s scrolling is disabled. Chaining can stave off the player’s losing the game.

# Networking

The Network module is layered in 3 abstractions:

## enet implementation
The low-level network communication is implemented using the enet library.

## Channels and Messages
The `Channel` allows a network-connected peer to send `Message` data structures across the network to one or more remote peers, as well as receive a buffered collection of messages upon request via `Channel::poll()`. `Channel` is abstracted in the interface `class IChannel`.

A `Message` contains a small header with common data and a variable-length payload.
When sending a message, the recipients are determined by the underlying implementation from the `Message` header fields.

`ServerChannel` is an implementation that allows many clients to connect and broadcasts messages to all of them. `ClientChannel` is an implementation that connects to one server and sends messages to it.

## Protocols
The `Protocol` classes provide an interface for sending specific messages to the remote(s). They translate C++ function calls to and from low-level `Message` representation and send/receive the messages over a `Channel`.

The interface `class IServerProtocol` implements messages from the server, while the interface `class IClientProtocol` implements messages from the client.

On the server side, the `ServerSendProtocol` uses a `ServerChannel` to build `Message`s and send them. The server uses its own client protocol recipient implementation, derived from `IClientProtocol`, to receive client messages by invoking `server_send_protocol.poll(recipient)`. The client side sends and receives messages analogously.

## Network Testing
In tests, the `ServerChannel` and `ClientChannel` can be replaced by a `TestServerChannel` and `TestClientChannel`, which pass messages in memory to the other channel in the local program.
