#pragma once

#include "sdl_helper.hpp"

/**
 * Interface for playing sound effects.
 */
class Audio
{

public:

	virtual void play(Snd sound) const = 0;

};

/**
 * Swallows sound effects.
 */
class NoAudio : public Audio
{

public:

	virtual void play(Snd sound) const override {}

};

/**
 * Plays sound effects through the SDL audio device.
 */
class SdlAudio : public Audio
{

public:

	SdlAudio(SdlSoundPlayer& player);

	virtual void play(Snd sound) const override;

private:

	SdlSoundPlayer& m_player;

};
