#include "sdl_helper.hpp"
#include "error.hpp"
#include <cassert>
#include <cstring>

namespace
{

/**
 * Create an SDL surface with our preferred bit depth and color mask.
 */
SurfacePtr create_surface(int width, int height);

/**
 * Get the data of a single pixel from the surface in its own pixel format.
 */
Uint32 getpixel(SDL_Surface& surface, int x, int y) noexcept;

/**
 * Set the data of a single pixel on the surface in its own pixel format.
 */
void setpixel(SDL_Surface& surface, int x, int y, Uint32 data) noexcept;

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


SdlSoundPlayer::SdlSoundPlayer()
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

SdlSoundPlayer::~SdlSoundPlayer()
{
	SDL_CloseAudioDevice(devid);
}

void SdlSoundPlayer::play(const Sound& sound)
{
	pos = sound.buffer();
	remaining = sound.length();
}

void SdlSoundPlayer::callback(void* userdata, Uint8* stream, int length)
{
	SdlSoundPlayer* audio = reinterpret_cast<SdlSoundPlayer*>(userdata);
	int fill = (length > audio->remaining) ? audio->remaining : length;
	std::memcpy(stream, audio->pos, fill);
	// SDL_MixAudioFormat(stream, pos, AUDIO_S16, length, SDL_MIX_MAXVOLUME);

	std::memset(stream+fill, 0, length-fill); // pad with silence

	audio->pos += fill;
	audio->remaining -= fill;
}


Sdl::Sdl(Uint32 flags)
{
	assert(!SDL_WasInit(0));
	assert(!TTF_WasInit());

	// basic library setup
	sdlok(SDL_Init(flags));

	const int img_flags = IMG_INIT_PNG;
	const int img_result = IMG_Init(img_flags);
	if((img_result & img_flags) != img_flags) {
		SDL_Quit();
		throw SdlException(IMG_GetError());
	}

	const int ttf_result = TTF_Init();
	if(0 != ttf_result) {
		IMG_Quit();
		SDL_Quit();
		throw SdlException(TTF_GetError());
	}

	// graphics: window & renderer
	if(flags & SDL_INIT_VIDEO) {
		m_window.reset(SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CANVAS_W, CANVAS_H, 0));
		sdlok(m_window.get());

		m_renderer.reset(SDL_CreateRenderer(m_window.get(), -1, SDL_RENDERER_TARGETTEXTURE));
		sdlok(m_renderer.get());

		// The renderer must declare the capabilities to render stuff offscreen onto target textures.
		SDL_RendererInfo info;
		sdlok(SDL_GetRendererInfo(m_renderer.get(), &info));
		if((info.flags & SDL_RENDERER_TARGETTEXTURE) != SDL_RENDERER_TARGETTEXTURE)
			throw SdlException("Render driver does not support target textures.");
	}

	// audio device
	if(flags & SDL_INIT_AUDIO) {
		m_audio.reset(new SdlSoundPlayer);
	}

	// Enable joysticks in event handling
	if(flags & SDL_INIT_JOYSTICK) {
		if(1 != SDL_JoystickEventState(SDL_ENABLE))
			throw SdlException();
	}
}

Sdl::~Sdl()
{
	// be careful to destroy SDL objects before library shutdown
	m_audio.reset();
	m_renderer.reset();
	m_window.reset();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

SDL_Window& Sdl::window() const
{
	assert(m_window);
	return *m_window;
}

SDL_Renderer& Sdl::renderer() const
{
	assert(m_renderer);
	return *m_renderer;
}

SdlSoundPlayer& Sdl::audio() const
{
	assert(m_audio);
	return *m_audio;
}

SurfacePtr Sdl::load_surface(const char* file, SDL_PixelFormatEnum format) const
{
	SurfacePtr surface(IMG_Load(file));
	imgok(surface.get());

	if(SDL_PIXELFORMAT_UNKNOWN != format) {
		Uint32 original_format = SDL_MasksToPixelFormatEnum(
			surface->format->BitsPerPixel,
			surface->format->Rmask,
			surface->format->Gmask,
			surface->format->Bmask,
			surface->format->Amask);

		if(original_format != format) {
			surface.reset(SDL_ConvertSurfaceFormat(surface.get(), format, 0));
			sdlok(surface.get());
		}
	}

	return surface;
}

TexturePtr Sdl::cutout_texture(SDL_Surface& source, SDL_Rect rect) const
{
	SurfacePtr surface = create_surface(rect.w, rect.h);
	SDL_Rect dstrect{ 0, 0, rect.w, rect.h };
	sdlok(SDL_BlitSurface(&source, &rect, surface.get(), &dstrect));

	TexturePtr texture(SDL_CreateTextureFromSurface(m_renderer.get(), surface.get()));
	sdlok(texture.get());
	return texture;
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
		frames[c] = cutout_texture(*sheet, srcrect);
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
			frames[c] = cutout_texture(*sheet, srcrect);
		}

		textures[r] = std::move(frames);
	}

	return textures;
}

void Sdl::recolor(SDL_Surface& surface, SDL_Color before, SDL_Color after) const
{
	Uint32 before_data = SDL_MapRGBA(surface.format, before.r, before.g, before.b, before.a);
	Uint32 after_data = SDL_MapRGBA(surface.format, after.r, after.g, after.b, after.a);

	for(int y = 0; y < surface.h; y++)
	for(int x = 0; x < surface.w; x++) {
		Uint32 data = getpixel(surface, x, y);
		if(data == before_data) {
			setpixel(surface, x, y, after_data);
		}
	}
}

FontPtr Sdl::open_font(const char* file, int ptsize) const
{
	FontPtr font(TTF_OpenFont(file, ptsize));
	ttfok(font.get());
	return font;
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

// https://stackoverflow.com/questions/53033971/how-to-get-the-color-of-a-specific-pixel-from-sdl-surface
Uint32 getpixel(SDL_Surface& surface, int x, int y) noexcept
{
	assert(x >= 0);
	assert(y >= 0);
	assert(x < surface.w);
	assert(y < surface.h);

	int bpp = surface.format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8* p = (Uint8*)surface.pixels + (ptrdiff_t)y * surface.pitch + (ptrdiff_t)x * bpp;

	switch(bpp)
	{

	case 1:
		return *p;
		break;

	case 2:
		return *(Uint16*)p;
		break;

	case 3:
		if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
			return p[0] << 16 | p[1] << 8 | p[2];
		else
			return p[0] | p[1] << 8 | p[2] << 16;
		break;

	case 4:
		return *(Uint32*)p;
		break;

	default:
		assert(false);
		return 0;       /* shouldn't happen, but avoids warnings */

	}
}

void setpixel(SDL_Surface& surface, int x, int y, Uint32 data) noexcept
{
	assert(x >= 0);
	assert(y >= 0);
	assert(x < surface.w);
	assert(y < surface.h);

	int bpp = surface.format->BytesPerPixel;
	Uint8* p = (Uint8*)surface.pixels + (ptrdiff_t)y * surface.pitch + (ptrdiff_t)x * bpp;

	switch(bpp)
	{

	case 1:
		*p = static_cast<Uint8>(data);
		break;

	case 2:
		*(Uint16*)p = static_cast<Uint16>(data);
		break;

	case 3:
		if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p[0] = static_cast<Uint8>((data & 0xff0000) >> 16);
			p[1] = static_cast<Uint8>((data & 0xff00) >> 8);
			p[2] = static_cast<Uint8>(data & 0xff);
		}
		else {
			p[0] = static_cast<Uint8>(data & 0xff);
			p[1] = static_cast<Uint8>((data & 0xff00) >> 8);
			p[2] = static_cast<Uint8>((data & 0xff0000) >> 16);
		}
		break;

	case 4:
		*(Uint32*)p = data;
		break;

	default:
		assert(false);

	}
}

}
