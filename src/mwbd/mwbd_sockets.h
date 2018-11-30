/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
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

struct fd_info {
  int fd;
  time_t lastused;
  int majorversion;
  int minorversion;
  char domain[MWMAXNAMELEN+1];
  char instance[MWMAXNAMELEN+1];
  char * incomplete_mesg;
  struct fd_info * next;
};

#define READREADY    1
#define WRITEREADY   2
#define EXCEPTREADY  3

void markfd(int fd);
void unmarkfd(int fd);
void add_gw(int fd);
void add_c(int fd);
void del_gw(int fd);
void del_c(int fd);
struct fd_info * find_gw(char * domain, char *instance);
struct fd_info * find_gw_byfd(int fd);
struct fd_info * find_c_byfd(int fd);


int accept_tcp(void);
int accept_unix(void);
int opensockets(void);
int waitdata(int * fd, int * operation);
int sendfd(int new_fd, int gwfd, char * message, int messagelen);
void closeall(void);
void client_clean(void);

void initmcast(void);
