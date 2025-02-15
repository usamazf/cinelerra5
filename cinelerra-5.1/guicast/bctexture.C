
/*
 * CINELERRA
 * Copyright (C) 2008 Adam Williams <broadcast at earthling dot net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define GL_GLEXT_PROTOTYPES

#include "bcsynchronous.h"
#include "bctexture.h"
#include "bcwindowbase.h"
#include "bccmodels.h"


BC_Texture::BC_Texture(int w, int h, int colormodel)
{
	this->w = w;
	this->h = h;
	this->colormodel = colormodel;
	texture_id = -1;
	texture_w = 0;
	texture_h = 0;
	texture_components = 0;
	window_id = -1;
	create_texture(w, h, colormodel);
}


BC_Texture::~BC_Texture()
{
	clear_objects();
}

void BC_Texture::clear_objects()
{
	if(get_texture_id() >= 0)
	{
// printf("VFrame::clear_objects %p window_id=%d texture_id=%d w=%d h=%d\n",
// this, window_id, texture_id, texture_w, texture_h);
		BC_WindowBase::get_synchronous()->release_texture(
			window_id,
			texture_id);
		texture_id = -1;
	}
}

void BC_Texture::new_texture(BC_Texture **texture,
	int w, int h, int colormodel)
{
	if(!(*texture)) {
		(*texture) = new BC_Texture(w, h, colormodel);
	}
	else {
		(*texture)->create_texture(w, h, colormodel);
	}
}

void BC_Texture::create_texture(int w, int h, int colormodel)
{
#ifdef HAVE_GL

// Get max texture size from the server.
// Maximum size was 4096 on the earliest cards that could do video.
 	int max_texture_size = 0;
 	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

// Calculate dimensions of texture
	int new_w = calculate_texture_size(w, &max_texture_size);
	int new_h = calculate_texture_size(h, &max_texture_size);
	int new_components = BC_CModels::components(colormodel);

	if(new_w < w || new_h < h) {
		printf("BC_Texture::create_texture frame size %dx%d bigger than maximum texture %dx%d.\n",
			w, h, max_texture_size, max_texture_size);
	}

// Delete existing texture
	if(texture_id >= 0 && (new_h != texture_h || new_w != texture_w ||
		new_components != texture_components ||
		BC_WindowBase::get_synchronous()->current_window->get_id() != window_id))
	{
// printf("BC_Texture::create_texture released window_id=%d texture_id=%d\n",
// BC_WindowBase::get_synchronous()->current_window->get_id(),
// texture_id);
		BC_WindowBase::get_synchronous()->release_texture(
			window_id,
			texture_id);
		texture_id = -1;
		window_id = -1;
	}


	texture_w = new_w;
	texture_h = new_h;
	texture_components = new_components;

// Get matching texture
	if(texture_id < 0) {
		texture_id = BC_WindowBase::get_synchronous()->get_texture(
			texture_w, texture_h, texture_components);
// A new VFrame has no window_id, so it must read it from the matching texture.
		if(texture_id >= 0)
			window_id = BC_WindowBase::get_synchronous()->current_window->get_id();
	}

// No matching texture exists.
// Create new texture with the proper dimensions
	if(texture_id < 0) {
		glGenTextures(1, (GLuint*)&texture_id);
		glBindTexture(GL_TEXTURE_2D, (GLuint)texture_id);
		glEnable(GL_TEXTURE_2D);
		int internal_format = texture_components == 4 ? GL_RGBA8 : GL_RGB8 ;
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture_w, texture_h,
				0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		window_id = BC_WindowBase::get_synchronous()->current_window->get_id();
		BC_WindowBase::get_synchronous()->put_texture(texture_id,
			texture_w, texture_h, texture_components);
//printf("BC_Texture::new_texture created texture_id=%d window_id=%d w=%d h=%d\n",
// texture_id, window_id, texture_w, texture_h);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, (GLuint)texture_id);
		glEnable(GL_TEXTURE_2D);
	}
#endif
}

int BC_Texture::calculate_texture_size(int w, int *max)
{
	int i;
	for(i = 2; (max && i <= *max) || (!max && i < w); i *= 2)
	{
		if(i >= w)
		{
			return i;
			break;
		}
	}
	if(max && i > *max) return 16;
	return i;
}

int BC_Texture::get_texture_id()
{
	return texture_id;
}

int BC_Texture::get_texture_w()
{
	return texture_w;
}

int BC_Texture::get_texture_h()
{
	return texture_h;
}

int BC_Texture::get_texture_components()
{
	return texture_components;
}

int BC_Texture::get_window_id()
{
	return window_id;
}


void BC_Texture::draw_texture(
	float in_x1, float in_y1, float in_x2, float in_y2,
	float out_x1, float out_y1, float out_x2, float out_y2)
{
#ifdef HAVE_GL
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1.0);
	glTexCoord2f(in_x1, in_y1);   glVertex3f(out_x1, out_y1, 0);
	glTexCoord2f(in_x2, in_y1);   glVertex3f(out_x2, out_y1, 0);
	glTexCoord2f(in_x2, in_y2);   glVertex3f(out_x2, out_y2, 0);
	glTexCoord2f(in_x1, in_y2);   glVertex3f(out_x1, out_y2, 0);
	glEnd();
#endif
}

void BC_Texture::bind(int texture_unit, int nearest)
{
#ifdef HAVE_GL
// Bind the texture
	if(texture_id >= 0)
	{
		if(texture_unit >= 0) glActiveTexture(GL_TEXTURE0 + texture_unit);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glEnable(GL_TEXTURE_2D);
		if(texture_unit >= 0) {
			int filter = nearest ? GL_NEAREST : GL_LINEAR;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

// GL_REPEAT in this case causes the upper left corners of the masks
// to blur.
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

// Get the texture to alpha blend
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
	}
#endif
}

#ifdef HAVE_GL
static void write_ppm(uint8_t *tp, int w, int h, const char *fmt, ...)
{
  va_list ap;    va_start(ap, fmt);
  char fn[256];  vsnprintf(fn, sizeof(fn), fmt, ap);
  va_end(ap);
  FILE *fp = !strcmp(fn,"-") ? stdout : fopen(fn,"w");
  if( fp ) {
    fprintf(fp,"P6\n%d %d\n255\n",w,h);
    fwrite(tp,3*w,h,fp);
    if( fp != stdout ) fclose(fp);
  }
}
#endif

void BC_Texture::write_tex(const char *fn, int id)
{
#ifdef HAVE_GL
	int prev_id = -1;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_id);
	glActiveTexture(GL_TEXTURE0+id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glEnable(GL_TEXTURE_2D);
	int w = get_texture_w(), h = get_texture_h();
	uint8_t *img = new uint8_t[w*h*3];
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
	write_ppm(img, w, h, "%s", fn);
	delete [] img;
	glActiveTexture(prev_id);
#endif
}

void BC_Texture::write_tex(const char *fn)
{
#ifdef HAVE_GL
	write_tex(fn, 31);
#endif
}

