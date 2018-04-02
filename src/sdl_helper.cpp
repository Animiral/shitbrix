#include "sdl_helper.hpp"
#include "error.hpp"
#include <cstring>
#include <SDL2/SDL.h>

namespace
{

/**
 * Create an SDL surface with our preferred bit depth and color mask.
 */
SurfacePtr create_surface(int width, int height);

/**
 * Create a texture from the specified region of the source surface.
 */
TexturePtr cutout_texture(SDL_Renderer& renderer, SDL_Surface& source, SDL_Rect& srcrect);

}


Sound::Sound(const char* file)
{
	sdlok(SDL_LoadWAV(file, &m_spec, &m_buffer, &m_length));
}

Sound::~Sound() noexcept
{
	SDL_FreeWAV(m_buffer);
}

Sound::Sound(Sound&& rhs) noexcept
: m_length(rhs.m_length), m_buffer(rhs.m_buffer), m_spec(rhs.m_spec)
{
	rhs.m_buffer = nullptr;
}

Sound& Sound::operator=(Sound&& rhs) noexcept
{
	m_length = rhs.m_length;
	m_buffer = rhs.m_buffer;
	m_spec = rhs.m_spec;
	rhs.m_buffer = nullptr;

	return *this;
}


SdlAudio::SdlAudio()
{
	SDL_AudioSpec want;
	want.freq = 48000;
	want.format = AUDIO_S16;
	want.channels = 1;
	want.samples = AUDIO_SAMPLES;
	want.callback = &callback;
	want.userdata = this;

	devid = SDL_OpenAudioDevice(nullptr, 0, &want, &spec, 0);
	if(devid <= 0)
		throw SdlException();

	SDL_PauseAudioDevice(devid, 0);
}

SdlAudio::~SdlAudio()
{
	SDL_CloseAudioDevice(devid);
}

void SdlAudio::play(const Sound& sound)
{
	pos = sound.buffer();
	remaining = sound.length();
}

void SdlAudio::callback(void* userdata, Uint8* stream, int length)
{
	SdlAudio* audio = reinterpret_cast<SdlAudio*>(userdata);
	int fill = (length > audio->remaining) ? audio->remaining : length;
	std::memcpy(stream, audio->pos, fill);
	// SDL_MixAudioFormat(stream, pos, AUDIO_S16, length, SDL_MIX_MAXVOLUME);

	std::memset(stream+fill, 0, length-fill); // pad with silence

	audio->pos += fill;
	audio->remaining -= fill;
}


Sdl& Sdl::instance()
{
	static Sdl sdl;
	return sdl;
}

TexturePtr Sdl::create_texture(const char* file) const
{
	TexturePtr texture(IMG_LoadTexture(m_renderer.get(), file));
	imgok(texture.get());
	return texture;
}

TexturePtr Sdl::create_target_texture() const
{
	TexturePtr texture(SDL_CreateTexture(m_renderer.get(), 0, SDL_TEXTUREACCESS_TARGET,
	                                     CANVAS_W, CANVAS_H));
	sdlok(texture.get());
	return texture;
}

std::vector<TexturePtr> Sdl::create_texture_row(const char* file, int width) const
{
	SurfacePtr sheet(IMG_Load(file));
	imgok(sheet.get());

	int columns = sheet->w / width;
	std::vector<TexturePtr> frames(columns);

	for(int c = 0; c < columns; c++) {
		SDL_Rect srcrect{ c*width, 0, width, sheet->h };
		frames[c] = cutout_texture(*m_renderer, *sheet, srcrect);
	}

	return frames;
}

std::vector< std::vector<TexturePtr> > Sdl::create_texture_sheet(const char* file, int height, int width) const
{
	SurfacePtr sheet(IMG_Load(file));
	imgok(sheet.get());

	int rows = sheet->h / height;
	int columns = sheet->w / width;
	std::vector< std::vector<TexturePtr> > textures(rows);

	for(int r = 0; r < rows; r++) {
		std::vector<TexturePtr> frames(columns);

		for(int c = 0; c < columns; c++) {
			SDL_Rect srcrect{ c*width, r*height, width, height };
			frames[c] = cutout_texture(*m_renderer, *sheet, srcrect);
		}

		textures[r] = std::move(frames);
	}

	return textures;
}

Sdl::Sdl()
{
	// basic library setup
	sdlok(SDL_Init(SDL_INIT_EVERYTHING));

	int flags = IMG_INIT_PNG;
	int img_result = IMG_Init(flags);
	if((img_result & flags) != flags) {
		SDL_Quit();
		throw SdlException(IMG_GetError());
	}

	// graphics: window & renderer
	m_window.reset(SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CANVAS_W, CANVAS_H, 0));
	sdlok(m_window.get());

	m_renderer.reset(SDL_CreateRenderer(m_window.get(), -1, SDL_RENDERER_TARGETTEXTURE));
	sdlok(m_renderer.get());

	// The renderer must declare the capabilities to render stuff offscreen onto target textures.
	SDL_RendererInfo info;
	sdlok(SDL_GetRendererInfo(m_renderer.get(), &info));
	if((info.flags & SDL_RENDERER_TARGETTEXTURE) != SDL_RENDERER_TARGETTEXTURE)
		throw SdlException("Render driver does not support target textures.");

	// audio device
	m_audio.reset(new SdlAudio);
}

Sdl::~Sdl()
{
	// be careful to destroy SDL objects before library shutdown
	m_audio.reset();
	m_renderer.reset();
	m_window.reset();
	IMG_Quit();
	SDL_Quit();
}

namespace
{

SurfacePtr create_surface(int width, int height)
{
	// be careful to preserve alpha channel (SDL defaults to amask=0)
	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif

	SurfacePtr surface(SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask));
	sdlok(surface.get());
	return surface;
}

TexturePtr cutout_texture(SDL_Renderer& renderer, SDL_Surface& source, SDL_Rect& srcrect)
{
	SurfacePtr surface = create_surface(srcrect.w, srcrect.h);
	SDL_Rect dstrect{ 0, 0, srcrect.w, srcrect.h};
	SDL_BlitSurface(&source, &srcrect, surface.get(), &dstrect);

	TexturePtr texture(SDL_CreateTextureFromSurface(&renderer, surface.get()));
	sdlok(texture.get());
	return texture;
}

}
