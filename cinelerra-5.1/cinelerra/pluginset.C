
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

#include "edl.h"
#include "edlsession.h"
#include "filexml.h"
#include "keyframe.h"
#include "keyframes.h"
#include "plugin.h"
#include "pluginautos.h"
#include "pluginset.h"
#include "track.h"
#include "transportque.inc"

#include <string.h>

PluginSet::PluginSet(EDL *edl, Track *track)
 : Edits(edl, track)
{
	record = 1;
}

PluginSet::~PluginSet()
{
	while(last) delete last;
}


void PluginSet::copy_from(PluginSet *src)
{
	while(last) delete last;
	for(Plugin *current = (Plugin*)src->first; current; current = (Plugin*)NEXT)
	{
		Plugin *new_plugin;
		append(new_plugin = (Plugin*)create_edit());
		new_plugin->copy_from(current);
// update gui_id when copying edl
		new_plugin->gui_id = current->gui_id;
	}
	this->record = src->record;
}

Plugin* PluginSet::get_first_plugin()
{
// Called when a new pluginset is added.
// Get first non-silence plugin in the plugin set.
	for(Plugin *current = (Plugin*)first; current; current = (Plugin*)NEXT)
	{
		if(current && current->plugin_type != PLUGIN_NONE)
		{
			return current;
		}
	}
	return 0;
}

int64_t PluginSet::plugin_change_duration(int64_t input_position,
	int64_t input_length,
	int reverse)
{
	int result = input_length;
	Edit *current;

	if(reverse)
	{
		int input_start = input_position - input_length;
		for(current = last; current; current = PREVIOUS)
		{
			int start = current->startproject;
			int end = start + current->length;
			if(end > input_start && end < input_position)
			{
				result = input_position - end;
				return result;
			}
			else
			if(start > input_start && start < input_position)
			{
				result = input_position - start;
				return result;
			}
		}
	}
	else
	{
		int input_end = input_position + input_length;
		for(current = first; current; current = NEXT)
		{
			int start = current->startproject;
			int end = start + current->length;
			if(start > input_position && start < input_end)
			{
				result = start - input_position;
				return result;
			}
			else
			if(end > input_position && end < input_end)
			{
				result = end - input_position;
				return result;
			}
		}
	}
	return input_length;
}

void PluginSet::synchronize_params(PluginSet *plugin_set)
{
	for(Plugin *this_plugin = (Plugin*)first, *that_plugin = (Plugin*)plugin_set->first;
		this_plugin && that_plugin;
		this_plugin = (Plugin*)this_plugin->next, that_plugin = (Plugin*)that_plugin->next)
	{
		this_plugin->synchronize_params(that_plugin);
	}
}

Plugin* PluginSet::insert_plugin(const char *title,
	int64_t unit_position, int64_t unit_length, int plugin_type,
	SharedLocation *shared_location, KeyFrame *default_keyframe,
	int do_optimize)
{
	Plugin *plugin = (Plugin*)create_silence(unit_position, unit_position + unit_length);
	plugin->init(title, unit_position, unit_length, plugin_type,
			shared_location, default_keyframe);
// May delete the plugin we just added so not desirable while loading.
	if( do_optimize ) optimize();
	return plugin;
}

Edit* PluginSet::create_edit()
{
	Plugin* result = new Plugin(edl, this, "");
	return result;
}

Edit* PluginSet::insert_edit_after(Edit *previous_edit)
{
	Plugin *current = new Plugin(edl, this, "");
	List<Edit>::insert_after(previous_edit, current);
	return (Edit*)current;
}

KeyFrame *PluginSet::nearest_keyframe(int64_t pos, int dir)
{
	if( first && pos < first->startproject )
		pos = first->startproject;
	else if( last && pos > last->startproject+last->length )
		pos = last->startproject+last->length;
	KeyFrame *keyframe = 0;
	Plugin *plugin = (Plugin*)editof(pos, dir, 0);
	if( dir == PLAY_FORWARD ) {
		for( ; !keyframe && plugin; plugin=(Plugin*)plugin->next ) {
			if( plugin->plugin_type == PLUGIN_NONE ) continue;
			keyframe = (KeyFrame *)plugin->keyframes->nearest_after(pos);
		}
	}
	else {
		for( ; !keyframe && plugin; plugin=(Plugin*)plugin->previous ) {
			if( plugin->plugin_type == PLUGIN_NONE ) continue;
			keyframe = (KeyFrame *)plugin->keyframes->nearest_before(pos);
		}
	}
	return keyframe;
}

int PluginSet::get_number()
{
	return track->plugin_set.number_of(this);
}

void PluginSet::clear(int64_t start, int64_t end, int edit_autos)
{
	if(edit_autos)
	{
// Clear keyframes
		for(Plugin *current = (Plugin*)first;
			current;
			current = (Plugin*)NEXT)
		{
			current->keyframes->clear(start, end, 1);
		}
	}

// Clear edits
	Edits::clear(start, end);
}

//void PluginSet::clear_recursive(int64_t start, int64_t end)
//{
//printf("PluginSet::clear_recursive 1\n");
//	clear(start, end, 1);
//}

void PluginSet::shift_keyframes_recursive(int64_t position, int64_t length)
{
// Plugin keyframes are shifted in shift_effects
}

void PluginSet::shift_effects_recursive(int64_t position, int64_t length, int edit_autos)
{
// Effects are shifted in length extension
}


void PluginSet::clear_keyframes(int64_t start, int64_t end)
{
	for(Plugin *current = (Plugin*)first; current; current = (Plugin*)NEXT)
	{
		current->clear_keyframes(start, end);
	}
}

void PluginSet::copy_keyframes(int64_t start,
	int64_t end,
	FileXML *file,
	int default_only,
	int active_only)
{
	file->tag.set_title("PLUGINSET");
	file->append_tag();
	file->append_newline();

	for(Plugin *current = (Plugin*)first;
		current;
		current = (Plugin*)NEXT)
	{
		current->copy_keyframes(start, end, file, default_only, active_only);
	}

	file->tag.set_title("/PLUGINSET");
	file->append_tag();
	file->append_newline();
}


void PluginSet::paste_keyframes(int64_t start,
	int64_t length,
	FileXML *file,
	int default_only,
	int active_only)
{
	int result = 0;
	int first_keyframe = 1;
	Plugin *current;


	while(!result)
	{
		result = file->read_tag();

		if(!result)
		{
			if(file->tag.title_is("/PLUGINSET"))
				result = 1;
			else
			if(file->tag.title_is("KEYFRAME"))
			{
				int64_t position = file->tag.get_property("POSITION", 0);
				if(first_keyframe && default_only)
				{
					position = start;
				}
				else
				{
					position += start;
				}

// Get plugin owning keyframe
				for(current = (Plugin*)last;
					current;
					current = (Plugin*)PREVIOUS)
				{
// We want keyframes to exist beyond the end of the last plugin to
// make editing intuitive, but it will always be possible to
// paste keyframes from one plugin into an incompatible plugin.
					if(position >= current->startproject)
					{
						KeyFrame *keyframe = 0;
						if(file->tag.get_property("DEFAULT", 0) || default_only)
						{
							keyframe = (KeyFrame*)current->keyframes->default_auto;
						}
						else
						if(!default_only)
						{
							keyframe =
								(KeyFrame*)current->keyframes->insert_auto(position);
						}

						if(keyframe)
						{
							keyframe->load(file);
							keyframe->position = position;
						}
						break;
					}
				}

				first_keyframe = 0;
			}
		}
	}
}

void PluginSet::shift_effects(int64_t start, int64_t length, int edit_autos)
{
	for(Plugin *current = (Plugin*)first;
		current;
		current = (Plugin*)NEXT)
	{
// Shift beginning of this effect
		if(current->startproject >= start)
		{
			current->startproject += length;
		}
		else
// Extend end of this effect.
// In loading new files, the effect should extend to fill the entire track.
// In muting, the effect must extend to fill the gap if another effect follows.
// The user should use Settings->edit effects to disable this.
		if(current->startproject + current->length >= start)
		{
			current->length += length;
		}

// Shift keyframes in this effect.
// If the default keyframe lands on the starting point, it must be shifted
// since the effect start is shifted.
		if(edit_autos && current->keyframes->default_auto->position >= start)
			current->keyframes->default_auto->position += length;

		if(edit_autos) current->keyframes->paste_silence(start, start + length);
	}
}

void PluginSet::paste_silence(int64_t start, int64_t end, int edit_autos)
{
	Plugin *new_plugin = (Plugin *) insert_new_edit(start);
	int64_t length = end - start;
	new_plugin->length = length;
	while( (new_plugin=(Plugin *)new_plugin->next) != 0 ) {
		new_plugin->startproject += length;
		if( !edit_autos ) continue;
		new_plugin->keyframes->default_auto->position += length;
		new_plugin->keyframes->paste_silence(start, end);
	}
}

void PluginSet::copy(int64_t start, int64_t end, FileXML *file)
{
	file->tag.set_title("PLUGINSET");
	file->tag.set_property("RECORD", record);
	file->append_tag();
	file->append_newline();

	for(Plugin *current = (Plugin*)first; current; current = (Plugin*)NEXT)
	{
		current->copy(start, end, file);
	}

	file->tag.set_title("/PLUGINSET");
	file->append_tag();
	file->append_newline();
}

void PluginSet::save(FileXML *file)
{
	copy(0, length(), file);
}

void PluginSet::load(FileXML *file, uint32_t load_flags)
{
	int result = 0;
// Current plugin being amended
	Plugin *plugin = (Plugin*)first;
	int64_t startproject = 0;

	record = file->tag.get_property("RECORD", record);
	do{
		result = file->read_tag();


		if(!result)
		{
			if(file->tag.title_is("/PLUGINSET"))
			{
				result = 1;
			}
			else
			if(file->tag.title_is("PLUGIN"))
			{
				int64_t length = file->tag.get_property("LENGTH", (int64_t)0);
				int plugin_type = file->tag.get_property("TYPE", 1);
				char title[BCTEXTLEN];
				title[0] = 0;
				file->tag.get_property("TITLE", title);
				Plugin::fix_plugin_title(title);
				SharedLocation shared_location;
				shared_location.load(file);


				if(load_flags & LOAD_EDITS)
				{
					plugin = insert_plugin(title,
						startproject,
						length,
						plugin_type,
						&shared_location,
						0,
						0);
					plugin->load(file);
					startproject += length;
				}
				else
				if(load_flags & LOAD_AUTOMATION)
				{
					if(plugin)
					{
						plugin->load(file);
						plugin = (Plugin*)plugin->next;
					}
				}
			}
		}
	} while(!result);

	optimize();
}

int PluginSet::optimize()
{
	int result = 1;
	Plugin *current_edit;

// trim plugins before position 0
	while( first && first->startproject+first->length < 0 )
		delete first;
	if( first && first->startproject < 0 ) {
		first->length += first->startproject;
		first->startproject = 0;
	}

// Delete keyframes out of range
	for(current_edit = (Plugin*)first;
		current_edit;
		current_edit = (Plugin*)current_edit->next)
	{
		current_edit->keyframes->default_auto->position = 0;
		for(KeyFrame *current_keyframe = (KeyFrame*)current_edit->keyframes->last;
			current_keyframe; )
		{
			KeyFrame *previous_keyframe = (KeyFrame*)current_keyframe->previous;
			if(current_keyframe->position >
				current_edit->startproject + current_edit->length ||
				current_keyframe->position < current_edit->startproject)
			{
				delete current_keyframe;
			}
			current_keyframe = previous_keyframe;
		}
	}

// Insert silence between plugins
	for(Plugin *current = (Plugin*)last; current; current = (Plugin*)PREVIOUS)
	{
		if(current->previous)
		{
			Plugin *previous = (Plugin*)PREVIOUS;

			if(current->startproject -
				previous->startproject -
				previous->length > 0)
			{
				Plugin *new_plugin = (Plugin*)create_edit();
				insert_before(current, new_plugin);
				new_plugin->startproject = previous->startproject +
					previous->length;
				new_plugin->length = current->startproject -
					previous->startproject -
					previous->length;
			}
		}
		else
		if(current->startproject > 0)
		{
			Plugin *new_plugin = (Plugin*)create_edit();
			insert_before(current, new_plugin);
			new_plugin->length = current->startproject;
		}
	}


// delete 0 length plugins
	while(result)
	{
		result = 0;

		for(current_edit = (Plugin*)first; !result && current_edit; ) {
			Plugin* next = (Plugin*)current_edit->next;
			if(current_edit->length == 0) {
				delete current_edit;
				result = 1;
			}
			current_edit = next;
		}


// merge identical plugins with same keyframes
		for( current_edit = (Plugin*)first;
			!result && current_edit && current_edit->next; ) {
			Plugin *next_edit = (Plugin*)current_edit->next;

// plugins identical
			if(next_edit->identical(current_edit)) {
				current_edit->length += next_edit->length;
// Merge keyframes
				for(KeyFrame *source = (KeyFrame*)next_edit->keyframes->first;
					source;
					source = (KeyFrame*)source->next) {
					KeyFrame *dest = new KeyFrame(edl, current_edit->keyframes);
					dest->copy_from(source);
					current_edit->keyframes->append(dest);
				}
				remove(next_edit);
				result = 1;
				continue;
			}

			current_edit = next_edit;

// delete last edit if 0 length or silence
			if( last && (last->silence() || !last->length) ) {
				delete last;
				result = 1;
			}
		}
	}

	return 0;
}





void PluginSet::dump(FILE *fp)
{
	fprintf(fp,"   PLUGIN_SET:\n");
	for(Plugin *current = (Plugin*)first; current; current =  (Plugin*)NEXT)
		current->dump(fp);
}
