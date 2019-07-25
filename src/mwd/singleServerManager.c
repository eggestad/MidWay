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
#include <sys/wait.h>
#include <sys/time.h>
#include <poll.h>

#include <MidWay.h>
#include "mwd.h"
#include "simpleSrvManager.h"

static int shutdown_flag = 0;

static char * logtag = "servermgr";
static char * mktag() {
   return logtag;
};

int mwdfd = -1;
struct {
   struct managed_server_t * server;
   time_t exittime;
   int lastexitcode;
   int waskilled;
} managedserver = {0};


// config
int gracetime = 1;

// should only be called with TERM, HUP, INT, QUIT
static void     shutdownHandler(int sig, siginfo_t * si, void * uctx) {
   DEBUG("got sig %d, sending it along to the kids", sig);
   
   // TODO, tree processgroup?
   shutdown_flag = 1;
}
// should only be called with SIGCHLD
static void     childDiedHandler(int sig, siginfo_t * si, void * uctx) {

   DEBUG("child with pid died %d ; code = %d status = %d", si->si_pid,
	    si->si_code, si->si_status);

   int sts = 0;
   int waitrc = waitpid(si->si_pid, &sts, WNOHANG);
   DEBUG("wait returned %d sts %d", waitrc, sts);
   
   time(&managedserver.exittime);
   
   if (si->si_code == CLD_EXITED) {
      DEBUG("child with pid died %d exit %d", si->si_pid, si->si_status);
      managedserver.lastexitcode = si->si_status;
      
      srvmgr_messages_t msg;
      msg.exit.mtype = msg_type_exit;
      msg.exit.exitcode = si->si_status;
      msg.exit.server_pid = si->si_pid;
      write(mwdfd, &msg, sizeof(srvmgr_messages_t));
      return;
   }

   if (si->si_code == CLD_KILLED) {
      DEBUG("child with pid died %d terminated on signal %d", si->si_pid, si->si_status);

      int sig = si->si_status;
      // if we went down on one of these signals, don't restart
      if (sig == SIGTERM || sig == SIGHUP || sig == SIGQUIT || sig == SIGINT )
	 managedserver.waskilled = 1;
      else 
      	 managedserver.waskilled = 0;
      srvmgr_messages_t msg;
      msg.killed.mtype = msg_type_killed;
      msg.killed.server_pid = si->si_pid;
      msg.killed.signal = si->si_status;
      write(mwdfd, &msg, sizeof(srvmgr_messages_t));
      return;
   }

   
   DEBUG("child with pid died %d ; code = %d status = %d", si->si_pid,
	 si->si_code, si->si_status);
   
}

// should only be called with SIGCHLD
static void     sigUsrHandler(int sig, siginfo_t * si, void * uctx) {
      DEBUG("got usr sig %d", sig);
}

static void setup_manager_signals() {

   struct sigaction action = { 0 }; 
   int failed = 0;
   
   action.sa_flags = SA_SIGINFO;
   action.sa_sigaction = shutdownHandler;;
   sigfillset(&action.sa_mask);
   
   if (sigaction(SIGTERM, &action, NULL)) failed++;
   if (sigaction(SIGHUP, &action, NULL)) failed++;
   if (sigaction(SIGINT, &action, NULL)) failed++;
   if (sigaction(SIGQUIT, &action, NULL)) failed++;

   action.sa_sigaction = childDiedHandler;;
   
   if (sigaction(SIGCHLD, &action, NULL)) failed++;

   action.sa_sigaction = sigUsrHandler;;
   if (sigaction(SIGUSR1, &action, NULL)) failed++;
   if (sigaction(SIGUSR2, &action, NULL)) failed++;
   
   if (failed != 0) {
      Error("failed to install %d signals handlers\n", failed);
      exit(-19);
   }

}
int initSingleServerManager(struct managed_server_t * server, int fd) {

   mwdfd = fd;
   logtag = malloc(100);
   snprintf(logtag, 100, "[%s]", server->name);
   _mw_log_settagfunc(mktag);

   DEBUG("mgr pid, pgid = %ju, %ju", (uintmax_t)getpid(), (uintmax_t)getpgid(0));
   setpgid(0, 0);
   DEBUG(" pid, pgid = %ju, %ju", (uintmax_t)getpid(), (uintmax_t)getpgid(0));

   setup_manager_signals();
   
   managedserver.server =  server;
   
   return 0;
}

int  runSingleServerManager(struct managed_server_t * server, int mwdfd) {
   int mgrchildpipe[2];
   int rc = pipe(mgrchildpipe);
   
   Info("Starting server  %s", server->name);

   // second fork, child will do execve, i'm singleServerManager
   pid_t child = fork();
   if (child > 0) {
      // im manager process
      char buf[32];
      int waitforchilddeath = 0;
      int waitcount = 0;
      
      srvmgr_messages_t msg;
      msg.startup.mtype = msg_type_startup;
      msg.startup.server_pid = child;
      msg.startup.pgid = getpgid(0);
   
      write(mwdfd, &msg, sizeof(srvmgr_messages_t));
      
      close(mgrchildpipe[1]);
      
      struct pollfd pipes[2] = {0};
      pipes[0].fd = mwdfd;
      pipes[0].events = 0;
      pipes[0].revents = 0;
      pipes[1].fd = mgrchildpipe[0];
      pipes[1].events = 0;
      pipes[1].revents = 0;
      
      int  errcount = 0;
      do {

	 DEBUG2("polling %d %d", pipes[0].fd, pipes[1].fd);
	 int nready = poll(pipes,  2, 5000);
	 DEBUG2("poll returned %d %d %d", nready, pipes[0].revents, pipes[1].revents);
	 if (waitforchilddeath) {
	    int signals[] ={ SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGKILL};
	    DEBUG("send kill to serverpid %d", server->serverpid);
	    kill (server->serverpid, signals[waitcount]);
	    waitcount++;
	    if (waitcount >= 5) {
	       Warning("manager process exits while child still runs");
	       return 0;
	    }
	    continue;
	 }
		  
	 if (nready == 0) continue;

	 if (pipes[0].revents != 0) {
	    Info("mwd died") ;
	    waitforchilddeath = 1;
	    pipes[0].fd = - pipes[0].fd;
	    shutdown_flag = 1;
	    continue;
	 }

	 if (pipes[1].revents != 0) {
	    rc = read(mgrchildpipe[0], buf, 32);
	    if (rc == 0) {
	       Info("child died");
	       return 0;
	    }
	    if (rc > 0)  {
	       DEBUG("readfrom child %d %.*s, ignoring", rc, rc, buf);
	       continue;
	    }
	    if (errno == EINTR) continue;
	    perror("read from child");
	    //	 wait(NULL);
	    Info("child died 2");
	    return 0;
	 }
	 Warning("poll returned but neither mwdfd or childfd had anything");
	 errcount++;
      } while (errcount < 10);
      DEBUG("while loop terminated, errcount=%d", errcount);
      shutdown_flag = 1;
      return 0;
   }

   // i'm the process that will do exec on sever executable/wrapper
   if (child == 0) {
      close(mgrchildpipe[0]);

      // we add $MWHOME/bin to the beginning of the path 
      char * envpath = getenv("PATH");
      int newpathlen =strlen(envpath) + strlen(mwd_get_mwhome())
	 + strlen(instancename) + 10; 
      char newenvpath[newpathlen];
      snprintf(newenvpath, newpathlen, "%s/%s/bin:%s", mwd_get_mwhome(),
	       instancename, envpath);
      setenv ("PATH", newenvpath, 1);
      DEBUG("PATH is now %s", getenv("PATH"));
      char * path = server->fullpath;
      char * args[2] = { path, NULL };
      
      Info("exec server %s", path);

      extern char **environ;
      rc = execve (path, args, environ /* server->envp */);
      perror("exec failed");
      write(mgrchildpipe[1], "exec fail", 10);
      
      //kill(pid, SIGUSR1);
      exit(1);
   }
   
}

int runSingleServerManagerLoop(struct managed_server_t *server, int mwdfd) {

   int rc = runSingleServerManager(server, mwdfd);
   DEBUG("child has died %d", rc);
   do {
      if (shutdown_flag) {
	 Info("Shutdown manager for %s done", server->name);
	 return 0;
      }
      if (managedserver.waskilled) {
	 Info("Server was killed on signal, not restarting");
	 return 0;
      }
      sleep(gracetime);
      Info("Restarting Server");
      rc = runSingleServerManager(server, mwdfd);
      DEBUG("child has died %d", rc);
   } while (!shutdown_flag);
   
}
