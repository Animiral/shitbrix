/**
 * sdl_helper.hpp
 * C++-friendly wrappers for SDL stuff
 */
#pragma once

#include "globals.hpp"
#include <memory>
#include <cstring>
#include <SDL2/SDL.h>
#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_audio.h>

/**
 * An SDL_Texture* with an additional SDL_Rect* specifying its width and height (x,y == 0,0)
 */
struct TextRect
{
	SDL_Texture* texture;
	const SDL_Rect* rect;
};

/**
 * Custom deleter for SDL objects
 */
class SdlDeleter
{
public:
	void operator()(SDL_Surface* p) const { SDL_FreeSurface(p); }
	void operator()(SDL_Texture* p) const { SDL_DestroyTexture(p); }
	void operator()(SDL_Window* p) const { SDL_DestroyWindow(p); }
	void operator()(SDL_Renderer* p) const { SDL_DestroyRenderer(p); }
};

struct SoundImpl;

using Sound = std::shared_ptr<SoundImpl>;

/**
 * RAII for SDL sounds
 */
struct SoundImpl
{
	SoundImpl(const char* file)
	{
		auto load_result = SDL_LoadWAV(file, &spec, &buffer, &length);
		game_assert(load_result, SDL_GetError());
	}
	SoundImpl(const SoundImpl& ) = delete;
	SoundImpl(SoundImpl&& ) = default;
	~SoundImpl()
	{
		SDL_FreeWAV(buffer);
	}

	SoundImpl& operator=(const SoundImpl& ) = delete;
	SoundImpl& operator=(SoundImpl&& ) = default;

	Uint32 length; // bytes in wav
	Uint8* buffer; // wav data
	SDL_AudioSpec spec; // metadata
};

/**
 * Basic SDL sound player
 */
class Audio
{

public:

	Audio()
	{
		SDL_AudioSpec want;
		want.freq = 48000;
		want.format = AUDIO_S16;
		want.channels = 1;
		want.samples = AUDIO_SAMPLES;
		want.callback = &callback;
		want.userdata = this;

		devid = SDL_OpenAudioDevice(nullptr, 0, &want, &spec, 0);
		// std::cerr << "Opened device " << devid << "\n";
		// std::cerr << "spec.freq=" << spec.freq << "\n";
		// std::cerr << "spec.format=" << spec.format << "\n";
		// std::cerr << "spec.channels=" << static_cast<int>(spec.channels) << "\n";
		// std::cerr << "spec.samples=" << spec.samples << "\n";
		// std::cerr << "spec.callback=" << spec.callback << "\n";
		game_assert(devid > 0, SDL_GetError());

		SDL_PauseAudioDevice(devid, 0);
	}

	~Audio()
	{
		SDL_CloseAudioDevice(devid);
	}

	Audio(const Audio& ) =delete;
	Audio(Audio&& ) =default;
	Audio& operator=(const Audio& ) =delete;
	constexpr Audio& operator=(Audio&& ) =default;

	void play(Sound sound)
	{
		pos = sound->buffer;
		remaining = static_cast<int>(sound->length);
	}

private:

	static void callback(void* userdata, Uint8* stream, int length)
	{
		Audio* audio = reinterpret_cast<Audio*>(userdata);
		int fill = (length > audio->remaining) ? audio->remaining : length;
		std::memcpy(stream, audio->pos, fill);
		// SDL_MixAudioFormat(stream, pos, AUDIO_S16, length, SDL_MIX_MAXVOLUME);

		std::memset(stream+fill, 0, length-fill); // pad with silence

		audio->pos += fill;
		audio->remaining -= fill;
	}

	SDL_AudioDeviceID devid;
	SDL_AudioSpec spec; // metadata of audio device

	Uint8* pos = nullptr;
	int remaining = 0;
};

using Surface = std::unique_ptr<SDL_Surface, SdlDeleter>;
using Texture = std::unique_ptr<SDL_Texture, SdlDeleter>;
using Window = std::unique_ptr<SDL_Window, SdlDeleter>;
using Renderer = std::unique_ptr<SDL_Renderer, SdlDeleter>;

