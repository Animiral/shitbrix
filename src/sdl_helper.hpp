/**
 * sdl_helper.hpp
 * C++-friendly wrappers for SDL stuff
 */
#pragma once

#include "globals.hpp"
#include <memory>
#include <vector>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_audio.h>

class SdlRaiiImpl;
struct TextureImpl;
struct SoundImpl;

using SdlRaii = std::shared_ptr<SdlRaiiImpl>;
using Texture = std::shared_ptr<TextureImpl>;
using Sound = std::shared_ptr<SoundImpl>;

/**
 * Wrapper class for safe initialization and shutdown of the SDL library.
 * SDL-dependent classes should keep a shared_ptr to a central instance of SdlRaiiImpl
 * for their own life time to guarantee the safety of SDL function calls.
 * The SdlRaii type is a shortcut for that purpose.
 */
class SdlRaiiImpl
{
public:
	SdlRaiiImpl();

	SdlRaiiImpl(const SdlRaiiImpl& ) =delete;
	SdlRaiiImpl(SdlRaiiImpl&& ) =default;
	SdlRaiiImpl& operator=(const SdlRaiiImpl& ) =delete;
	SdlRaiiImpl& operator=(SdlRaiiImpl&& ) =delete;

	~SdlRaiiImpl();
};

/**
 * Custom deleter for SDL objects, to be used with unique_ptrs
 * when manual RAII wrappers are not needed.
 */
class SdlDeleter
{
public:
	void operator()(SDL_Surface* p) const { SDL_FreeSurface(p); }
	void operator()(SDL_Texture* p) const { SDL_DestroyTexture(p); }
	void operator()(SDL_Window* p) const { SDL_DestroyWindow(p); }
	void operator()(SDL_Renderer* p) const { SDL_DestroyRenderer(p); }
};

/**
 * RAII for an SDL texture asset.
 * Many of our textures (sprites, background graphics, banners...) are cut out
 * of sheets with a collection of different same-sized graphics. This texture
 * wrapper helps with such cut-outs.
 * The SdlFactory and Asset classes help with the clumsy constructor.
 */
struct TextureImpl
{
	TextureImpl(SdlRaii sdl, SDL_Renderer* renderer, SDL_Surface* sheet, int width, int height, int row, int column);

	TextureImpl(const TextureImpl& ) = delete;
	TextureImpl(TextureImpl&& ) = default;
	TextureImpl& operator=(const TextureImpl& ) = delete;
	TextureImpl& operator=(TextureImpl&& ) = default;

	SdlRaii sdl;
	std::unique_ptr<SDL_Texture, SdlDeleter> tex;
	int width;
	int height;
};

/**
 * RAII for an SDL sound asset.
 * Even though this wrapper can load any sound supported by SDL_LoadWAV(),
 * the current basic (non-mixing) implementation of Audio expects:
 *  - frequency: 48000Hz
 *  - format: signed 16-bit little-endian
 *  - channels: 1
 */
struct SoundImpl
{
	SoundImpl(SdlRaii sdl, const char* file);
	~SoundImpl();

	SoundImpl(const SoundImpl& ) = delete;
	SoundImpl(SoundImpl&& ) = default;
	SoundImpl& operator=(const SoundImpl& ) = delete;
	SoundImpl& operator=(SoundImpl&& ) = default;

	SdlRaii sdl;
	Uint32 length; // bytes in wav
	Uint8* buffer; // wav data
	SDL_AudioSpec spec; // metadata
};

/**
 * Basic SDL sound player.
 * Every instance opens an audio device upon instantiation.
 * The SDL audio subsystem must be initialized to successfully instantiate Audio.
 */
class SdlAudio
{

public:

	SdlAudio(SdlRaii sdl);
	~SdlAudio();

	SdlAudio(const SdlAudio& ) =delete;
	SdlAudio(SdlAudio&& ) =default;
	SdlAudio& operator=(const SdlAudio& ) =delete;
	SdlAudio& operator=(SdlAudio&& ) =default;

	void play(Sound sound);

private:

	static void callback(void* userdata, Uint8* stream, int length);

	SdlRaii sdl;
	SDL_AudioDeviceID devid;
	SDL_AudioSpec spec; // metadata of audio device
	Uint8* pos = nullptr;
	int remaining = 0;
};

/**
 * Factory for SDL asset objects and wrappers.
 * Methods get_* return a lazy-initialized object of which there is only one.
 * Methods create_* return a new object, often wrapped in a container.
 * The factory passes its own SdlRaii to all created objects. This guarantees
 * that all created objects use the same SDL “session”, hiding this
 * concern from clients of SdlFactory.
 */
class SdlFactory
{

public:

	SdlFactory() =default;
	SdlFactory(SdlRaii sdl);

	SdlFactory(const SdlFactory& ) =default;
	SdlFactory(SdlFactory&& ) =default;
	SdlFactory& operator=(const SdlFactory& ) =default;
	SdlFactory& operator=(SdlFactory&& ) =default;

	// single-instance getters and setters
	SdlRaii get_sdl() const;
	std::shared_ptr<SDL_Window> get_window() const;
	SDL_Renderer& get_renderer() const;
	std::shared_ptr<SdlAudio> get_audio() const;

	// creation methods

	/**
	 * Create an image texture from an image file.
	 */
	Texture create_texture(const char* file) const;

	/**
	 * Create a screen-sized texture for offscreen drawing.
	 */
	std::unique_ptr<SDL_Texture, SdlDeleter> create_target_texture() const;

	std::vector<Texture> create_texture_row(const char* file, int width) const;
	std::vector< std::vector<Texture> > create_texture_sheet(const char* file, int height, int width) const;
	Sound create_sound(const char* file) const;

private:

	// all mutable fields use lazy initialization
	mutable SdlRaii sdl;
	mutable std::shared_ptr<SDL_Window> window;
	mutable std::shared_ptr<SdlAudio> audio;
	mutable std::unique_ptr<SDL_Renderer, SdlDeleter> m_renderer;
	mutable Uint32 m_pxformat; //!< preferred texture pixel format

};
