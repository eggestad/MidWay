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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.1  2003/07/06 18:59:56  eggestad
 * introduced a table api for commands.c to return data in
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef TEST
#include <MidWay.h>
#endif

#include "dtbl.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */


#ifdef MWLOG_DEBUG3

#define EXTDEBUG
#define ENTER() 
#define LEAVE() 

#else 
static int debugflag = 0;

#define  DEBUG3(m...) do { if (!debugflag) break; printf ("%.*sDEBUG: %s:%d: ", calllevel, spcpad, __FUNCTION__, __LINE__); \
printf(m); printf ("\n"); } while(0)

#define  Fatal(m...) do { printf ("** Fatal:%s:%d: ", __FUNCTION__, __LINE__); \
printf(m); printf ("\n"); abort();} while(0)

static int calllevel = 0;
#define ENTER() do { if (!debugflag) break; printf ("%.*s ENTER :%s\n", ++calllevel, enterstr, __FUNCTION__);} while(0)
#define LEAVE() do { if (!debugflag) break; printf ("%.*s LEAVE :%s\n", calllevel--, leavestr, __FUNCTION__);} while(0)

static char * enterstr = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
static char * leavestr = "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

#endif


#if 1
static char * spcpad = "                                                                                ";
#else
static char * spcpad = "________________________________________________________________________________";
#endif

struct field {
   int len;
   char * context;
   char align;
};

static void clear_field(struct field * f)
{
   if (!f) return;
   f->len = 0;
   f->context = NULL;
   return;
};

struct _Row {
   struct field * fields;
};

typedef struct _Row Row;

struct dtbl {
   int rows;
   int cols;

   int * width;
   int * fixwidth;
   
   int indent; // left of table, not included in min/maxwidth
   //   int minwidth;
   //   int maxwidth;

   // the string that make up the table "lines"
   // for width calcs these are all assumed strlen() = 1
   char fL;  // outer left 
   char fV;  // vertical inside table 
   char fR;  // outer right
   char fT;  // Outer top
   char fH;  // horisontal below headers
   char fB;  // outer bottom
   char fC;  // corner/joins
   int style;
   int fieldpad;
   
   int * align;

   char * title, * subtext;
   
   Row * cntx;
} ;

typedef struct dtbl dtbl;

void dtbl_init(dtbl * dt)
{
   if (dt == NULL) return;

   dt->rows = 0;
   dt->cols = 0;

   dt->indent = 0;
   //  dt->maxwidth = -1;
   //dt->minwidth = -1;

   dt->fL = ' ';
   dt->fV = ' ';
   dt->fR = ' ';
   dt->fT = ' ';
   dt->fH = ' ';
   dt->fB = ' ';
   dt->fC = ' ';

   dt->title = NULL;
   dt->subtext = NULL;

   dt->style = DTBL_STYLE_EMPTY;
   dt->fieldpad = 0;

   dt->cntx = NULL;

   dt->fixwidth = NULL;
   dt->align = NULL;
   return;
};

static int inc_width(dtbl * dt)
{
   int nc;
   ENTER();

   if (!dt) return -EINVAL;

   nc = dt->cols + 1;
   
   dt->cntx[0].fields = realloc(dt->cntx[0].fields, sizeof (struct field) * nc);
   dt->fixwidth = realloc(dt->fixwidth, sizeof(int) * nc);
   dt->align = realloc(dt->align, sizeof(int) * nc);
   dt->fixwidth[dt->cols] = -1;
   dt->align[dt->cols] = DTBL_ALIGN_LEFT;

   dt->cols = nc; 

   DEBUG3("dt->cntx[0].fields=%p  dt->fixwidth=%p dt->align=%p", 
	 dt->cntx[0].fields,  
	 dt->fixwidth, 
	 dt->align);
   
   LEAVE();
   return nc;
};

dtbl * dtbl_new(int cols)
{
   dtbl * dt;
   int i;
   char * env;

#ifndef EXTDEBUG
   env = getenv("DEBUG3");
   if (env) debugflag = 1;
#endif

   ENTER();
  
   dt = malloc(sizeof(dtbl));

   dtbl_init(dt);
  
   if (cols < 0) return NULL;
  

   dt->rows = 1;
   dt->cntx = malloc(sizeof(Row));
  
   dt->cntx[0].fields = malloc(sizeof (struct field) * cols);
   dt->fixwidth = malloc(sizeof(int) * cols);
   dt->align = malloc(sizeof(int) * cols);
  
   for (i = 0; i < cols; i++) {
      clear_field(&dt->cntx[0].fields[i]);
      dt->fixwidth[i] = -1;
      dt->align[i] = DTBL_ALIGN_LEFT;
   };
  
   dt->cols = cols;

   LEAVE();
   return dt;
};

void dtbl_delete(dtbl * dt)
{
   ENTER();
   int c, r;

   if (dt->title) {
      free (dt->title);
      dt->title = NULL;
   };

   if (dt->subtext) {
      free (dt->subtext);
      dt->subtext = NULL;
   };
   if (dt->fixwidth) {
      free (dt->fixwidth);
      dt->fixwidth = NULL;
   };
   if (dt->align) {
      free (dt->align);
      dt->align = NULL;
   };

   for (r = 0; r < dt->rows; r++) {
      for (c = 0; c < dt->cols; c++) {
	 if (dt->cntx[r].fields[c].context) {
	    free(dt->cntx[r].fields[c].context);
	    dt->cntx[r].fields[c].context = 0;
	 };
      }
      free(dt->cntx[r].fields);
      dt->cntx[r].fields = NULL;
   };
   
   free(dt->cntx);
   dt->cntx = NULL;
   
   LEAVE();
   return ;
};


void dtbl_title(DTable dt, char * title, ...)
{
   int n, l;
   char * s = NULL;
   
   va_list ap;
   va_start(ap, title);

   if (dt->title != NULL) free (dt->title);

   if (title == NULL) {
      dt->title = NULL;
      goto out;
   };

   n = 80;
   do {
      n *= 4;
      dt->title = realloc(dt->title, n);
      DEBUG3("alloc  %d ", n);
	
      l = vsnprintf(dt->title, n, title, ap);
      DEBUG3("vnsprintf ret %d => %s", l, s);
   } while(l >= (n-1));

 out:
   va_end(ap);
   return ;
};

void dtbl_subtext(DTable dt, char * text, ...)
{
   int n, l;
   char * s = NULL;
   
   va_list ap;
   va_start(ap, text);

   if (dt->subtext != NULL) free (dt->subtext);

   if (text == NULL) {
      dt->subtext = NULL;
      goto out;
   };

   n = 80;
   do {
      n *= 4;
      dt->subtext = realloc(dt->subtext, n);
      DEBUG3("alloc  %d ", n);
	
      l = vsnprintf(dt->subtext, n, text, ap);
      DEBUG3("vnsprintf ret %d => %s", l, s);
   } while(l >= (n-1));

 out:
   va_end(ap);
   return ;


};
	    
void dtbl_style(DTable dt, int style)
{

   dt->style = style;
   switch (style) {

   case DTBL_STYLE_LINES:
      dt->fL = '|';
      dt->fV = '|';
      dt->fR = '|';
      dt->fT = '-';
      dt->fH = '-';
      dt->fB = '-';      
      dt->fC = '+';
      dt->fieldpad = 1;
      break;
      
   case DTBL_STYLE_SIDE:
      dt->fL = ' ';
      dt->fV = ':';
      dt->fR = ' ';
      dt->fT = ' ';
      dt->fH = '-';
      dt->fB = ' ';
      dt->fC = ' ';
      dt->fieldpad = 1;
      break;

   default:
      dt->fL = ' ';
      dt->fV = ' ';
      dt->fR = ' ';
      dt->fT = ' ';
      dt->fH = ' ';
      dt->fB = ' ';
      dt->fC = ' ';
      dt->style = DTBL_STYLE_EMPTY;
      dt->fieldpad = 0;
      break;
   };
};

   
int dtbl_col_format(dtbl * dt, int col, int align, int fixwidth)
{
   ENTER();
   if (!dt) return -EINVAL;
   
   if ( (col >= dt->cols) || (col < 0) )  {
      return -EINVAL;
   };

   switch (align) {
   case DTBL_ALIGN_LEFT:
   case DTBL_ALIGN_CENTER:
   case DTBL_ALIGN_RIGHT:
      dt->align[col] = align;
      DEBUG3("col %d align %d", col, align);
   };

   if (fixwidth >= 0) {
      dt->fixwidth[col] = fixwidth;
      DEBUG3("col %d fixwidth %d", col, fixwidth);
   };

   LEAVE();
   return 0;
};
   
			  
void dtbl_headers(dtbl * dt, ...) 
{
   va_list ap;
   char *s;
   int n = 0;

   ENTER();
   if (dt == NULL) Fatal("dt == NULL");

   va_start(ap, dt);

   for(s = va_arg(ap, char *); s != NULL; s = va_arg(ap, char *)) {
      if (n >= dt->cols) inc_width(dt);
      dt->cntx[0].fields = realloc(dt->cntx[0].fields, sizeof (struct field)*(n+1));
      dt->cntx[0].fields[n].context = strdup(s);
      dt->cntx[0].fields[n].len = strlen(s);
      DEBUG3("header %d = (%d)%s", n, dt->cntx[0].fields[n].len, dt->cntx[0].fields[n].context);
      n++;
   }
   if (dt->rows == 0)
      dt->rows = 1;
  
   LEAVE();
   va_end(ap);
};


void dtbl_newrow(dtbl * dt, ...) 
{
   va_list ap;
   char *s;
   int n = 0, i;
   struct field * fields = NULL;

   ENTER();

   if (dt == NULL) Fatal("dt == NULL");
   if (dt->cols == 0) Fatal("dt->cols == 0");
   if (dt->rows == 0) Fatal("dt->rows == 0");

   va_start(ap, dt);

   DEBUG3("cols %d  rows %d", dt->cols, dt->rows);

   fields = malloc(sizeof(struct field) * (dt->cols));

   for (i = 0; i < dt->cols; i++) {
      clear_field(&fields[i]);
   };

   for(n = 0, s = va_arg(ap, char *); s != NULL; s = va_arg(ap, char *)) {    
      fields[n].context = strdup(s);
      fields[n].len = strlen(s);
      DEBUG3("row=%d col=%d = (%d)%s", dt->rows, n, fields[n].len, fields[n].context);
      n++;
   }
  
   dt->cntx = realloc(dt->cntx,sizeof(Row)*(dt->rows+1));
   dt->cntx[dt->rows].fields = fields;
   dt->rows++;

   LEAVE();
   va_end(ap);
};

void dtbl_setfield(dtbl * dt, int row, int col, char * fmt, ...) 
{
   va_list ap;
   char *s = NULL;
   int n = 0, l;
   struct field * f;

   ENTER();

   if (dt == NULL) Fatal("dt == NULL");
   if (dt->cols == 0) Fatal("dt->cols == 0");
   if (dt->rows == 0) Fatal("dt->rows == 0");

   DEBUG3("row  %4d   col %4d", row, col);
   DEBUG3("rows %4d  cols %4d", dt->rows, dt->cols);

   if (col < 0) Fatal("col < 0");
   if (row >= dt->rows) Fatal("attempted to set a field beyond end of the table");
   if (col >= dt->cols) Fatal("attempted to set a field beyond end of the row");

   // row are index from 0, but 0 is headers
   row++;
   if (row <= 0) row = dt->rows-1;

   va_start(ap, fmt);
  
   n = 10;

   do {
      n *= 4;
      s = realloc(s, n);
      DEBUG3("alloc  %d ", n);
	
      l = vsnprintf(s, n, fmt, ap);
      DEBUG3("vnsprintf ret %d => %s", l, s);
   } while(l >= (n-1));

   DEBUG3("set at row=%d col=%d", row, col);

   f = &dt->cntx[row].fields[col];

   if (f->context != NULL) free(f->context);
   f->context = s;
   f->len = l;
   DEBUG3("(%d)%s", dt->cntx[row].fields[col].len, dt->cntx[row].fields[col].context);
   LEAVE();

   va_end(ap);
};

static inline int csnprintf(char ** buffer, int * buflen, int * offset, char * fmt, ...)
{
   va_list ap;
   int rc, l, len;
   char * buf;
   int blen;

   ENTER();
   DEBUG3 ("buffer @ %p bufferlen %d offset %d", *buffer, *buflen, *offset);
   if (!(buffer && buflen && offset && fmt)) {
      errno = EINVAL;
      return -1;
   };

   va_start(ap, fmt);
   
   if (*buffer == NULL) {
      *buffer = malloc(1024);
      *buflen = 1024;
   };

   len = *offset;

   blen = *buflen;
   buf = *buffer;

   do {
      l = blen - len;	
      rc = vsnprintf (buf+len, l, fmt, ap);
      if (rc >= l) {
	 blen *= 2;
	 DEBUG3("extending buffer to %d bytes", blen);
	 buf = realloc(buf, blen);
      } 
   } while(rc >= l);
   
   va_end(ap);

   *buffer = buf;
   *buflen = blen;
   *offset += rc;
   DEBUG3 ("buffer @ %p bufferlen %d offset %d", *buffer, *buflen, *offset);
   LEAVE();
   return rc;
};

void dtbl_printf(dtbl * dt, FILE * fp) 
{
   int c, r, l, i, totalwidth;
   int * width;
   char * hline;
   char * ctxt;
   char * tblbuf = 0;
   int blen = 0, len = 0;

   ENTER();

   if (dt == NULL) Fatal("dt == NULL");
   if (fp == NULL) Fatal("fp == NULL");

   l = sizeof(int) * dt->cols;
   width = malloc(l); 
   memset(width, 0, l);

   /* we go thru and find the width of all columns */
   for (r = 0; r < dt->rows; r++) {
      for (c = 0; c < dt->cols; c++) {
	 if (dt->fixwidth[c] >= 0) {
	    width[c]= dt->fixwidth[c];
	 } else {
	    l = dt->cntx[r].fields[c].len;       
	    if (width[c] < l) width[c] = l;
	 };
	 DEBUG3("width[%d]=%d", c, width[c]);
      };
   };

   totalwidth = 0;
   for (c = 0; c < dt->cols; c++) {
      totalwidth += width[c];
   };
   totalwidth +=  (1 + 2*dt->fieldpad)*dt->cols ;

   /* if we print with lines we create here a seperator line
      printed before abnd after the tabel and after the headers */
  
   hline = malloc(totalwidth+1);

   for (i = 0; i < totalwidth; i++) {
      hline[i] = dt->fT;
   };    
   
   if (dt->title) {
      csnprintf(&tblbuf, &blen, &len, "%s\n", dt->title);
   };

   DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);
   csnprintf(&tblbuf, &blen, &len, "%c%.*s%c\n", dt->fC, totalwidth-1, hline, dt->fC);
   DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);

   for (r = 0; r < dt->rows; r++) {
      char x;
      int empty = 1;
      int prepad, postpad; 
      int pad;
      int align;
      int fl;

      // first we check for empty rows, and skip
      for (c = 0; c < dt->cols; c++) {
	 if (dt->cntx[r].fields[c].len) {
	    empty = 0;
	    break;
	 }
      };
      if (empty) continue;

      for (c = 0; c < dt->cols; c++) {
	 if (c == 0) x = dt->fL;
	 else x = dt->fV;

	 ctxt = dt->cntx[r].fields[c].context;
	 if (!ctxt) ctxt = "";

	 // headers are always left aligned
	 if (r == 0) 
	    align = DTBL_ALIGN_LEFT;
	 else 
	    align = dt->align[c];


	 fl = dt->cntx[r].fields[c].len;
	 
	 pad = width[c] - fl;
	 DEBUG3("width[%d] fieldlen = %d pad=%d",  width[c],  dt->cntx[r].fields[c].len, pad);

	 if (pad <= 0) {
	    postpad = prepad = 0;
	 } else {
	    switch (align) {
	      
	    case DTBL_ALIGN_RIGHT:
	       DEBUG3("align right");
	       postpad = 0;
	       prepad = pad;
	       break;
	      	      
	    case DTBL_ALIGN_CENTER:
	       DEBUG3("align right");
	       prepad = pad / 2;
	       postpad = pad - prepad;
	       break;
	    default:
	    case DTBL_ALIGN_LEFT:
	       DEBUG3("align right");
	       prepad = 0;
	       postpad = pad;
	    };
	 }; 


	 prepad += dt->fieldpad;
	 postpad += dt->fieldpad;


	 DEBUG3("pad = %d postpad = %d prepad = %d", pad, postpad, prepad);
	
	 csnprintf(&tblbuf, &blen, &len, "%c%.*s%*.*s%.*s", 
		   x, 
		   prepad, spcpad, 
		   fl, fl, ctxt, 
		   postpad, spcpad);
	 DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);
      }; 
      csnprintf(&tblbuf, &blen, &len, "%c\n", dt->fR);
      DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);

      if (r == 0) {
	 for (i = 0; i < totalwidth; i++) {
	    hline[i] = dt->fH;
	 };    
	 csnprintf(&tblbuf, &blen, &len, "%c%.*s%c\n", dt->fC, totalwidth-1, hline, dt->fC);
      };
      DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);
   
   };
  
   for (i = 0; i < totalwidth; i++) {
      hline[i] = dt->fB;
   };    
   csnprintf(&tblbuf, &blen, &len, "%c%.*s%c\n", dt->fC, totalwidth-1, hline, dt->fC);
   DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);

   if (dt->subtext) {
      csnprintf(&tblbuf, &blen, &len, "\n%s\n", dt->subtext);
   };
   DEBUG3 ("buffer @ %p bufferlen %d offset %d", tblbuf, blen, len);

   fputs(tblbuf, fp);

   free(hline);
   free(width);
   free(tblbuf);

   LEAVE();
   return;
};
