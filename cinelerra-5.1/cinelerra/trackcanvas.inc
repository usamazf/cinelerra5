
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

#ifndef TRACKCANVAS_INC
#define TRACKCANVAS_INC

class TrackCanvas;

#define HANDLE_W xS(8)
#define HANDLE_H yS(8)

// draw mode:
// NO_DRAW used in MWindowGUI/TimelinePane update to disable canvas update
// NORMAL_DRAW causes incremental drawing of pixmaps.  Used for navigation and index refresh.
// FORCE_REDRAW causes all resource pixmaps to be redrawn from scratch.  Used by editing.
// IGNORE_THREAD causes resource pixmaps to ignore picon thread.  Used by Piconthread.
#define NO_DRAW 0
#define NORMAL_DRAW 1
#define FORCE_REDRAW 2
#define IGNORE_THREAD 3

#endif
