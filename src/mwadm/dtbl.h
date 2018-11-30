/*
  MidWay
  Copyright (C) 2003 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU  General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  MidWay is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License 
  along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/


#ifndef _DTABLE_H
#define _DTABLE_H
typedef struct dtbl * DTable;

/* this represent a generic way for commands.c to return data. Now,
   every command in commands.c must have a prototype that allow for
   the correct number od tables, and side info. Hence the caller, must
   have a basic knowledge of the data returned.  This is done so that
   commands.c can be called from a varaiety of frontends. We're just
   doing text for now, but this is a crucial step towards a X mwadm.

*/

enum { DTBL_STYLE_EMPTY = 0, DTBL_STYLE_LINES, DTBL_STYLE_SIDE };
enum { DTBL_ALIGN_LEFT = 0, DTBL_ALIGN_CENTER, DTBL_ALIGN_RIGHT };

#endif // _DTABLE_H

void dtbl_init(DTable dt);
DTable dtbl_new(int columns);
void dtbl_delete(DTable);

void dtbl_headers(DTable dt, ...) ;
void dtbl_newrow(DTable dt, ...) ;

void dtbl_setfield(DTable dt, int row, int col, char * fmt, ...);

void dtbl_style(DTable dt, int style);
int dtbl_col_format(DTable dt, int col, int align, int fixwidth);

void dtbl_title(DTable dt, char * title, ...);
void dtbl_subtext(DTable dt, char * text, ...);

// this is spesific to the text frontend, and shall only be called from mwadm.c
void dtbl_printf(DTable dt, FILE * fp) ;

