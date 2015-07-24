// http://paulbourke.net/dataformats/tga/

#include "Nooskewl_Engine/engine.h"
#include "Nooskewl_Engine/error.h"
#include "Nooskewl_Engine/image.h"
#include "Nooskewl_Engine/internal.h"
#include "Nooskewl_Engine/vertex_cache.h"

using namespace Nooskewl_Engine;

std::vector<Image::Internal *> Image::loaded_images;

struct TGA_Header {
	char idlength;
	char colourmaptype;
	char datatypecode;
	short int colourmaporigin;
	short int colourmaplength;
	char colourmapdepth;
	short int x_origin;
	short int y_origin;
	short width;
	short height;
	char bitsperpixel;
	char imagedescriptor;
	SDL_Colour palette[256];
};

static void merge_bytes(unsigned char *pixel, unsigned char *p, int bytes, TGA_Header *header)
{
	if (header->colourmaptype == 1) {
		SDL_Colour *colour = &header->palette[*p];
		// Magic pink
		// Paletted
		if (colour->r == 255 && colour->g == 0 && colour->b == 255) {
			// transparent
			*pixel++ = 0;
			*pixel++ = 0;
			*pixel++ = 0;
			*pixel++ = 0;
		}
		else {
			*pixel++ = colour->r;
			*pixel++ = colour->g;
			*pixel++ = colour->b;
			*pixel++ = 255;
		}
	}
	else {
		if (bytes == 4) {
			*pixel++ = p[2];
			*pixel++ = p[1];
			*pixel++ = p[0];
			*pixel++ = p[3];
		}
		else if (bytes == 3) {
			*pixel++ = p[2];
			*pixel++ = p[1];
			*pixel++ = p[0];
			*pixel++ = 255;
		}
		else if (bytes == 2) {
			*pixel++ = (p[1] & 0x7c) << 1;
			*pixel++ = ((p[1] & 0x03) << 6) | ((p[0] & 0xe0) >> 2);
			*pixel++ = (p[0] & 0x1f) << 3;
			*pixel++ = (p[1] & 0x80);
		}
	}
}

static inline unsigned char *pixel_ptr(unsigned char *p, int n, TGA_Header *h)
{
	/* OpenGL expects upside down, so that's what we provide */
	int flip = (h->imagedescriptor & 0x20) != 0;
	if (flip) {
		int x = n % h->width;
		int y = n / h->width;
		return p + (h->width * 4) * (h->height-1) - (y * h->width * 4) +  x * 4;
	}
	else
		return p + n * 4;
}

Image::Image(std::string filename, bool is_absolute_path)
{
	if (is_absolute_path == false) {
		filename = "images/" + filename;
	}

	this->filename = filename;

	reload();
}

Image::Image(SDL_Surface *surface) :
	filename("--FROM SURFACE--")
{
	unsigned char *pixels;
	SDL_Surface *tmp = 0;

	if (surface->format->format == SDL_PIXELFORMAT_RGBA8888)
		pixels = (unsigned char *)surface->pixels;
	else {
		SDL_PixelFormat format;
		format.format = SDL_PIXELFORMAT_RGBA8888;
		format.palette = 0;
		format.BitsPerPixel = 32;
		format.BytesPerPixel = 4;
		format.Rmask = 0xff;
		format.Gmask = 0xff00;
		format.Bmask = 0xff0000;
		format.Amask = 0xff000000;
		tmp = SDL_ConvertSurface(surface, &format, 0);
		if (tmp == 0) {
			throw Error("SDL_ConvertSurface returned 0");
		}
		pixels = (unsigned char *)tmp->pixels;
	}

	size = Size<int>(surface->w, surface->h);

	try {
		internal = new Internal(pixels, size);
	}
	catch (Error e) {
		if (tmp) SDL_FreeSurface(tmp);
		throw e;
	}

	if (tmp) {
		SDL_FreeSurface(tmp);
	}
}

Image::~Image()
{
	release();
}

void Image::release()
{
	if (filename == "--FROM SURFACE--") {
		delete internal;
		return;
	}

	for (size_t i = 0; i < loaded_images.size(); i++) {
		Internal *ii = loaded_images[i];
		if (ii->filename == filename) {
			ii->refcount--;
			if (ii->refcount == 0) {
				delete ii;
				loaded_images.erase(loaded_images.begin()+i);
				return;
			}
		}
	}
}

void Image::reload()
{
	if (filename == "--FROM SURFACE--") {
		return;
	}

	for (size_t i = 0; i < loaded_images.size(); i++) {
		Internal *ii = loaded_images[i];
		if (ii->filename == filename) {
			ii->refcount++;
			internal = ii;
			size = internal->size;
			return;
		}
	}

	internal = new Internal(filename);
	size = internal->size;
	loaded_images.push_back(internal);
}

void Image::start(bool repeat)
{
	m.vertex_cache->start(this, repeat);
}

void Image::end()
{
	m.vertex_cache->end();
}

void Image::stretch_region_tinted_repeat(SDL_Colour tint, Point<float> source_position, Size<int> source_size, Point<float> dest_position, Size<int> dest_size, int flags)
{
	SDL_Colour colours[4];
	colours[0] = colours[1] = colours[2] = colours[3] = tint;

	int wt = dest_size.w / source_size.w;
	if (dest_size.w % source_size.w != 0) {
		wt++;
	}
	int ht = dest_size.h / source_size.h;
	if (dest_size.h % source_size.h != 0) {
		ht++;
	}

	int drawn_h = 0;
	for (int y = 0; y < ht; y++) {
		int drawn_w = 0;
		int h = source_size.h;
		if (dest_size.h - drawn_h < h) {
			h = dest_size.h- drawn_h;
		}
		for (int x = 0; x < wt; x++) {
			int w = source_size.w;
			if (dest_size.w - drawn_w < w) {
				w = dest_size.w - drawn_w;
			}
			Size<int> sz(w, h);
			m.vertex_cache->cache(colours, source_position, sz, Point<float>(dest_position.x + x * source_size.w, dest_position.y + y * source_size.h), sz, flags);
			drawn_w += w;
		}
		drawn_h += h;
	}
}

void Image::stretch_region_tinted(SDL_Colour tint, Point<float> source_position, Size<int> source_size, Point<float> dest_position, Size<int> dest_size, int flags)
{
	SDL_Colour colours[4];
	colours[0] = colours[1] = colours[2] = colours[3] = tint;
	m.vertex_cache->cache(colours, source_position, source_size, dest_position, dest_size, flags);
}

void Image::stretch_region(Point<float> source_position, Size<int> source_size, Point<float> dest_position, Size<int> dest_size, int flags)
{
	m.vertex_cache->cache(noo.four_whites, source_position, source_size, dest_position, dest_size, flags);
}

void Image::draw_region_tinted(SDL_Colour tint, Point<float> source_position, Size<int> source_size, Point<float> dest_position, int flags)
{
	SDL_Colour colours[4];
	colours[0] = colours[1] = colours[2] = colours[3] = tint;
	m.vertex_cache->cache(colours, source_position, source_size, dest_position, source_size, flags);
}

void Image::draw_region_z(Point<float> source_position, Size<int> source_size, Point<float> dest_position, float z, int flags)
{
	m.vertex_cache->cache_z(noo.four_whites, source_position, source_size, dest_position, z, source_size, flags);
}

void Image::draw_region(Point<float> source_position, Size<int> source_size, Point<float> dest_position, int flags)
{
	draw_region_z(source_position, source_size, dest_position, 0.0f, flags);
}

void Image::draw_z(Point<float> dest_position, float z, int flags)
{
	draw_region_z(Point<float>(0, 0), size, dest_position, z, flags);
}

void Image::draw_tinted(SDL_Colour tint, Point<float> dest_position, int flags)
{
	draw_region_tinted(tint, Point<float>(0, 0), size, dest_position, flags);
}

void Image::draw(Point<float> dest_position, int flags)
{
	draw_z(dest_position, 0.0f, flags);
}

void Image::stretch_region_tinted_repeat_single(SDL_Colour tint, Point<float> source_position, Size<int> source_size, Point<float> dest_position, Size<int> dest_size, int flags)
{
	start();
	stretch_region_tinted_repeat(tint, source_position, source_size, dest_position, dest_size, flags);
	end();
}

void Image::stretch_region_single(Point<float> source_position, Size<int> source_size, Point<float> dest_position, Size<int> dest_size, int flags)
{
	start();
	stretch_region(source_position, source_size, dest_position, dest_size, flags);
	end();
}

void Image::stretch_region_tinted_single(SDL_Colour tint, Point<float> source_position, Size<int> source_size, Point<float> dest_position, Size<int> dest_size, int flags)
{
	start();
	stretch_region_tinted(tint, source_position, source_size, dest_position, dest_size, flags);
	end();
}

void Image::draw_region_tinted_single(SDL_Colour tint, Point<float> source_position, Size<int> source_size, Point<float> dest_position, int flags)
{
	start();
	draw_region_tinted(tint, source_position, source_size, dest_position, flags);
	end();
}

void Image::draw_region_z_single(Point<float> source_position, Size<int> source_size, Point<float> dest_position, float z, int flags)
{
	start();
	draw_region_z(source_position, source_size, dest_position, z, flags);
	end();
}

void Image::draw_region_single(Point<float> source_position, Size<int> source_size, Point<float> dest_position, int flags)
{
	start();
	draw_region(source_position, source_size, dest_position, flags);
	end();
}

void Image::draw_z_single(Point<float> dest_position, float z, int flags)
{
	start();
	draw_z(dest_position, z, flags);
	end();
}

void Image::draw_tinted_single(SDL_Colour tint, Point<float> dest_position, int flags)
{
	start();
	draw_tinted(tint, dest_position, flags);
	end();
}

void Image::draw_single(Point<float> dest_position, int flags)
{
	draw_z_single(dest_position, 0.0f, flags);
}

void Image::release_all()
{
	for (size_t i = 0; i < loaded_images.size(); i++) {
		loaded_images[i]->release();
	}
}

void Image::reload_all()
{
	for (size_t i = 0; i < loaded_images.size(); i++) {
		loaded_images[i]->reload();
	}
}

int Image::get_unfreed_count()
{
	for (size_t i = 0; i < loaded_images.size(); i++) {
		infomsg("Unfreed: %s\n", loaded_images[i]->filename);
	}
	return loaded_images.size();
}

unsigned char *Image::read_tga(std::string filename, Size<int> &out_size)
{
	SDL_RWops *file = open_file(filename);

	int n = 0, i, j;
	int bytes2read, skipover = 0;
	unsigned char p[5];
	TGA_Header header;
	unsigned char *pixels;

	/* Display the header fields */
	header.idlength = SDL_fgetc(file);
	header.colourmaptype = SDL_fgetc(file);
	header.datatypecode = SDL_fgetc(file);
	header.colourmaporigin = SDL_ReadLE16(file);
	header.colourmaplength = SDL_ReadLE16(file);
	header.colourmapdepth = SDL_fgetc(file);
	header.x_origin = SDL_ReadLE16(file);
	header.y_origin = SDL_ReadLE16(file);
	header.width = SDL_ReadLE16(file);
	header.height = SDL_ReadLE16(file);
	header.bitsperpixel = SDL_fgetc(file);
	header.imagedescriptor = SDL_fgetc(file);

	int w, h;
	out_size.w = w = header.width;
	out_size.h = h = header.height;

	/* Allocate space for the image */
	if ((pixels = new unsigned char[header.width*header.height*4]) == 0) {
		SDL_RWclose(file);
		throw MemoryError("malloc of image failed");
	}

	/* What can we handle */
	if (header.datatypecode != 1 && header.datatypecode != 2 && header.datatypecode != 9 && header.datatypecode != 10) {
		SDL_RWclose(file);
		throw LoadError("can only handle image type 1, 2, 9 and 10");
	}		
	if (header.bitsperpixel != 8 && header.bitsperpixel != 16 && header.bitsperpixel != 24 && header.bitsperpixel != 32) {
		SDL_RWclose(file);
		throw LoadError("can only handle pixel depths of 8, 16, 24 and 32");
	}
	if (header.colourmaptype != 0 && header.colourmaptype != 1) {
		SDL_RWclose(file);
		throw LoadError("can only handle colour map types of 0 and 1");
	}

	/* Skip over unnecessary stuff */
	SDL_RWseek(file, header.idlength, RW_SEEK_CUR);

	/* Read the palette if there is one */
	if (header.colourmaptype == 1) {
		if (header.colourmapdepth != 24) {
			SDL_RWclose(file);
			throw LoadError("can't handle anything but 24 bit palettes");
		}
		if (header.bitsperpixel != 8) {
			SDL_RWclose(file);
			throw LoadError("can only read 8 bpp paletted images");
		}
		int skip = header.colourmaporigin * (header.colourmapdepth / 8);
		SDL_RWseek(file, skip, RW_SEEK_CUR);
		// We can only read 256 colour palettes max, skip the rest
		int size = MIN(header.colourmaplength-skip, 256);
		skip = (header.colourmaplength - size) * (header.colourmapdepth / 8);
		for (i = 0; i < size; i++) {
			header.palette[i].b = SDL_fgetc(file);
			header.palette[i].g = SDL_fgetc(file);
			header.palette[i].r = SDL_fgetc(file);
		}
		SDL_RWseek(file, skip, RW_SEEK_CUR);
	}
	else {
		// Skip the palette on truecolour images
		SDL_RWseek(file, (header.colourmapdepth / 8) * header.colourmaplength, RW_SEEK_CUR);
	}

	/* Read the image */
	bytes2read = header.bitsperpixel / 8;
	while (n < header.width * header.height) {
		if (header.datatypecode == 1 || header.datatypecode == 2) {                     /* Uncompressed */
			if (SDL_RWread(file, p, 1, bytes2read) != bytes2read) {
				delete[] pixels;
				SDL_RWclose(file);
				throw LoadError("unexpected end of file at pixel " + itos(i));
			}
			merge_bytes(pixel_ptr(pixels, n, &header), p, bytes2read, &header);
			n++;
		}
		else if (header.datatypecode == 9 || header.datatypecode == 10) {             /* Compressed */
			if (SDL_RWread(file, p, 1, bytes2read+1) != bytes2read+1) {
				delete[] pixels;
				SDL_RWclose(file);
				throw LoadError("unexpected end of file at pixel " + itos(i));
			}
			j = p[0] & 0x7f;
			merge_bytes(pixel_ptr(pixels, n, &header), &(p[1]), bytes2read, &header);
			n++;
			if (p[0] & 0x80) {         /* RLE chunk */
				for (i = 0; i < j; i++) {
					merge_bytes(pixel_ptr(pixels, n, &header), &(p[1]), bytes2read, &header);
					n++;
				}
			}
			else {                   /* Normal chunk */
				for (i = 0; i < j; i++) {
					if (SDL_RWread(file, p, 1, bytes2read) != bytes2read) {
						delete[] pixels;
						SDL_RWclose(file);
						throw LoadError("unexpected end of file at pixel " + itos(i));
					}
					merge_bytes(pixel_ptr(pixels, n, &header), p, bytes2read, &header);
					n++;
				}
			}
		}
	}

	SDL_RWclose(file);

	return pixels;
}

Image::Internal::Internal(std::string filename) :
	filename(filename),
	refcount(1)
{
	reload();
}

Image::Internal::Internal(unsigned char *pixels, Size<int> size) :
	size(size)
{
	filename = "--FROM SURFACE--";
	upload(pixels);
}

Image::Internal::~Internal()
{
	release();
}

void Image::Internal::release()
{
	if (noo.opengl) {
		glDeleteTextures(1, &texture);
		printGLerror("glDeleteTextures");
		glDeleteBuffers(1, &vbo);
		printGLerror("glDeleteBuffers");
		glDeleteVertexArrays(1, &vao);
	}
#ifdef NOOSKEWL_ENGINE_WINDOWS
	else {
		video_texture->Release();
	}
#endif
}

void Image::Internal::reload()
{
	unsigned char *pixels = Image::read_tga(filename, size);

	try {
		upload(pixels);
	}
	catch (Error e) {
		delete[] pixels;
		throw e;
	}

	delete[] pixels;
}

void Image::Internal::upload(unsigned char *pixels)
{
	/*
	// To get a complete palette..
	unsigned char *rgb = pixels;
	for (int i = 0; i < size.w*size.h; i++) {
		if (rgb[3] != 0) {
			printf("rgb: %d %d %d\n", rgb[0], rgb[1], rgb[2]);
		}
		rgb += 4;
	}
	*/

	if (noo.opengl) {
		glGenVertexArrays(1, &vao);
		printGLerror("glBindVertexArrays");
		glBindVertexArray(vao);
		printGLerror("glBindVertexArray");

		glGenBuffers(1, &vbo);
		printGLerror("glGenBuffers");
		if (vbo == 0) {
			glDeleteVertexArrays(1, &vao);
			printGLerror("glDeleteVertexArrays");
			vao = 0;
			throw GLError("glBenBuffers failed");
		}
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		printGLerror("glBindBuffer");

		glGenTextures(1, &texture);
		printGLerror("glGenTextures");
		if (texture == 0) {
			glDeleteVertexArrays(1, &vao);
			vao = 0;
			glDeleteBuffers(1, &vbo);
			printGLerror("glDeleteBuffers");
			vbo = 0;
			throw GLError("glGenTextures failed");
		}

		glBindTexture(GL_TEXTURE_2D, texture);
		printGLerror("glBindTexture");
		glActiveTexture(GL_TEXTURE0);
		printGLerror("glActiveTexture");

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.w, size.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		printGLerror("glTexImage2D");

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		printGLerror("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		printGLerror("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		printGLerror("glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		printGLerror("glTexParameteri");
	}
#ifdef NOOSKEWL_ENGINE_WINDOWS
	else {
		int err = noo.d3d_device->CreateTexture(size.w, size.h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &video_texture, 0);
		D3DLOCKED_RECT locked_rect;
		if (video_texture->LockRect(0, &locked_rect, 0, 0) == D3D_OK) {
			for (int y = 0; y < size.h; y++) {
				unsigned char *dest = ((unsigned char *)locked_rect.pBits) + y * locked_rect.Pitch;
				for (int x = 0; x < size.w; x++) {
					unsigned char r = *pixels++;
					unsigned char g = *pixels++;
					unsigned char b = *pixels++;
					unsigned char a = *pixels++;
					*dest++ = b;
					*dest++ = g;
					*dest++ = r;
					*dest++ = a;
				}
			}
			video_texture->UnlockRect(0);
		}
		else {
			infomsg("Unable to lock texture\n");
		}
	}
#endif
}
