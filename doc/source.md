# Introduction
This file documents the technical design of shitbrix. It explains the different modules, classes and interactions from different perspectives.
Documentation is to be added bit by bit when an area is worked on. It is still very much incomplete.

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

