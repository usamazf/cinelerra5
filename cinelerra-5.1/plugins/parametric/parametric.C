
/*
 * CINELERRA
 * Copyright (C) 2011 Adam Williams <broadcast at earthling dot net>
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
#include "bcsignals.h"
#include "clip.h"
#include "bchash.h"
#include "filexml.h"
#include "language.h"
#include "parametric.h"
#include "samples.h"
#include "theme.h"
#include "units.h"
#include "vframe.h"

#include <math.h>
#include <string.h>
#include <unistd.h>









REGISTER_PLUGIN(ParametricEQ)








ParametricBand::ParametricBand()
{
	freq = 440;
	quality = 1;
	magnitude = 0;
	mode = NONE;
}


int ParametricBand::equivalent(ParametricBand &that)
{
	if(freq == that.freq &&
		EQUIV(quality, that.quality) &&
		EQUIV(magnitude, that.magnitude) &&
		mode == that.mode)
	{
		return 1;
	}
	else
		return 0;
}


void ParametricBand::copy_from(ParametricBand &that)
{
	freq = that.freq;
	quality = that.quality;
	magnitude = that.magnitude;
	mode = that.mode;
}

void ParametricBand::interpolate(ParametricBand &prev,
		ParametricBand &next,
		double prev_scale,
		double next_scale)
{
	freq = (int)(prev.freq * prev_scale + next.freq * next_scale + 0.5);
	quality = prev.quality * prev_scale + next.quality * next_scale;
	magnitude = prev.magnitude * prev_scale + next.magnitude * next_scale;
	mode = prev.mode;
}





ParametricConfig::ParametricConfig()
{
	wetness = INFINITYGAIN;
	window_size = 4096;
}


int ParametricConfig::equivalent(ParametricConfig &that)
{
	for(int i = 0; i < BANDS; i++)
		if(!band[i].equivalent(that.band[i])) return 0;

	if(!EQUIV(wetness, that.wetness) ||
		window_size != that.window_size) return 0;
	return 1;
}

void ParametricConfig::copy_from(ParametricConfig &that)
{
	wetness = that.wetness;
	window_size = that.window_size;
	for(int i = 0; i < BANDS; i++)
		band[i].copy_from(that.band[i]);
}

void ParametricConfig::interpolate(ParametricConfig &prev,
		ParametricConfig &next,
		int64_t prev_frame,
		int64_t next_frame,
		int64_t current_frame)
{
	double next_scale = (double)(current_frame - prev_frame) / (next_frame - prev_frame);
	double prev_scale = (double)(next_frame - current_frame) / (next_frame - prev_frame);
	wetness = prev.wetness;
	window_size = prev.window_size;
	for(int i = 0; i < BANDS; i++)
	{
		band[i].interpolate(prev.band[i], next.band[i], prev_scale, next_scale);
	}
}



















ParametricFreq::ParametricFreq(ParametricEQ *plugin, int x, int y, int band)
 : BC_QPot(x, y, plugin->config.band[band].freq)
{
	this->plugin = plugin;
	this->band = band;
}

int ParametricFreq::handle_event()
{
	plugin->config.band[band].freq = get_value();
	plugin->send_configure_change();
	((ParametricWindow*)plugin->thread->window)->update_canvas();
	return 1;
}








ParametricQuality::ParametricQuality(ParametricEQ *plugin, int x, int y, int band)
 : BC_FPot(x, y, plugin->config.band[band].quality, 0, 1)
{
	this->plugin = plugin;
	this->band = band;
	set_precision(0.01);
}

int ParametricQuality::handle_event()
{
	plugin->config.band[band].quality = get_value();
	plugin->send_configure_change();
	((ParametricWindow*)plugin->thread->window)->update_canvas();
	return 1;
}











ParametricMagnitude::ParametricMagnitude(ParametricEQ *plugin, int x, int y, int band)
 : BC_FPot(x, y, plugin->config.band[band].magnitude, -MAXMAGNITUDE, MAXMAGNITUDE)
{
	this->plugin = plugin;
	this->band = band;
}

int ParametricMagnitude::handle_event()
{
	plugin->config.band[band].magnitude = get_value();
	plugin->send_configure_change();
	((ParametricWindow*)plugin->thread->window)->update_canvas();
	return 1;
}









ParametricMode::ParametricMode(ParametricEQ *plugin, int x, int y, int band)
 : BC_PopupMenu(x,
		y,
		xS(150),
		mode_to_text(plugin->config.band[band].mode))
{
//printf("ParametricMode::ParametricMode %d %d\n", band, plugin->config.band[band].mode);
	this->plugin = plugin;
	this->band = band;
}

void ParametricMode::create_objects()
{
	add_item(new BC_MenuItem(mode_to_text(ParametricBand::LOWPASS)));
	add_item(new BC_MenuItem(mode_to_text(ParametricBand::HIGHPASS)));
	add_item(new BC_MenuItem(mode_to_text(ParametricBand::BANDPASS)));
	add_item(new BC_MenuItem(mode_to_text(ParametricBand::NONE)));
}


int ParametricMode::handle_event()
{
	plugin->config.band[band].mode = text_to_mode(get_text());
	plugin->send_configure_change();
	((ParametricWindow*)plugin->thread->window)->update_canvas();
	return 1;
}

int ParametricMode::text_to_mode(char *text)
{
	if(!strcmp(mode_to_text(ParametricBand::LOWPASS), text)) return ParametricBand::LOWPASS;
	if(!strcmp(mode_to_text(ParametricBand::HIGHPASS), text)) return ParametricBand::HIGHPASS;
	if(!strcmp(mode_to_text(ParametricBand::BANDPASS), text)) return ParametricBand::BANDPASS;
	if(!strcmp(mode_to_text(ParametricBand::NONE), text)) return ParametricBand::NONE;
	return ParametricBand::BANDPASS;
}



const char* ParametricMode::mode_to_text(int mode)
{
	switch(mode)
	{
		case ParametricBand::LOWPASS:
			return _("Lowpass");
			break;
		case ParametricBand::HIGHPASS:
			return _("Highpass");
			break;
		case ParametricBand::BANDPASS:
			return _("Bandpass");
			break;
		case ParametricBand::NONE:
			return _("None");
			break;
	}
	return "";
}











ParametricBandGUI::ParametricBandGUI(ParametricEQ *plugin, ParametricWindow *window, int x, int y, int band)
{
	this->plugin = plugin;
	this->band = band;
	this->window = window;
	this->x = x;
	this->y = y;
}

ParametricBandGUI::~ParametricBandGUI()
{
}


#define X1 xS(10)
#define X2 xS(60)
#define X3 xS(110)
#define X4 xS(160)


void ParametricBandGUI::create_objects()
{
	window->add_subwindow(freq = new ParametricFreq(plugin, X1, y, band));
	window->add_subwindow(quality = new ParametricQuality(plugin, X2, y, band));
	window->add_subwindow(magnitude = new ParametricMagnitude(plugin, X3, y, band));
	window->add_subwindow(mode = new ParametricMode(plugin, X4, y, band));
	mode->create_objects();
}

void ParametricBandGUI::update_gui()
{
	freq->update(plugin->config.band[band].freq);
	quality->update(plugin->config.band[band].quality);
	magnitude->update(plugin->config.band[band].magnitude);
	mode->set_text(ParametricMode::mode_to_text(plugin->config.band[band].mode));
}






ParametricWetness::ParametricWetness(ParametricEQ *plugin, int x, int y)
 : BC_FPot(x, y, plugin->config.wetness, INFINITYGAIN, 0)
{
	this->plugin = plugin;
}

int ParametricWetness::handle_event()
{
	plugin->config.wetness = get_value();
	plugin->send_configure_change();
	((ParametricWindow*)plugin->thread->window)->update_canvas();
	return 1;
}







ParametricSize::ParametricSize(ParametricWindow *window, ParametricEQ *plugin, int x, int y)
 : BC_PopupMenu(x, y, xS(120), "4096", 1)
{
	this->plugin = plugin;
	this->window = window;
}

int ParametricSize::handle_event()
{
	plugin->config.window_size = atoi(get_text());
	plugin->send_configure_change();

	window->update_canvas();
	return 1;
}

void ParametricSize::create_objects()
{
	add_item(new BC_MenuItem("2048"));
	add_item(new BC_MenuItem("4096"));
	add_item(new BC_MenuItem("8192"));
	add_item(new BC_MenuItem("16384"));
	add_item(new BC_MenuItem("32768"));
	add_item(new BC_MenuItem("65536"));
	add_item(new BC_MenuItem("131072"));
	add_item(new BC_MenuItem("262144"));
}

void ParametricSize::update(int size)
{
	char string[BCTEXTLEN];
	sprintf(string, "%d", size);
	set_text(string);
}






ParametricWindow::ParametricWindow(ParametricEQ *plugin)
 : PluginClientWindow(plugin,
	xS(350),
	yS(400),
	xS(350),
	yS(400),
	0)
{
	this->plugin = plugin;
}

ParametricWindow::~ParametricWindow()
{
	for(int i = 0; i < BANDS; i++)
		delete bands[i];
}

void ParametricWindow::create_objects()
{
	int y = yS(35);
SET_TRACE

	add_subwindow(new BC_Title(X1, 10, _("Freq")));
	add_subwindow(new BC_Title(X2, 10, _("Qual")));
	add_subwindow(new BC_Title(X3, 10, _("Level")));
	add_subwindow(new BC_Title(X4, 10, _("Mode")));
	for(int i = 0; i < BANDS; i++)
	{
		bands[i] = new ParametricBandGUI(plugin, this, xS(10), y, i);
		bands[i]->create_objects();
		y += yS(50);
	}

SET_TRACE
	BC_Title *title;
	int x = plugin->get_theme()->widget_border;
	add_subwindow(title = new BC_Title(x, y + yS(10), _("Wetness:")));
	x += title->get_w() + plugin->get_theme()->widget_border;
	add_subwindow(wetness = new ParametricWetness(plugin,
		x,
		y));
	x += wetness->get_w() + plugin->get_theme()->widget_border;

	add_subwindow(title = new BC_Title(x, y + yS(10), _("Window:")));
	x += title->get_w() + plugin->get_theme()->widget_border;
	add_subwindow(size = new ParametricSize(this,
		plugin,
		x,
		y + yS(10)));
	size->create_objects();
	size->update(plugin->config.window_size);



	y += yS(50);
	int canvas_x = xS(30);
	int canvas_y = y;
	int canvas_w = get_w() - canvas_x - xS(10);
	int canvas_h = get_h() - canvas_y - yS(30);
	add_subwindow(canvas = new BC_SubWindow(canvas_x,
		canvas_y,
		canvas_w,
		canvas_h,
		BLACK));

SET_TRACE
// Draw canvas titles
	set_font(SMALLFONT);
#define MAJOR_DIVISIONS 4
#define MINOR_DIVISIONS 5
	for(int i = 0; i <= MAJOR_DIVISIONS; i++)
	{
		int y1 = canvas_y + canvas_h - i * (canvas_h / MAJOR_DIVISIONS) - 2;
		int y2 = y1 + yS(3);
		int x1 = canvas_x - xS(25);
		int x2 = canvas_x - xS(10);
		int x3 = canvas_x - xS(2);

		char string[BCTEXTLEN];
		if(i == 0)
			sprintf(string, "oo");
		else
			sprintf(string, "%d", i * 5 - 5);

		set_color(BLACK);
		draw_text(x1 + 1, y2 + 1, string);
		draw_line(x2 + 1, y1 + 1, x3 + 1, y1 + 1);
		set_color(RED);
		draw_text(x1, y2, string);
		draw_line(x2, y1, x3, y1);

		if(i < MAJOR_DIVISIONS)
		{
			for(int j = 1; j < MINOR_DIVISIONS; j++)
			{
				int y3 = y1 - j * (canvas_h / MAJOR_DIVISIONS) / MINOR_DIVISIONS;
				int x4 = x3 - xS(5);
				set_color(BLACK);
				draw_line(x4 + 1, y3 + 1, x3 + 1, y3 + 1);
				set_color(RED);
				draw_line(x4, y3, x3, y3);
			}
		}
	}

SET_TRACE
#undef MAJOR_DIVISIONS
#define MAJOR_DIVISIONS 5
	for(int i = 0; i <= MAJOR_DIVISIONS; i++)
	{
		int freq = Freq::tofreq(i * TOTALFREQS / MAJOR_DIVISIONS);
		int x1 = canvas_x + i * canvas_w / MAJOR_DIVISIONS;
		int y1 = canvas_y + canvas_h + yS(20);
		char string[BCTEXTLEN];
		sprintf(string, "%d", freq);
		int x2 = x1 - get_text_width(SMALLFONT, string);
		int y2 = y1 - yS(10);
		int y3 = y2 - yS(5);
		int y4 = canvas_y + canvas_h;

		set_color(BLACK);
		draw_text(x2 + 1, y1 + 1, string);
		draw_line(x1 + 1, y4 + 1, x1 + 1, y2 + 1);
		set_color(RED);
		draw_text(x2, y1, string);
		draw_line(x1, y4, x1, y2);

		if(i < MAJOR_DIVISIONS)
		{
#undef MINOR_DIVISIONS
#define MINOR_DIVISIONS 5
			for(int j = 0; j < MINOR_DIVISIONS; j++)
			{
				int x3 = (int)(x1 +
					(canvas_w / MAJOR_DIVISIONS) -
					exp(-(double)j * 0.7) *
					(canvas_w / MAJOR_DIVISIONS));
				set_color(BLACK);
				draw_line(x3+1, y4+1, x3+1, y3+1);
				set_color(RED);
				draw_line(x3, y4, x3, y3);
			}
		}
	}

SET_TRACE
	update_canvas();
	show_window();
SET_TRACE
}



void ParametricWindow::update_gui()
{
	for(int i = 0; i < BANDS; i++)
		bands[i]->update_gui();
	wetness->update(plugin->config.wetness);
	size->update(plugin->config.window_size);
	update_canvas();
}


void ParametricWindow::update_canvas()
{
	int y1 = canvas->get_h() / 2;
	int niquist = plugin->PluginAClient::project_sample_rate / 2;

	canvas->clear_box(0, 0, canvas->get_w(), canvas->get_h());

// Draw spectrogram
	double tracking_position = plugin->get_tracking_position();
	ParametricGUIFrame *frame = (ParametricGUIFrame *)
		plugin->get_gui_frame(tracking_position, 1);
// Draw most recent frame
	if( frame ) {
		int y1 = 0, y2 = 0;
		canvas->set_color(MEGREY);

		for(int i = 0; i < canvas->get_w(); i++) {
			int freq = Freq::tofreq(i * TOTALFREQS / canvas->get_w());
			int index = (int64_t)freq * (int64_t)frame->window_size / 2 / niquist;
			if(index < frame->window_size / 2) {
				double magnitude = frame->data[index] /
					frame->freq_max * frame->time_max;
				y2 = (int)(canvas->get_h() -
					(DB::todb(magnitude) - INFINITYGAIN) *
					canvas->get_h() / -INFINITYGAIN);
				CLAMP(y2, 0, canvas->get_h() - 1);
				if(i > 0)
					canvas->draw_line(i - 1, y1, i, y2);
				y1 = y2;
			}
		}
		delete frame;
	}

// 	canvas->set_color(GREEN);
// 	canvas->draw_line(0, wetness, canvas->get_w(), wetness);
// 	canvas->draw_line(0,
// 		wetness,
// 		canvas->get_w(),
// 		wetness);

	canvas->set_color(WHITE);
	canvas->set_line_width(2);

	plugin->calculate_envelope();
	for(int i = 0; i < canvas->get_w(); i++)
	{
		int freq = Freq::tofreq(i * TOTALFREQS / canvas->get_w());
		int index = (int64_t)freq * (int64_t)plugin->config.window_size / 2 / niquist;
		if(freq < niquist && index < plugin->config.window_size / 2)
		{
//printf("ParametricWindow::update_canvas %d %d\n", __LINE__, index);
			double magnitude = plugin->envelope[index];
			int y2 = canvas->get_h() * 3 / 4;

			if(magnitude > 1)
			{
				y2 -= (int)(DB::todb(magnitude) *
					canvas->get_h() *
					3 /
					4 /
					15);
			}
			else
			{
				y2 += (int)((1 - magnitude) * canvas->get_h() / 4);
			}
			if(i > 0) canvas->draw_line(i - 1, y1, i, y2);
			y1 = y2;
		}
		else
		{
			canvas->draw_line(i - 1, y1, i, y1);
		}
	}
	canvas->set_line_width(1);


// 	for(int i = 0; i < canvas->get_w(); i++)
// 	{
// 		int freq = Freq::tofreq((int)((float)i / canvas->get_w() * TOTALFREQS));
// 		int index = (int)((float)freq / niquist * plugin->config.window_size / 2);
// 		double magnitude = plugin->envelope[index];
// 		int y2 = canvas->get_h() -
// 			(int)((double)canvas->get_h() / normalize * magnitude);
// 		if(i > 0) canvas->draw_line(i - 1, y1, i, y2);
// 		y1 = y2;
// 	}

	canvas->flash(1);
}


ParametricGUIFrame::ParametricGUIFrame(int window_size, int sample_rate)
 : PluginClientFrame()
{
	this->window_size = window_size;
	data_size = window_size / 2;
	data = new double[data_size];
	freq_max = 0;
	time_max = 0;
}

ParametricGUIFrame::~ParametricGUIFrame()
{
	delete [] data;
}


ParametricFFT::ParametricFFT(ParametricEQ *plugin)
 : CrossfadeFFT()
{
	this->plugin = plugin;
}

ParametricFFT::~ParametricFFT()
{
}


int ParametricFFT::signal_process()
{
// Create new frame for updating GUI
	frame = new ParametricGUIFrame(window_size,
		plugin->PluginAClient::project_sample_rate);
	plugin->add_gui_frame(frame);

	double freq_max = 0;
	for(int i = 0; i < window_size / 2; i++)
	{

// if(i == 10) printf("ParametricFFT::signal_process %d %f\n",
// __LINE__,
// plugin->envelope[i]);

		double result = plugin->envelope[i] *
			sqrt(freq_real[i] * freq_real[i] + freq_imag[i] * freq_imag[i]);
		double angle = atan2(freq_imag[i], freq_real[i]);
		freq_real[i] = result * cos(angle);
		freq_imag[i] = result * sin(angle);

		frame->data[i] = result;
		if(result > freq_max) freq_max = result;
	}
	frame->freq_max = freq_max;


	symmetry(window_size, freq_real, freq_imag);
	return 0;
}

int ParametricFFT::post_process()
{
	double time_max = 0;
	for(int i = 0; i < window_size; i++)
	{
		if(output_real[i] > time_max) time_max = output_real[i];
	}
	frame->time_max = time_max;
	return 0;
}



int ParametricFFT::read_samples(int64_t output_sample,
	int samples,
	Samples *buffer)
{
	return plugin->read_samples(buffer,
		0,
		plugin->get_samplerate(),
		output_sample,
		samples);
}








ParametricEQ::ParametricEQ(PluginServer *server)
 : PluginAClient(server)
{

	fft = 0;
	need_reconfigure = 1;
	envelope = 0;
	last_frame = 0;
}

ParametricEQ::~ParametricEQ()
{
	delete last_frame;
	delete [] envelope;
	if(fft) delete fft;
}

NEW_WINDOW_MACRO(ParametricEQ, ParametricWindow)

LOAD_CONFIGURATION_MACRO(ParametricEQ, ParametricConfig)


const char* ParametricEQ::plugin_title() { return N_("EQ Parametric"); }
int ParametricEQ::is_realtime() { return 1; }

void ParametricEQ::read_data(KeyFrame *keyframe)
{
	FileXML input;
	input.set_shared_input(keyframe->xbuf);

	int result = 0;
	while(!result)
	{
		result = input.read_tag();

		if(!result)
		{
			if(input.tag.title_is("PARAMETRICEQ"))
			{
				config.wetness = input.tag.get_property("WETNESS", config.wetness);
				config.window_size = input.tag.get_property("WINDOW_SIZE", config.window_size);
			}
			else
			if(input.tag.title_is("BAND"))
			{
				int band = input.tag.get_property("NUMBER", 0);
				config.band[band].freq = input.tag.get_property("FREQ", config.band[band].freq);
				config.band[band].quality = input.tag.get_property("QUALITY", config.band[band].quality);
				config.band[band].magnitude = input.tag.get_property("MAGNITUDE", config.band[band].magnitude);
				config.band[band].mode = input.tag.get_property("MODE", config.band[band].mode);
			}
		}
	}
}

void ParametricEQ::save_data(KeyFrame *keyframe)
{
	FileXML output;
	output.set_shared_output(keyframe->xbuf);

	output.tag.set_title("PARAMETRICEQ");
	output.tag.set_property("WETNESS", config.wetness);
	output.tag.set_property("WINDOW_SIZE", config.window_size);
	output.append_tag();
	output.append_newline();

	for(int i = 0; i < BANDS; i++)
	{
		output.tag.set_title("BAND");
		output.tag.set_property("NUMBER", i);
		output.tag.set_property("FREQ", config.band[i].freq);
		output.tag.set_property("QUALITY", config.band[i].quality);
		output.tag.set_property("MAGNITUDE", config.band[i].magnitude);
		output.tag.set_property("MODE", config.band[i].mode);
		output.append_tag();
		output.tag.set_title("/BAND");
		output.append_tag();
		output.append_newline();
	}

	output.tag.set_title("/PARAMETRICEQ");
	output.append_tag();
	output.append_newline();
	output.terminate_string();
}

void ParametricEQ::reconfigure()
{
	if(fft &&
		config.window_size != fft->window_size)
	{
//printf("ParametricEQ::reconfigure %d %d\n", __LINE__, config.window_size);
		delete fft;
		fft = 0;
	}

	if(!fft)
	{
//printf("ParametricEQ::reconfigure %d %d\n", __LINE__, config.window_size);
		fft = new ParametricFFT(this);
		fft->initialize(config.window_size);
	}

// Reset envelope

//printf("ParametricEQ::reconfigure %f\n", wetness);
	calculate_envelope();

	for(int i = 0; i < config.window_size / 2; i++)
	{
		if(envelope[i] < 0) envelope[i] = 0;
	}

	need_reconfigure = 0;
}

double ParametricEQ::calculate_envelope()
{
	double wetness = DB::fromdb(config.wetness);
	if(EQUIV(config.wetness, INFINITYGAIN)) wetness = 0;
	int niquist = PluginAClient::project_sample_rate / 2;

	if(!envelope) envelope = new double[MAX_WINDOW / 2];

//printf("ParametricEQ::calculate_envelope %d %f\n", __LINE__, wetness);
	for(int i = 0; i < config.window_size / 2; i++)
	{
		envelope[i] = wetness;
	}

	for(int pass = 0; pass < 2; pass++)
	{
		for(int band = 0; band < BANDS; band++)
		{
			switch(config.band[band].mode)
			{
				case ParametricBand::LOWPASS:
					if(pass == 1)
					{
						double magnitude = DB::fromdb(config.band[band].magnitude);
						int cutoff = (int)((double)config.band[band].freq / niquist * config.window_size / 2);
						for(int i = 0; i < config.window_size / 2; i++)
						{
							if(i < cutoff)
								envelope[i] += magnitude;
						}
					}
					break;

				case ParametricBand::HIGHPASS:
					if(pass == 1)
					{
						double magnitude = DB::fromdb(config.band[band].magnitude);
						int cutoff = (int)((double)config.band[band].freq / niquist * config.window_size / 2);
						for(int i = 0; i < config.window_size / 2; i++)
						{
							if(i > cutoff)
								envelope[i] += magnitude;
						}
					}
					break;

				case ParametricBand::BANDPASS:
					if(pass == 0)
					{
						double magnitude = (config.band[band].magnitude > 0) ?
							(DB::fromdb(config.band[band].magnitude) - 1) :
							(-1 + DB::fromdb(config.band[band].magnitude));
						double sigma = (config.band[band].quality < 1) ?
							(1.0 - config.band[band].quality) :
							0.01;
						sigma /= 4;
						double center = (double)Freq::fromfreq(config.band[band].freq) /
							TOTALFREQS;
						double normalize = gauss(sigma, 0, 0);
						if(config.band[band].magnitude <= -MAXMAGNITUDE)
							magnitude = -1;

						for(int i = 0; i < config.window_size / 2; i++)
						{
							int freq = i * niquist / (config.window_size / 2);
							int current_slot = Freq::fromfreq(freq);
							envelope[i] += magnitude *
								gauss(sigma, center, (double)current_slot / TOTALFREQS) /
								normalize;
// printf("freq=%d magnitude=%f gauss=%f normalize=%f envelope[i]=%f\n",
// freq,
// magnitude,
// gauss(sigma, center, (double)current_slot / TOTALFREQS),
// normalize,
// envelope[i]);
						}
					}
					break;
			}
		}
	}

	return 0;
}

double ParametricEQ::gauss(double sigma, double center, double x)
{
	if(EQUIV(sigma, 0)) sigma = 0.01;

	double result = 1.0 /
		sqrt(2 * M_PI * sigma * sigma) *
		exp(-(x - center) * (x - center) /
			(2 * sigma * sigma));


	return result;
}



int ParametricEQ::process_buffer(int64_t size,
	Samples *buffer,
	int64_t start_position,
	int sample_rate)
{
	need_reconfigure |= load_configuration();
	if(need_reconfigure) reconfigure();


	fft->process_buffer(start_position,
		size,
		buffer,
		get_direction());



	return 0;
}

void ParametricEQ::reset()
{
	need_reconfigure = 1;
	thread = 0;
	fft = 0;
}

void ParametricEQ::update_gui()
{
	if( !thread ) return;
	ParametricWindow *window = (ParametricWindow*)thread->window;
	window->lock_window("ParametricEQ::update_gui");
	if( load_configuration() ) {
		calculate_envelope();
		window->update_gui();
	}
	else if( pending_gui_frame() ) {
		window->update_canvas();
	}
	window->unlock_window();
}

