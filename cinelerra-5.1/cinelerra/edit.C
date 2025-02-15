
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

#include "asset.h"
#include "assets.h"
#include "bcsignals.h"
#include "clip.h"
#include "edit.h"
#include "edits.h"
#include "edl.h"
#include "edlsession.h"
#include "filexml.h"
#include "filesystem.h"
#include "labels.h"
#include "localsession.h"
#include "plugin.h"
#include "mainsession.h"
#include "strack.h"
#include "trackcanvas.h"
#include "tracks.h"
#include "transition.h"
#include <string.h>


Edit::Edit()
{
	reset();
}

Edit::Edit(EDL *edl, Track *track)
{
	reset();
	this->edl = edl;
	this->track = track;
	if(track) this->edits = track->edits;
	id = EDL::next_id();
	orig_id = id;
}

Edit::Edit(EDL *edl, Edits *edits)
{
	reset();
	this->edl = edl;
	this->edits = edits;
	if(edits) this->track = edits->track;
	id = EDL::next_id();
	orig_id = id;
}

Edit::~Edit()
{
//printf("Edit::~Edit 1\n");
	if(transition) delete transition;
//printf("Edit::~Edit 2\n");
}

void Edit::reset()
{
	edl = 0;
	track = 0;
	edits = 0;
	startsource = 0;
	startproject = 0;
	length = 0;
	asset = 0;
	transition = 0;
	channel = 0;
	user_title[0] = 0;
	nested_edl = 0;
	is_selected = 0;
	hard_left = 0;
	hard_right = 0;
	color = 0;
	group_id = 0;
}

Indexable* Edit::get_source()
{
	if(asset) return asset;
	if(nested_edl) return nested_edl;
	return 0;
}

int Edit::copy(int64_t start,
	int64_t end,
	FileXML *file,
	const char *output_path)
{
// variables
//printf("Edit::copy 1\n");

	int64_t endproject = startproject + length;
	int result;

	if((startproject >= start && startproject <= end) ||  // startproject in range
		 (endproject <= end && endproject >= start) ||	   // endproject in range
		 (startproject <= start && endproject >= end))    // range in project
	{
// edit is in range
		int64_t startproject_in_selection = startproject; // start of edit in selection in project
		int64_t startsource_in_selection = startsource; // start of source in selection in source
		//int64_t endsource_in_selection = startsource + length; // end of source in selection
		int64_t length_in_selection = length;             // length of edit in selection
//printf("Edit::copy 2\n");

		if(startproject < start)
		{         // start is after start of edit in project
			int64_t length_difference = start - startproject;

			startsource_in_selection += length_difference;
			startproject_in_selection += length_difference;
			length_in_selection -= length_difference;
		}
//printf("Edit::copy 3\n");

		if(endproject > end)
		{         // end is before end of edit in project
			length_in_selection = end - startproject_in_selection;
		}

//printf("Edit::copy 4\n");
		if(file)    // only if not counting
		{
			file->tag.set_title("EDIT");
			file->tag.set_property("STARTSOURCE", startsource_in_selection);
			file->tag.set_property("CHANNEL", (int64_t)channel);
			file->tag.set_property("LENGTH", length_in_selection);
			file->tag.set_property("HARD_LEFT", hard_left);
			file->tag.set_property("HARD_RIGHT", hard_right);
			file->tag.set_property("COLOR", color);
			file->tag.set_property("GROUP_ID", group_id);
			if(user_title[0]) file->tag.set_property("USER_TITLE", user_title);
//printf("Edit::copy 5\n");

			copy_properties_derived(file, length_in_selection);

			file->append_tag();
//			file->append_newline();
//printf("Edit::copy 6\n");

			if(nested_edl)
			{
				file->tag.set_title("NESTED_EDL");
				file->tag.set_property("SRC", nested_edl->path);
				file->append_tag();
				file->tag.set_title("/NESTED_EDL");
				file->append_tag();
				file->append_newline();
			}

			if(asset)
			{
//printf("Edit::copy 6 %s\n", asset->path);
				char stored_path[BCTEXTLEN];
				char asset_directory[BCTEXTLEN];
				char output_directory[BCTEXTLEN];
				FileSystem fs;

//printf("Edit::copy %d %s\n", __LINE__, asset->path);
				fs.extract_dir(asset_directory, asset->path);
//printf("Edit::copy %d %s\n", __LINE__, asset->path);

				if(output_path)
					fs.extract_dir(output_directory, output_path);
				else
					output_directory[0] = 0;
//printf("Edit::copy %s, %s %s, %s\n", asset->path, asset_directory, output_path, output_directory);

				if(output_path && !strcmp(asset_directory, output_directory))
					fs.extract_name(stored_path, asset->path);
				else
					strcpy(stored_path, asset->path);

				file->tag.set_title("FILE");
				file->tag.set_property("SRC", stored_path);
				file->append_tag();
				file->tag.set_title("/FILE");
				file->append_tag();
			}

			if(transition && startsource_in_selection == startsource)
			{
				transition->save_xml(file);
			}

//printf("Edit::copy 7\n");
			file->tag.set_title("/EDIT");
			file->append_tag();
			file->append_newline();
//printf("Edit::copy 8\n");
		}
//printf("Edit::copy 9\n");
		result = 1;
	}
	else
	{
		result = 0;
	}
//printf("Edit::copy 10\n");
	return result;
}


int64_t Edit::get_source_end(int64_t default_)
{
	return default_;
}

void Edit::insert_transition(char *title)
{
//printf("Edit::insert_transition this=%p title=%p title=%s\n", this, title, title);
	delete transition;
	transition = new Transition(edl, this, title,
		track->to_units(edl->session->default_transition_length, 1));
}

void Edit::detach_transition()
{
	delete transition;
	transition = 0;
	if( edl->local_session->gang_tracks == GANG_NONE ) return;
	double pos = track->from_units(startproject);
	Track *current = edl->tracks->first;
	for( ; current; current=current->next ) {
		if( current == track ) continue;
		if( current->data_type != track->data_type ) continue;
		if( !current->armed_gang(track) ) continue;
		int64_t track_pos = current->to_units(pos, 1);
		Edit *edit = current->edits->editof(track_pos, PLAY_FORWARD, 0);
		if( !edit ) continue;
		double edit_pos = track->from_units(edit->startproject);
		if( !edl->equivalent(pos, edit_pos) ) continue;
		delete edit->transition;
		edit->transition = 0;
	}
}

int Edit::silence()
{
	return (track->data_type != TRACK_SUBTITLE ?
		asset || nested_edl :
		*((SEdit *)this)->get_text()) ? 0 : 1;
}

void Edit::set_selected(int v)
{
	if( !group_id ) {
		if( v < 0 ) v = !is_selected ? 1 : 0;
		int gang = edl->local_session->gang_tracks != GANG_NONE ? 1 : 0;
		select_affected_edits(v, gang);
	}
	else
		edl->tracks->set_group_selected(group_id, v);
}

// gang<0: rest of tracks, gang==0: this track, gang>0: to next master
void Edit::select_affected_edits(int v, int gang)
{
	is_selected = v;
	if( !gang ) return;
	double position = track->from_units(startproject);
	for( Track *current=track->next; current; current=current->next ) {
		if( gang > 0 && current->master ) break;
		if( !current->is_armed() ) continue;
		for( Edit *edit=current->edits->first; edit; edit=edit->next ) {
			if( edit->silence() ) continue;
			double start = current->from_units(edit->startproject);
			if( edl->equivalent(start, position) ) {
				edit->is_selected = v;
				break;
			}
		}
	}
}


void Edit::copy_from(Edit *edit)
{
	this->orig_id = edit->orig_id;
	this->nested_edl = edl->nested_edls.get_nested(edit->nested_edl);
	this->asset = edl->assets->update(edit->asset);
	this->startsource = edit->startsource;
	this->startproject = edit->startproject;
	this->length = edit->length;
	this->hard_left = edit->hard_left;
	this->hard_right = edit->hard_right;
	this->color = edit->color;
	this->group_id = edit->group_id;
	strcpy (this->user_title, edit->user_title);

	if(edit->transition)
	{
		if(!transition) transition = new Transition(edl,
			this,
			edit->transition->title,
			edit->transition->length);
		*this->transition = *edit->transition;
	}
	this->channel = edit->channel;
}

void Edit::clone_from(Edit *edit)
{
	copy_from(edit);
	edit->orig_id = edit->id;
}

void Edit::equivalent_output(Edit *edit, int64_t *result)
{
// End of edit changed
	if(startproject + length != edit->startproject + edit->length)
	{
		int64_t new_length = MIN(startproject + length,
			edit->startproject + edit->length);
		if(*result < 0 || new_length < *result)
			*result = new_length;
	}

	if(
// Different nested EDLs
		(edit->nested_edl && !nested_edl) ||
		(!edit->nested_edl && nested_edl) ||
// Different assets
		(edit->asset == 0 && asset != 0) ||
		(edit->asset != 0 && asset == 0) ||
// different transitions
		(edit->transition == 0 && transition != 0) ||
		(edit->transition != 0 && transition == 0) ||
// Position changed
		(startproject != edit->startproject) ||
		(startsource != edit->startsource) ||
// Transition changed
		(transition && edit->transition &&
			!transition->identical(edit->transition)) ||
// Asset changed
		(asset && edit->asset &&
			!asset->equivalent(*edit->asset, 1, 1, edl)) ||
// Nested EDL changed
		(nested_edl && edit->nested_edl &&
			strcmp(nested_edl->path, edit->nested_edl->path))
		)
	{
// Start of edit changed
		if(*result < 0 || startproject < *result) *result = startproject;
	}
}


Edit& Edit::operator=(Edit& edit)
{
//printf("Edit::operator= called\n");
	copy_from(&edit);
	return *this;
}

void Edit::synchronize_params(Edit *edit)
{
	copy_from(edit);
}


// Comparison for ResourcePixmap drawing
int Edit::identical(Edit &edit)
{
	int result = (this->nested_edl == edit.nested_edl &&
		this->asset == edit.asset &&
		this->startsource == edit.startsource &&
		this->startproject == edit.startproject &&
		this->length == edit.length &&
		this->transition == edit.transition &&
		this->channel == edit.channel);
	return result;
}

int Edit::operator==(Edit &edit)
{
	return identical(edit);
}

double Edit::frames_per_picon()
{
	return Units::round(picon_w()) / frame_w();
}

double Edit::frame_w()
{
	return track->from_units(1) *
		edl->session->sample_rate /
		edl->local_session->zoom_sample;
}

double Edit::picon_w()
{
	int w = 0, h = 0;
	if(asset) {
		w = asset->width;
		h = asset->height;
	}
	else if(nested_edl) {
		w = nested_edl->session->output_w;
		h = nested_edl->session->output_h;
	}
	return w>0 && h>0 ? ((double)track->data_h*w)/h : 0;
}

int Edit::picon_h()
{
	return track->data_h;
}


int Edit::dump(FILE *fp)
{
	fprintf(fp,"     EDIT %p\n", this); fflush(fp);
	fprintf(fp,"      id %d, orig_id %d, nested_edl=%p %s asset=%p %s\n",
		id, orig_id, nested_edl, nested_edl ? nested_edl->path : "",
		asset, asset ? asset->path : "");
	fflush(fp);
	fprintf(fp,"      channel %d, color %08x, hard lt/rt %d/%d"
		" group_id %d, is_selected %d\n",
		channel, color, hard_left, hard_right, group_id, is_selected);
	if( transition ) {
		fprintf(fp,"      TRANSITION %p\n", transition);
		transition->dump(fp);
	}
	fprintf(fp,"      startsource %jd startproject %jd length %jd\n",
		startsource, startproject, length);
	fflush(fp);
	return 0;
}

int Edit::load_properties(FileXML *file, int64_t &startproject)
{
	startsource = file->tag.get_property("STARTSOURCE", (int64_t)0);
	length = file->tag.get_property("LENGTH", (int64_t)0);
	hard_left = file->tag.get_property("HARD_LEFT", (int64_t)0);
	hard_right = file->tag.get_property("HARD_RIGHT", (int64_t)0);
	color = file->tag.get_property("COLOR", 0);
	group_id = file->tag.get_property("GROUP_ID", group_id);
	user_title[0] = 0;
	file->tag.get_property("USER_TITLE", user_title);
	this->startproject = startproject;
	load_properties_derived(file);
	return 0;
}

void Edit::shift(int64_t difference)
{
	startproject += difference;
}

void Edit::trim(int64_t difference)
{
	length += difference;
	if( startproject < 0 ) {
		if( (startsource+=startproject) < 0 ) startsource = 0;
		if( (length+=startproject) < 0 ) length = 0;
		startproject = 0;
	}
	if( startsource < 0 )
		startsource = 0;
	int64_t src_len = get_source_end(INT64_MAX);
	if( startsource + length > src_len ) {
		length = src_len - startsource;
		if( length < 0 ) length = 0;
	}
	if( length < 0 )
		length = 0;
}

int Edit::shift_start(int edit_mode, int64_t newposition, int64_t oldposition,
	int edit_labels, int edit_autos, int edit_plugins, Edits *trim_edits)
{
	int64_t cut_length = newposition - oldposition;
	int rest_moved = edit_mode == MOVE_RIPPLE || edit_mode == MOVE_EDGE ? 1 : 0;
	if( cut_length > length )
		cut_length = length;
	else if( cut_length < -length )
		cut_length = -length;

	int64_t start = startproject, end = start + length;
	Edit *prev = this->previous, *next = this->next;
	int edits_moved = 0;

	switch( edit_mode ) {
	case MOVE_RIPPLE:
		edits_moved = 1;
		startsource += cut_length;
		cut_length = -cut_length;
		length += cut_length;
		for( Edit *edit=next; edit; edit=edit->next )
			edit->startproject += cut_length;
		break;
	case MOVE_ROLL:
		if( prev && prev->length + cut_length < 0 )
			cut_length = -prev->length;
		if( prev ) prev->trim(cut_length);
		startproject += cut_length;
		startsource += cut_length;
		length -= cut_length;
		break;
	case MOVE_SLIP:
		edits_moved = 1;
		startsource -= cut_length;
		break;
	case MOVE_SLIDE:
		edits_moved = 1;
		if( prev && prev->length + cut_length < 0 )
			cut_length = -prev->length;
		if( next && next->length - cut_length < 0 )
			cut_length = next->length;
		if( prev ) prev->trim(cut_length);
		startproject += cut_length;
		if( next ) {
			next->startproject += cut_length;
			next->startsource += cut_length;
			next->trim(-cut_length);
		}
		break;
	case MOVE_EDGE:
		edits_moved = 1;
		startsource -= cut_length;
		length += cut_length;
		for( Edit *edit=next; edit; edit=edit->next )
			edit->startproject += cut_length;
		break;
	}
	trim(0);
	return follow_edits(start, end, cut_length, edits_moved, rest_moved,
		edit_labels, edit_autos, edit_plugins, trim_edits);
}

int Edit::shift_end(int edit_mode, int64_t newposition, int64_t oldposition,
	int edit_labels, int edit_autos, int edit_plugins, Edits *trim_edits)
{
	int64_t cut_length = newposition - oldposition;
	int rest_moved = edit_mode == MOVE_RIPPLE || edit_mode == MOVE_EDGE ? 1 : 0;
	if( cut_length > length ) {
		 if( !rest_moved ) cut_length = length;
	}
	else if( cut_length < -length )
		cut_length = -length;
	int64_t start = startproject, end = start + length;
	Edit *prev = this->previous, *next = this->next;
	int edits_moved = 0;

	switch( edit_mode ) {
	case MOVE_RIPPLE:
	case MOVE_EDGE:
		length += cut_length;
		for( Edit *edit=next; edit; edit=edit->next )
			edit->startproject += cut_length;
		break;
	case MOVE_ROLL:
		if( next && next->length - cut_length < 0 )
			cut_length = next->length;
		length += cut_length;
		if( next ) {
			next->startproject += cut_length;
			next->startsource += cut_length;
			next->trim(-cut_length);
		}
		break;
	case MOVE_SLIP:
		edits_moved = 1;
		startsource -= cut_length;
		break;
	case MOVE_SLIDE:
		edits_moved = 1;
		if( prev && prev->length + cut_length < 0 )
			cut_length = -prev->length;
		if( next && next->length - cut_length < 0 )
			cut_length = next->length;
		if( prev ) prev->trim(cut_length);
		startproject += cut_length;
		if( next ) {
			next->startproject += cut_length;
			next->startsource += cut_length;
			next->trim(-cut_length);
		}
		break;
	}
	trim(0);
	return follow_edits(start, end, cut_length, edits_moved, rest_moved,
		edit_labels, edit_autos, edit_plugins, trim_edits);
}

int Edit::follow_edits(int64_t start, int64_t end, int64_t cut_length,
		int edits_moved, int rest_moved, int edit_labels, int edit_autos,
		int edit_plugins, Edits *trim_edits)
{
	if( edits_moved && rest_moved ) {
		if( edit_labels ) {
			double cut_len = track->from_units(cut_length);
			double start_pos = edits->track->from_units(start);
			if( cut_len < 0 )
				edits->edl->labels->clear(start_pos + cut_len, start_pos, 1);
			else if( cut_len > 0 )
				edits->edl->labels->insert(start_pos, cut_len);
		}
		if( cut_length < 0 )
			track->clear(start + cut_length, start,
				0, 0, edit_autos, edit_plugins, trim_edits);
		else if( cut_length > 0 ) {
			if( edit_autos )
				track->shift_keyframes(start, cut_length);
			if( edit_plugins )
				track->shift_effects(start, cut_length, 1, trim_edits);
		}
	}
	else if( edits_moved ) {
		if( edit_labels ) {
			double cut_len = track->from_units(cut_length);
			double start_pos = edits->track->from_units(start);
			edits->edl->labels->insert(start_pos, cut_len);
			double end_pos = edits->track->from_units(end);
			edits->edl->labels->insert(end_pos + cut_len, -cut_len);
		}
		if( edit_autos ) {
			if( cut_length > 0 ) {
				track->clear(end, end+cut_length, 0, 0, 0, 1, 0);
				track->shift_keyframes(start, cut_length);
			}
			else if( cut_length < 0 ) {
				track->clear(start+cut_length, start, 0, 0, 0, 1, 0);
				track->shift_keyframes(end+cut_length, -cut_length);
			}
		}
		if( edit_plugins ) {
			if( cut_length > 0 ) {
				track->clear(end, end+cut_length, 0, 0, -1, 0, 0);
				track->shift_effects(start, cut_length, 1, 0);
			}
			else if( cut_length < 0 ) {
				track->clear(start+cut_length, start, 0, 0, -1, 0, 0);
				track->shift_effects(end+cut_length, -cut_length, 1, 0);
			}
		}
	}
	else if( rest_moved ) {
		if( edit_labels ) {
			double cut_len = track->from_units(cut_length);
			double end_pos = edits->track->from_units(end);
			if( cut_len < 0 )
				edits->edl->labels->clear(end_pos + cut_len, end_pos, 1);
			else if( cut_len > 0 )
				edits->edl->labels->insert(end_pos, cut_len);
		}
		if( cut_length < 0 )
			track->clear(end + cut_length, end,
				0, 0, edit_autos, edit_plugins, trim_edits);
		else if( cut_length > 0 ) {
			if( edit_autos )
				track->shift_keyframes(end, cut_length);
			if( edit_plugins )
				track->shift_effects(end, cut_length, 1, trim_edits);
		}
	}
	return 0;
}



int Edit::popup_transition(float view_start, float zoom_units, int cursor_x, int cursor_y)
{
	int64_t left, right, left_unit, right_unit;
	if(!transition) return 0;
	get_handle_parameters(left, right, left_unit, right_unit, view_start, zoom_units);

	if(cursor_x > left && cursor_x < right)
	{
//		transition->popup_transition(cursor_x, cursor_y);
		return 1;
	}
	return 0;
}

int Edit::select_handle(float view_start, float zoom_units, int cursor_x, int cursor_y, int64_t &selection)
{
	int64_t left, right, left_unit, right_unit;
	get_handle_parameters(left, right, left_unit, right_unit, view_start, zoom_units);

	int64_t pixel1, pixel2;
	pixel1 = left;
	pixel2 = pixel1 + xS(10);

// test left edit
// cursor_x is faked in acanvas
	if(cursor_x >= pixel1 && cursor_x <= pixel2)
	{
		selection = left_unit;
		return 1;     // left handle
	}

	//int64_t endproject = startproject + length;
	pixel2 = right;
	pixel1 = pixel2 - xS(10);

// test right edit
	if(cursor_x >= pixel1 && cursor_x <= pixel2)
	{
		selection = right_unit;
		return 2;     // right handle
	}
	return 0;
}

void Edit::get_title(char *title)
{
	if( user_title[0] ) {
		strcpy(title, user_title);
		return;
	}
	Indexable *idxbl = asset ? (Indexable*)asset : (Indexable*)nested_edl;
	if( !idxbl ) {
		title[0] = 0;
		return;
	}
	FileSystem fs;
	fs.extract_name(title, idxbl->path);
	if( asset || track->data_type == TRACK_AUDIO ) {
		char number[BCSTRLEN];
		sprintf(number, " #%d", channel + 1);
		strcat(title, number);
	}
}

