
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

#include "bcdisplayinfo.h"
#include "language.h"
#include "rgb601window.h"











RGB601Window::RGB601Window(RGB601Main *client)
 : PluginClientWindow(client, xS(235), yS(100), xS(235), yS(100), 0)
{
	this->client = client;
}

RGB601Window::~RGB601Window()
{
}

void RGB601Window::create_objects()
{
	int xs10 = xS(10);
	int ys10 = yS(10), ys30 = yS(30);
	int x = xs10, y = ys10;

	add_tool(forward = new RGB601Direction(this,
		x,
		y,
		&client->config.direction,
		1,
		_("RGB -> 601 compression")));
	y += ys30;
	add_tool(reverse = new RGB601Direction(this,
		x,
		y,
		&client->config.direction,
		2,
		_("601 -> RGB expansion")));

	show_window();
	flush();
}

void RGB601Window::update()
{
	forward->update(client->config.direction == 1);
	reverse->update(client->config.direction == 2);
}



RGB601Direction::RGB601Direction(RGB601Window *window, int x, int y, int *output, int true_value, char *text)
 : BC_CheckBox(x, y, *output == true_value, text)
{
	this->output = output;
	this->true_value = true_value;
	this->window = window;
}
RGB601Direction::~RGB601Direction()
{
}

int RGB601Direction::handle_event()
{
	*output = get_value() ? true_value : 0;
	window->update();
	window->client->send_configure_change();
	return 1;
}

