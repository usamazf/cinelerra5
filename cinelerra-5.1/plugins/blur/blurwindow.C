
/*
 * CINELERRA
 * Copyright (C) 2010 Adam Williams <broadcast at earthling dot net>
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
#include "blur.h"
#include "blurwindow.h"
#include "language.h"






BlurWindow::BlurWindow(BlurMain *client)
 : PluginClientWindow(client,
	xS(210),
	yS(360),
	xS(210),
	yS(360),
	0)
{
	this->client = client;
}

BlurWindow::~BlurWindow()
{
//printf("BlurWindow::~BlurWindow 1\n");
}

void BlurWindow::create_objects()
{
	int xs10 = xS(10);
	int ys10 = yS(10), ys20 = yS(20), ys30 = yS(30), ys35 = yS(35), ys40 = yS(40);
	int x = xs10, y = ys10;
	BC_Title *title;

	add_subwindow(new BC_Title(x, y, _("Blur")));
	y += ys20;
	add_subwindow(horizontal = new BlurHorizontal(client, this, x, y));
	y += ys30;
	add_subwindow(vertical = new BlurVertical(client, this, x, y));
	y += ys35;
	add_subwindow(title = new BC_Title(x, y, _("Radius:")));
	y += title->get_h() + ys10;
	add_subwindow(radius = new BlurRadius(client, this, x, y));
	add_subwindow(radius_text = new BlurRadiusText(client, this, x + radius->get_w() + xs10, y, 100));
	y += radius->get_h() + ys10;
	add_subwindow(a_key = new BlurAKey(client, x, y));
	y += ys30;
	add_subwindow(a = new BlurA(client, x, y));
	y += ys30;
	add_subwindow(r = new BlurR(client, x, y));
	y += ys30;
	add_subwindow(g = new BlurG(client, x, y));
	y += ys30;
	add_subwindow(b = new BlurB(client, x, y));
	y += ys40;
	add_subwindow(reset = new BlurReset(client, this, x, y));

	show_window();
	flush();
}

// for Reset button
void BlurWindow::update()
{
	horizontal->update(client->config.horizontal);
	vertical->update(client->config.vertical);
	radius->update(client->config.radius);
	radius_text->update((int64_t)client->config.radius);
	a_key->update(client->config.a_key);
	a->update(client->config.a);
	r->update(client->config.r);
	g->update(client->config.g);
	b->update(client->config.b);
}


BlurRadius::BlurRadius(BlurMain *client, BlurWindow *gui, int x, int y)
 : BC_IPot(x,
 	y,
	client->config.radius,
	0,
	MAXRADIUS)
{
	this->client = client;
	this->gui = gui;
}
BlurRadius::~BlurRadius()
{
}
int BlurRadius::handle_event()
{
	client->config.radius = get_value();
	gui->radius_text->update((int64_t)client->config.radius);
	client->send_configure_change();
	return 1;
}




BlurRadiusText::BlurRadiusText(BlurMain *client, BlurWindow *gui, int x, int y, int w)
 : BC_TextBox(x,
	y,
	w,
	1,
	client->config.radius)
{
	this->client = client;
	this->gui = gui;
}

int BlurRadiusText::handle_event()
{
	client->config.radius = atoi(get_text());
	gui->radius->update((int64_t)client->config.radius);
	client->send_configure_change();
	return 1;
}




BlurVertical::BlurVertical(BlurMain *client, BlurWindow *window, int x, int y)
 : BC_CheckBox(x,
 	y,
	client->config.vertical,
	_("Vertical"))
{
	this->client = client;
	this->window = window;
}
BlurVertical::~BlurVertical()
{
}
int BlurVertical::handle_event()
{
	client->config.vertical = get_value();
	client->send_configure_change();
	return 1;
}

BlurHorizontal::BlurHorizontal(BlurMain *client, BlurWindow *window, int x, int y)
 : BC_CheckBox(x,
 	y,
	client->config.horizontal,
	_("Horizontal"))
{
	this->client = client;
	this->window = window;
}
BlurHorizontal::~BlurHorizontal()
{
}
int BlurHorizontal::handle_event()
{
	client->config.horizontal = get_value();
	client->send_configure_change();
	return 1;
}




BlurA::BlurA(BlurMain *client, int x, int y)
 : BC_CheckBox(x, y, client->config.a, _("Blur alpha"))
{
	this->client = client;
}
int BlurA::handle_event()
{
	client->config.a = get_value();
	client->send_configure_change();
	return 1;
}




BlurAKey::BlurAKey(BlurMain *client, int x, int y)
 : BC_CheckBox(x, y, client->config.a_key, _("Alpha determines radius"))
{
	this->client = client;
}
int BlurAKey::handle_event()
{
	client->config.a_key = get_value();
	client->send_configure_change();
	return 1;
}

BlurR::BlurR(BlurMain *client, int x, int y)
 : BC_CheckBox(x, y, client->config.r, _("Blur red"))
{
	this->client = client;
}
int BlurR::handle_event()
{
	client->config.r = get_value();
	client->send_configure_change();
	return 1;
}

BlurG::BlurG(BlurMain *client, int x, int y)
 : BC_CheckBox(x, y, client->config.g, _("Blur green"))
{
	this->client = client;
}
int BlurG::handle_event()
{
	client->config.g = get_value();
	client->send_configure_change();
	return 1;
}

BlurB::BlurB(BlurMain *client, int x, int y)
 : BC_CheckBox(x, y, client->config.b, _("Blur blue"))
{
	this->client = client;
}
int BlurB::handle_event()
{
	client->config.b = get_value();
	client->send_configure_change();
	return 1;
}

BlurReset::BlurReset(BlurMain *client, BlurWindow *window, int x, int y)
 : BC_GenericButton(x, y, _("Reset"))
{
	this->client = client;
	this->window = window;
}
BlurReset::~BlurReset()
{
}
int BlurReset::handle_event()
{
	client->config.reset();
	window->update();
	client->send_configure_change();
	return 1;
}
