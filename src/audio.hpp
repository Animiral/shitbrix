#pragma once

#include "sdl_helper.hpp"
#include "asset.hpp"

/**
 * Plays sound effects.
 */
class Audio
{

public:

	Audio(SdlAudio& audio, const Assets& assets);

	void play(Snd sound) const;

private:

	SdlAudio& m_audio;
	const Assets& m_assets;

};
