/*
  MidWay
  Copyright (C) 2019 Terje Eggestad

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

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>

#include <glib.h>

#include <MidWay.h>
#include "mwd.h"
#include "simpleSrvManager.h"

static int shutdown_flag = 0;

static GHashTable * servertable = NULL;
static GSequence * serverlist = NULL;

static int max_fd = -1;
static struct managed_server_t ** server_by_fd = NULL;
static struct pollfd *poll_fds = NULL;
static nfds_t npoll_fds = 0;



static struct managed_server_t * addserver(char * name, char * path,  char * const envp[] ) {
   Info("server name %s path %s", name, path);
   
   struct managed_server_t * server = malloc(sizeof(struct managed_server_t));
   memset(server, 0, sizeof(struct managed_server_t));
   server->name = name;
   server->fullpath = path;
   server->envp = envp;

   g_sequence_append (serverlist, server);
   //   g_hash_table_insert (servertable, 
   return server;
}

void dumpServertable(){
   DEBUG("fd  sts mgrpid serverpid   pgid name");

   if (g_sequence_is_empty (serverlist)) return;

   GSequenceIter * iter = g_sequence_get_begin_iter (serverlist);
   while (! g_sequence_iter_is_end(iter)) {
      struct managed_server_t * srv = g_sequence_get (iter);
      DEBUG("%3d %3d %6d    %6d %6d %s", srv->fd, srv->status,
	    srv->mgrpid, srv->serverpid, srv->pgid, srv->name);

      iter = g_sequence_iter_next (iter);
   }
   
}
int triggershutdown(int signal) {
   DEBUG("trigger shutdown");
   shutdown_flag = 1;
   int stillrunning = 0;
   if (g_sequence_is_empty (serverlist)) return stillrunning;;
   GSequenceIter * iter = g_sequence_get_begin_iter (serverlist);
   while (! g_sequence_iter_is_end(iter)) {
      struct managed_server_t * srv = g_sequence_get (iter);
      if (srv->status != server_status_stopped) {
	 DEBUG("kill %d %d", -srv->pgid, signal);
	 kill(-srv->pgid, signal);
	 stillrunning ++;
      }
      iter = g_sequence_iter_next (iter);
   }
   return stillrunning;
}
   

void doMessage(int fd, srvmgr_messages_t * msg){

   switch (msg->type.mtype) {

   case msg_type_startup:
      DEBUG("startup message");
      server_by_fd[fd]->serverpid = msg->startup.server_pid;
      server_by_fd[fd]->pgid = msg->startup.pgid;
      server_by_fd[fd]->status = server_status_running;
      
      break;
      
   case msg_type_restart:
      DEBUG("restart message");
      break;
   
   case msg_type_exit:
      DEBUG("exit message");
      server_by_fd[fd]->serverpid = msg->exit.server_pid;
      server_by_fd[fd]->status = server_status_stopped;      
      break;
   
   case msg_type_killed:
      DEBUG("killed message");
      server_by_fd[fd]->serverpid = msg->killed.server_pid;
      server_by_fd[fd]->status = server_status_stopped;      
      break;
   }
   dumpServertable();
}


void server_booted(struct managed_server_t * bsrv) {

   int fd = bsrv->fd;
   DEBUG("adding new booted server on fd %d, fdmax=%d", fd, max_fd);
   if (max_fd < fd) {
      int last_max_fd = max_fd;
      DEBUG("increasing max fd from last_max_fd %d to fd %d", max_fd, fd);
      max_fd =   fd;
      server_by_fd = realloc(server_by_fd, (max_fd+1) * sizeof(struct managed_server_t*));

      DEBUG("increasing table %d to fd %d", last_max_fd+1, max_fd);
      for (int i = last_max_fd+1; i <= max_fd; i++) {
	 server_by_fd[i] = NULL;
      }
   };
   if (server_by_fd[fd] != NULL) {
      Error("got a new server of fd %d, but fd was already in use");
      return;
   }

   server_by_fd[fd] = bsrv;
   int pollidx = npoll_fds;
   npoll_fds++;
   poll_fds = realloc(poll_fds, npoll_fds * sizeof(struct pollfd));
   struct pollfd * ps = &poll_fds[pollidx];
   ps->fd = fd;
   ps->events = POLLIN;
   ps->revents = 0;

   dumpServertable();
   
   return;
}

void server_stopped_byfd(int fd) {
   
   struct managed_server_t * bsrv = server_by_fd[fd];
   server_by_fd[fd] = NULL;
   bsrv->mgrpid = 0;
   bsrv->serverpid = 0;
   bsrv->pgid = 0;
   bsrv->status = server_status_stopped;
   bsrv->fd = -1;
   
   for (int i = 0; i < npoll_fds; i++) {
      int pfd = poll_fds[i].fd;
      if (pfd == fd) {
	 npoll_fds--;
	 poll_fds[i] = poll_fds[npoll_fds];
	 dumpServertable();

	 return;
      }
   }
   Error("can't unpoll fd=%d", fd);
}



static struct managed_server_t * bootOne(struct managed_server_t * server ) {

   // this create one child that will run as mwd that will manage the server
   // then that child will do a forc exec
   
   // TODO: create pipes between mwd master and server manager
   // as well as server manager and server, in server manager will then do a poll
   // on both so that we detect either dying

   
   Info("booting server name %s path %s", server->name, server->fullpath);   
   
   int mwdmgrpipe[2];
   int rc = pipe(mwdmgrpipe);
   pid_t ppid = getpid();

   // first fork, child is single server manager, I'm in mwd, manager thread
   pid_t pid = fork();

   char * path = server->fullpath;

   if (pid > 0) { // i'm parent i.e mwd and I'm done
      // close write
      close(mwdmgrpipe[1]);
      server->fd = mwdmgrpipe[0];
      server->mgrpid = pid;
      server->status = server_status_startup;
      server_booted(server);
      return server;
   }

   if (pid < 0) {
      perror ("fork failed");
      return NULL;
   } // on error
   
   //if (pid == 0) { // i'm child i.e server manager

   // close read
   close(mwdmgrpipe[0]);
   initSingleServerManager(server, mwdmgrpipe[1]);

   runSingleServerManagerLoop(server, mwdmgrpipe[1]);
   exit(1);
}


int bootAll( ) {

   extern char **environ;
   char * mwd_get_mwhome() ;

   char * homedir = mwd_get_mwhome();
   char **  envp = environ;
   DIR *dir;
   struct dirent *ent;

   char bootdir[PATH_MAX];
   snprintf(bootdir, PATH_MAX, "%s/%s/boot", homedir, instancename);

   char url[256];
   snprintf(url, 256, "ipc:/%s",  instancename, 0);
   setenv("MWHOME", homedir, 0);
   setenv("MWURL", url, 0);
   setenv("MWINSTANCE", instancename, 0);
   
   Info(" boot dir %s", bootdir);
   
   if ((dir = opendir (bootdir)) != NULL) {
      /* print all the files and directories within directory */
      while ((ent = readdir (dir)) != NULL) {

	 // ignore hidden files
	 if (strncmp(ent->d_name, ".", 1) == 0) continue;
	 // ignore emacs save files
	 if (strncmp(ent->d_name + strlen(ent->d_name)-1, "~", 1) == 0) continue;
	 
	 printf ("%s\n", ent->d_name);

	 char fullpath[PATH_MAX];	 
	 snprintf(fullpath, PATH_MAX, "%s/%s/boot/%s",
		  homedir, instancename, ent->d_name);
		  
	 struct stat statbuf;	 
	 int rc = stat(fullpath, &statbuf);
	 if (rc != 0) {
	    perror("");
	    continue;
	 }
	 switch (statbuf.st_mode & S_IFMT) {
	 case S_IFBLK:  printf("block device\n");            break;
	 case S_IFCHR:  printf("character device\n");        break;
	 case S_IFDIR:  printf("directory\n");               break;
	 case S_IFIFO:  printf("FIFO/pipe\n");               break;
	 case S_IFLNK:  printf("symlink\n");                 break;
	 case S_IFREG:  printf("regular file\n");            break;
	 case S_IFSOCK: printf("socket\n");                  break;
	 default:       printf("unknown?\n");                break;
	 }

	 if (! statbuf.st_mode & S_IFMT == S_IFREG) {
	    perror("non file in boot dir, ignoring");
	    continue;
	 }
	 rc = access (fullpath, X_OK);
	 if (rc != 0) {
	    perror("not executable");
	    continue;
	 }
	 struct managed_server_t * bsrv = addserver(ent->d_name, fullpath, envp);

	 bootOne(bsrv);
      }
      closedir (dir);
      dumpServertable();
   } else {
      /* could not open directory */
      perror ("");
      return EXIT_FAILURE;
   }
 
}



void * serverManagerThread(void * data) {
   
   Info("Starting Server manager Thread");

   //servertable = g_hash_table_new (g_direct_hash, g_direct_equal);
   serverlist = g_sequence_new (NULL);
   bootAll();
   time_t boottime;
   time(&boottime);
   int nidx = 0;
   do {
      DEBUG2("polling %d fd's" , npoll_fds);
      int nready = poll(poll_fds,  npoll_fds, 1000);
      DEBUG2("poll returned %d" , nready);
      
      for (int i = 0; i < npoll_fds; i++) {
	 short ev = poll_fds[i].revents;
	 if (ev == 0) continue;

	 int fd = poll_fds[i].fd;
	 DEBUG("revent %x on fd %d", ev, fd);
	 
	 if (ev & POLLIN) {
	    srvmgr_messages_t msg;
	    int len = read(fd, &msg, sizeof(srvmgr_messages_t));
	    if (len !=  sizeof(srvmgr_messages_t)) {
	       Error("wrong message len %d", len);
	       continue;
	    }
	    DEBUG("mtype %d", msg.type.mtype);
	    doMessage(fd, &msg);
	    
	 } else if (ev & (POLLHUP|POLLERR)) {
	    server_stopped_byfd(fd);
	 } else if (ev & POLLNVAL) {
	    Error("did a poll on a closed fd %d", fd);
	    server_stopped_byfd(fd);
	 } else {
	    Error("got unexpected event %d on fd %d", ev, fd);
	    server_stopped_byfd(fd);
	    sleep(1);
	 }
      }
   } while (!shutdown_flag ||  npoll_fds > 0);
   Info("All servers down, shutting down Server manager Thread");
      
}
