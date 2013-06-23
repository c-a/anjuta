/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    breakpoints.h
    Copyright (C) 2007 Sébastien Granjoux

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _BREAKPOINTS_DBASE_H_
#define _BREAKPOINTS_DBASE_H_

#include "plugin.h"

#include <stdio.h>
#include <libanjuta/anjuta-plugin.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-debugger.h>

/* Stock icons */
#define ANJUTA_STOCK_BREAKPOINT_TOGGLE			"gdb-breakpoint-toggle"
#define ANJUTA_STOCK_BREAKPOINT_CLEAR			"anjuta-breakpoint-clear"
#define ANJUTA_STOCK_BREAKPOINT_DISABLED		"gdb-breakpoint-disabled"
#define ANJUTA_STOCK_BREAKPOINT_ENABLED			"gdb-breakpoint-enabled"

G_BEGIN_DECLS

typedef struct _BreakpointsDBase BreakpointsDBase;

BreakpointsDBase *breakpoints_dbase_new (DebugManagerPlugin *plugin);
void breakpoints_dbase_destroy (BreakpointsDBase * bd);

/* Handler for breakpoint toggle on double clicking line marks gutter */
void breakpoint_toggle_handler(GtkAction * action, gint line_number, BreakpointsDBase *bd);

void breakpoints_dbase_save_session (BreakpointsDBase *bd, AnjutaSession *session);
void breakpoints_dbase_load_session (BreakpointsDBase *bd, AnjutaSession *session);

G_END_DECLS
											
#endif
