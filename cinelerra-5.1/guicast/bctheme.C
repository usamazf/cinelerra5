
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

#include "bcresources.h"
#include "bctheme.h"
#include "bcwindowbase.h"
#include "clip.h"
#include "language.h"
#include "vframe.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

BC_Theme::BC_Theme()
{
	last_image_data = 0;
	last_image_set = 0;
	image_sets_start = -1;
}

BC_Theme::~BC_Theme()
{
	image_sets.remove_all_objects();
	images.remove_all_objects();
}

void BC_Theme::dump()
{
	printf("BC_Theme::dump 1 images=%d\n", images.size());
	for( int i=0; i<images.size(); ++i ) {
		BC_ImageData *image_data = images[i];
		printf("%4d. %s %p\n", i, image_data->name, image_data->data);
	}
	printf("BC_Theme::dump 2 image_sets=%d\n", image_sets.size());
	for( int i=0; i<image_sets.size(); ++i ) {
		BC_ThemeSet *image_set = image_sets[i];
		printf("%4d. %s %p\n", i, image_set->title, image_set->data[0]);
	}
}

BC_Resources* BC_Theme::get_resources()
{
	return BC_WindowBase::get_resources();
}

// These create single images for storage in the image_sets table.
VFrame* BC_Theme::new_image(const char *title, const char *path)
{
	VFrame *existing_image = title[0] ? get_image(title, 0) : 0;
	if( existing_image ) return existing_image;

	BC_ThemeSet *result = new BC_ThemeSet(1, 0, title);
	result->data[0] = new VFramePng(get_image_data(path));
	add_image_set(result);
	return result->data[0];
}

VFrame* BC_Theme::new_image(const char *path)
{
	return new_image("", path);
}

VFrame* BC_Theme::new_image1(const char *title, const char *path)
{
	VFrame *existing_image = title[0] ? get_image(title, 0) : 0;
	if( existing_image ) return existing_image;

	BC_ThemeSet *result = new BC_ThemeSet(1, 0, title);
	result->data[0] = new VFramePng(get_image_data(path), 1.);
	add_image_set(result);
	return result->data[0];
}

// These create image sets which are stored in the image_sets table.
VFrame** BC_Theme::new_image_set(const char *title, int total, va_list *args)
{
	if( !total ) {
		printf("BC_Theme::new_image_set %d %s zero number of images\n",
			__LINE__, title);
	}

	VFrame **existing_image_set = title[0] ? get_image_set(title, 0) : 0;
	if( existing_image_set ) return existing_image_set;

	BC_ThemeSet *result = new BC_ThemeSet(total, 1, title);
	add_image_set(result);
	for( int i=0; i<total; ++i ) {
		char *path = va_arg(*args, char*);
		result->data[i] = new_image(path);
	}
	return result->data;
}

VFrame** BC_Theme::new_image_set_images(const char *title, int total, ...)
{
	va_list list;
	va_start(list, total);
	BC_ThemeSet *existing_image_set = title[0] ? get_image_set_object(title) : 0;
	if( existing_image_set ) {
		image_sets.remove_object(existing_image_set);
		last_image_set = 0;
		last_image_data = 0;
	}

	BC_ThemeSet *result = new BC_ThemeSet(total, 0, title);
	add_image_set(result);
	for( int i=0; i<total; ++i ) {
		result->data[i] = va_arg(list, VFrame*);
	}
	va_end(list);
	return result->data;
}

VFrame** BC_Theme::new_image_set(const char *title, int total, ...)
{
	va_list list;
	va_start(list, total);
	VFrame **result = new_image_set(title, total, &list);
	va_end(list);

	return result;
}

VFrame** BC_Theme::new_image_set(int total, ...)
{
	va_list list;
	va_start(list, total);
	VFrame **result = new_image_set("", total, &list);
	va_end(list);

	return result;
}

void BC_Theme::add_image_set(BC_ThemeSet *image_set)
{
	image_sets.append(image_set);
	if( image_sets_start >= 0 ) {
		printf("BC_Theme::add_image_set image_sets unsorted, lookups degraded\n");
		image_sets_start = -1;
	}
}

int BC_Theme::image_set_cmpr(const void *ap, const void *bp)
{
	BC_ThemeSet*a = *(BC_ThemeSet**)ap, *b = *(BC_ThemeSet**)bp;
        return strcmp(a->title, b->title);
}

void BC_Theme::sort_image_sets()
{
	if( image_sets_start >= 0 ) return;
	qsort(&image_sets[0], image_sets.size(), sizeof(image_sets[0]), image_set_cmpr);
// skip over un-titled image sets
	int i = 0, n = image_sets.size();
	while( i<n && !image_sets[i]->title[0] ) ++i;
	image_sets_start = i;
}

BC_ThemeSet* BC_Theme::get_image_set_object(const char *title)
{
	if( last_image_set && !strcmp(title,last_image_set->title) )
		return last_image_set;

	if( image_sets_start >= 0 ) {
// binary search for image set
		int r = image_sets.size(), l = image_sets_start-1;
		int m = 0, v = -1;
		while( r-l > 1 ) {
			m = (l + r) / 2;
			BC_ThemeSet *image_set = image_sets[m];
			if( !(v=strcmp(title, image_set->title)) )
				return last_image_set = image_set;
			if( v > 0 ) l = m; else r = m;
		}
	}
	else {
// compare title[0],title[1] for faster prefix test
		const unsigned char *tp = (const unsigned char*)title;
		unsigned short tval = tp[0];
		if( tval ) tval |= (tp[1] << 8);

		for( int i=0; i<image_sets.size(); ++i ) {
			tp = (const unsigned char *)image_sets[i]->title;
			unsigned short val = tp[0];
			if( val ) val |= (tp[1] << 8);
			if( val != tval ) continue;
			if( !strcmp((const char *)tp, title) )
				return last_image_set = image_sets[i];
		}
	}

	return 0;
}

VFrame* BC_Theme::get_image(const char *title, int use_default)
{
	BC_ThemeSet* tsp = get_image_set_object(title);
	if( tsp ) return tsp->data[0];


// Return the first image it can find.  This should always work.
	if( use_default ) {
		printf("BC_Theme::get_image: image \"%s\" not found.\n",
			title);
		if( image_sets.size() )
			return image_sets[0]->data[0];
	}

// Give up and go to a movie.
	return 0;
}

VFrame** BC_Theme::get_image_set(const char *title, int use_default)
{
	BC_ThemeSet* tsp = get_image_set_object(title);
	if( tsp ) return tsp->data;

// Get the image set with the largest number of images.
	if( use_default ) {
		printf("BC_Theme::get_image_set: image set \"%s\" not found.\n",
			title);
		int max_total = 0;
		int max_number = -1;
		for( int i=0; i<image_sets.size(); ++i ) {
			if( image_sets[i]->total > max_total ) {
				max_total = image_sets[i]->total;
				max_number = i;
			}
		}

		if( max_number >= 0 )
			return image_sets[max_number]->data;
	}

// Give up and go to a movie
	return 0;
}











VFrame** BC_Theme::new_button(const char *overlay_path,
	const char *up_path,
	const char *hi_path,
	const char *dn_path,
	const char *title)
{
	VFramePng default_data(get_image_data(overlay_path));
	BC_ThemeSet *result = new BC_ThemeSet(3, 1, title ? title : "");
	if( title ) add_image_set(result);

	result->data[0] = new_image(up_path);
	result->data[1] = new_image(hi_path);
	result->data[2] = new_image(dn_path);
	for( int i=0; i<3; ++i ) {
		overlay(result->data[i], &default_data, -1, -1, (i == 2));
	}
	return result->data;
}


VFrame** BC_Theme::new_button4(const char *overlay_path,
	const char *up_path,
	const char *hi_path,
	const char *dn_path,
	const char *disabled_path,
	const char *title)
{
	VFramePng default_data(get_image_data(overlay_path));
	BC_ThemeSet *result = new BC_ThemeSet(4, 1, title ? title : "");
	if( title ) add_image_set(result);

	result->data[0] = new_image(up_path);
	result->data[1] = new_image(hi_path);
	result->data[2] = new_image(dn_path);
	result->data[3] = new_image(disabled_path);
	for( int i=0; i<4; ++i ) {
		overlay(result->data[i], &default_data, -1, -1, (i == 2));
	}
	return result->data;
}


VFrame** BC_Theme::new_button(const char *overlay_path,
	VFrame *up,
	VFrame *hi,
	VFrame *dn,
	const char *title)
{
	VFramePng default_data(get_image_data(overlay_path));
	BC_ThemeSet *result = new BC_ThemeSet(3, 0, title ? title : "");
	if( title ) add_image_set(result);

	result->data[0] = new VFrame(*up);
	result->data[1] = new VFrame(*hi);
	result->data[2] = new VFrame(*dn);
	for( int i=0; i<3; ++i )
		overlay(result->data[i], &default_data, -1, -1, (i == 2));
	return result->data;
}


VFrame** BC_Theme::new_toggle(const char *overlay_path,
	const char *up_path,
	const char *hi_path,
	const char *checked_path,
	const char *dn_path,
	const char *checkedhi_path,
	const char *title)
{
	VFramePng default_data(get_image_data(overlay_path));
	BC_ThemeSet *result = new BC_ThemeSet(5, 1, title ? title : "");
	if( title ) add_image_set(result);

	result->data[0] = new_image(up_path);
	result->data[1] = new_image(hi_path);
	result->data[2] = new_image(checked_path);
	result->data[3] = new_image(dn_path);
	result->data[4] = new_image(checkedhi_path);
	for( int i=0; i<5; ++i )
		overlay(result->data[i], &default_data, -1, -1, (i == 3));
	return result->data;
}

VFrame** BC_Theme::new_toggle(const char *overlay_path,
	VFrame *up,
	VFrame *hi,
	VFrame *checked,
	VFrame *dn,
	VFrame *checkedhi,
	const char *title)
{
	VFramePng default_data(get_image_data(overlay_path));
	BC_ThemeSet *result = new BC_ThemeSet(5, 0, title ? title : "");
	if( title ) add_image_set(result);

	result->data[0] = new VFrame(*up);
	result->data[1] = new VFrame(*hi);
	result->data[2] = new VFrame(*checked);
	result->data[3] = new VFrame(*dn);
	result->data[4] = new VFrame(*checkedhi);
	for( int i=0; i<5; ++i )
		overlay(result->data[i], &default_data, -1, -1, (i == 3));
	return result->data;
}

void BC_Theme::overlay(VFrame *dst, VFrame *src, int in_x1, int in_x2, int shift)
{
	int w;
	int h;
	unsigned char **in_rows;
	unsigned char **out_rows;

	if( in_x1 < 0 ) {
		w = MIN(src->get_w(), dst->get_w());
		h = MIN(dst->get_h(), src->get_h());
		in_x1 = 0;
		in_x2 = w;
	}
	else {
		w = in_x2 - in_x1;
		h = MIN(dst->get_h(), src->get_h());
	}
	in_rows = src->get_rows();
	out_rows = dst->get_rows();

	switch( src->get_color_model() )
	{
		case BC_RGBA8888:
			switch( dst->get_color_model() )
			{
				case BC_RGBA8888:
					for( int i=shift; i<h; ++i ) {
						unsigned char *in_row = 0;
						unsigned char *out_row;

						if( !shift ) {
							in_row = in_rows[i] + in_x1 * 4;
							out_row = out_rows[i];
						}
						else {
							in_row = in_rows[i - 1] + in_x1 * 4;
							out_row = out_rows[i] + 4;
						}

						for( int j=shift; j<w; ++j ) {
							int opacity = in_row[3];
							int transparency = 0xff - opacity;

							out_row[0] = (in_row[0] * opacity + out_row[0] * transparency) / 0xff;
							out_row[1] = (in_row[1] * opacity + out_row[1] * transparency) / 0xff;
							out_row[2] = (in_row[2] * opacity + out_row[2] * transparency) / 0xff;
							out_row[3] = MAX(in_row[3], out_row[3]);
							out_row += 4;
							in_row += 4;
						}
					}
					break;

				case BC_RGB888:
					for( int i=shift; i<h; ++i ) {
						unsigned char *in_row;
						unsigned char *out_row = out_rows[i];

						if( !shift ) {
							in_row = in_rows[i] + in_x1 * 3;
							out_row = out_rows[i];
						}
						else {
							in_row = in_rows[i - 1] + in_x1 * 3;
							out_row = out_rows[i] + 3;
						}

						for( int j=shift; j<w; ++j ) {
							int opacity = in_row[3];
							int transparency = 0xff - opacity;
							out_row[0] = (in_row[0] * opacity + out_row[0] * transparency) / 0xff;
							out_row[1] = (in_row[1] * opacity + out_row[1] * transparency) / 0xff;
							out_row[2] = (in_row[2] * opacity + out_row[2] * transparency) / 0xff;
							out_row += 3;
							in_row += 4;
						}
					}
					break;
			}
			break;
	}
}

void BC_Theme::set_data(unsigned char *ptr)
{
	int hdr_sz = *(int*)ptr - sizeof(int);
	unsigned char *cp = ptr + sizeof(int);
	unsigned char *dp = cp + hdr_sz;
	int start_item = images.size();

	while( cp < dp ) {
		char *nm = (char *)cp;
		while( cp < dp && *cp++ );
		if( cp + sizeof(unsigned) > dp ) break;
		unsigned ofs = 0;
		for( int i=sizeof(unsigned); --i>=0; ofs|=cp[i] ) ofs <<= 8;
		images.append(new BC_ImageData(nm, dp+ofs));
		cp += sizeof(unsigned);
	}

	int items = images.size() - start_item;
	data_items.append(items);
	qsort(&images[start_item], items, sizeof(images[0]), images_cmpr);
}

int BC_Theme::images_cmpr(const void *ap, const void *bp)
{
	BC_ImageData *a = *(BC_ImageData**)ap, *b = *(BC_ImageData**)bp;
        return strcmp(a->name, b->name);
}

unsigned char* BC_Theme::get_image_data(const char *name, int log_errs)
{
// Image is the same as the last one
	if( last_image_data && !strcmp(last_image_data->name, name) )
		return last_image_data->data;

// look forwards thru data sets for name
	int start_item = 0;
	for( int i=0,n=data_items.size(); i<n; ++i ) {
		int end_item = start_item + data_items[i];
		int r = end_item, l = start_item-1;
// binary search for image
		int m = 0, v = -1;
		while( r-l > 1 ) {
			m = (l + r) / 2;
			BC_ImageData *image_data = images[m];
			if( !(v=strcmp(name, image_data->name)) ) {
				image_data->used = 1;
				last_image_data = image_data;
				return image_data->data;
			}
			if( v > 0 ) l = m; else r = m;
		}
		start_item = end_item;
	}

	if( log_errs )
		fprintf(stderr, _("Theme::get_image: %s not found.\n"), name);
	return 0;
}

void BC_Theme::check_used()
{
// Can't use because some images are gotten the old fashioned way.
return;
	int got_it = 0;
	for( int i=0; i<images.size(); ++i ) {
		if( !images[i]->used ) {
			if( !got_it ) printf(_("BC_Theme::check_used: Images aren't used.\n"));
			printf("%s ", images[i]->name);
			got_it = 1;
		}
	}
	if( got_it ) printf("\n");
}


BC_ThemeSet::BC_ThemeSet(int total, int is_reference, const char *title)
{
	this->total = total;
	this->title = new char[strlen(title) + 1];
	strcpy(this->title, title);
	this->is_reference = is_reference;
	data = new VFrame*[total];
}

BC_ThemeSet::~BC_ThemeSet()
{
	if( data ) {
		if( !is_reference ) {
			for( int i = 0; i < total; i++ )
				delete data[i];
		}

		delete [] data;
	}

	delete [] title;
}

