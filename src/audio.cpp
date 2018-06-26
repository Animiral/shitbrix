#include "audio.hpp"
#include "context.hpp"
#include "asset.hpp"

SdlAudio::SdlAudio(SdlSoundPlayer& player)
: m_player(player)
{
}

void SdlAudio::play(Snd sound) const
{
	const Sound& sdl_sound = the_context.assets->sound(sound);
	m_player.play(sdl_sound);
}
