#include "audio.hpp"

Audio::Audio(SdlAudio& audio, const Assets& assets)
: m_audio(audio), m_assets(assets)
{
}

void Audio::play(Snd sound) const
{
	Sound sdl_sound = m_assets.sound(sound);
	m_audio.play(sdl_sound);
}
