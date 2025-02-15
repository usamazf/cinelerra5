
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

#include "assets.h"
#include "file.h"
#include "formatwindow.h"
#include <string.h>


FormatAWindow::FormatAWindow(Asset *asset, int *dither)
 : BC_Window(_(PROGRAM_NAME ": File format"), xS(410),
	yS((asset->format == FILE_WAV) ? 115 : 185), 0, 0)
{ this->asset = asset; this->dither = dither; }

FormatAWindow::~FormatAWindow()
{
}

void FormatAWindow::create_objects()
{
	lock_window("FormatAWindow::create_objects");
	int x;
	int init_x;
	int y = yS(10);
	File file;
	x = init_x = xS(10);

	add_subwindow(new BC_Title(x, y, _("Set parameters for this audio format:")));
	y += yS(30);
	add_subwindow(new BC_Title(x, y, _("Bits:")));
	x += yS(45);
	add_subwindow(new FormatBits(x, y, asset));
	x += xS(100);
	add_subwindow(new FormatDither(x, y, this->dither));

	if(asset->format == FILE_PCM)
	{
		x += xS(90);
		add_subwindow(new FormatSigned(x, y, asset));
	}
	y += yS(40);
	x = init_x;

	if(asset->format == FILE_PCM)
	{
		add_subwindow(new BC_Title(x, y, _("Byte order:")));
		y += yS(25);
		add_subwindow(new BC_Title(x, y, _("HiLo:"), SMALLFONT));
		add_subwindow(hilo_button = new FormatHILO(x + xS(30), y, asset));
		x += xS(50);
		add_subwindow(new BC_Title(x, y, _("LoHi:"), SMALLFONT));
		add_subwindow(lohi_button = new FormatLOHI(x + xS(30), y, hilo_button, asset));
		hilo_button->lohi = lohi_button;
		y += yS(30);
	}

	x = init_x;

	add_subwindow(new BC_OKButton(x + xS(170), y));
	unlock_window();
}


int FormatAWindow::close_event()
{
	set_done(0);
}



FormatVWindow::FormatVWindow(Asset *asset, int recording)
 : BC_Window(_(PROGRAM_NAME ": File format"), xS(410), yS(115), 0, 0)
{
	this->asset = asset;
	this->recording = recording;
}

FormatVWindow::~FormatVWindow()
{
}

void FormatVWindow::create_objects()
{
	lock_window("FormatVWindow::create_objects");
	int x, y = yS(10);
	int init_x;

	init_x = x = xS(10);

	if(asset->format == FILE_JPEG_LIST)
	{
		add_subwindow(new BC_Title(x, y, _("Set parameters for this video format:")));
		y += yS(30);
		add_subwindow(new BC_Title(x, y, _("Quality:")));
		x += xS(70);
		add_subwindow(new FormatQuality(x, y, asset, asset->quality));
		y += yS(40);
		x = init_x;
	}
	else
	{
		add_subwindow(new BC_Title(x, y, _("Video is not supported in this format.")));
		y += yS(40);
	}

	add_subwindow(new BC_OKButton(x + xS(170), y));
	unlock_window();
}

int FormatVWindow::close_event()
{
	set_done(0);
}


FormatQuality::FormatQuality(int x, int y, Asset *asset, int default_)
 : BC_ISlider(x, y, 0, xS(100), xS(100), 0, 100, default_, 1)
{
	this->asset = asset;
}
FormatQuality::~FormatQuality()
{
}
int FormatQuality::handle_event()
{
	asset->quality = get_value();
}


FormatBits::FormatBits(int x, int y, Asset *asset)
 : BitsPopup(x, y, asset)
{ this->asset = asset; }
FormatBits::~FormatBits() {}
int FormatBits::handle_event()
{
	asset->bits = get_bits();
}


FormatDither::FormatDither(int x, int y, int *dither)
 : BC_CheckBox(x, y, *dither, _("Dither"))
{ this->dither = dither; }
FormatDither::~FormatDither() {}
int FormatDither::handle_event()
{
	*dither = get_value();
}




FormatSigned::FormatSigned(int x, int y, Asset *asset)
 : BC_CheckBox(x, y, asset->signed_, _("Signed"))
{ this->asset = asset; }
FormatSigned::~FormatSigned() {}
int FormatSigned::handle_event()
{
	asset->signed_ = get_value();
}


FormatHILO::FormatHILO(int x, int y, Asset *asset)
 : BC_Radial(x, y, asset->byte_order ^ 1)
{
	this->asset = asset;
}
FormatHILO::~FormatHILO() {}

int FormatHILO::handle_event()
{
	asset->byte_order = get_value() ^ 1;
	lohi->update(get_value() ^ 1);
}

FormatLOHI::FormatLOHI(int x, int y, FormatHILO *hilo, Asset *asset)
 : BC_Radial(x, y, asset->byte_order)
{
	this->hilo = hilo;
	this->asset = asset;
}
FormatLOHI::~FormatLOHI() {}

int FormatLOHI::handle_event()
{
	asset->byte_order = get_value();
	hilo->update(get_value() ^ 1);
}

