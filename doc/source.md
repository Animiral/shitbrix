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
