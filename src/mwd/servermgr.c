/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

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

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <paths.h>


#include <MidWay.h>

#include <ipctables.h>
#include <version.h>
#include <urlencode.h>
#include "mwd.h"
#include "execvpe.h"
#include "servermgr.h"
#include "watchdog.h"


static int default_autoboot = TRUE;
static char ** defaultEnv = NULL;


struct ServerGroup * grouproot = NULL, * currentgroup = NULL;
struct Server * currentserver = NULL;

void smgrDumpTree(void)
{
  struct ServerGroup * SG;
  struct Server * S;
  int i;

  DEBUG("Dumping Server tree");
  for (SG = grouproot; SG != NULL; SG = SG->next) {
    DEBUG("  GROUP %s autoboot=%d", SG->name, SG->autoboot);

    for(S = SG->serverlist; S != NULL; S = S->next) {
      DEBUG("    SERVER %s autoboot=%d exec=%s", 
	     S->name, S->autoboot, S->exec);
      
      for (i = 0; S->args[i] != NULL; i++) {
	DEBUG("       argument %3d: \"%s\"", i, S->args[i]);
      }
    };
  };
  DEBUG("Dump of Server tree ends");
};

/* pending sigchild count */
static int sigchldcount = 0;

static int killwaits = 0;


/* 
static int getuserenv(void);
   For future reference, getting login env, NOTE all envs in
   environ(5) must be set first. see jim.

   bash -c "exec bash -a -bash -c env" 

*/


/* 
 * The envp is not a single malloced area as the envp given by exec
 * is, but the var/value pairs is a malloced seperatly as is the char
 * * array, in order to avoid a lot of moves and pointer recalcs as we
 * increase the array.  */

static int mySetEnv(char *** envpp, char * var, char * value)
{
  char ** envp;
  int  varlen, vallen, i;

  if (var == NULL) return -1;

  if (envpp == NULL) return -1;

  envp = *envpp;
  
  varlen = strlen(var);
  if (value == NULL) 
    vallen = 0;
  else 
    vallen = strlen(value);

  if (envp == NULL) {
    envp = malloc(sizeof(char *) * 3);
    i = 0;
  } else {
    for ( i = 0; envp[i] != NULL; i++) ;
    envp = realloc(envp, sizeof(char *) * (i+2));
  };

  envp[i] = malloc(varlen+vallen+2);
  sprintf( envp[i], "%s=%s", var, value);
  envp[i+1] = NULL;
  
  *envpp = envp;
  return 0;
};

static int myUnSetEnv(char *** envpp, char * var)
{
  int varlen, i, j, rc;
  char ** envp;

  if ( (envpp == NULL) || (var == NULL) ) return -1;
  envp = * envpp;

  varlen = strlen(var);
  for (i = 0; envp[i] != NULL; i++) {
    rc = strncmp(var, envp[i], varlen);
    if (rc != 0) continue;
    if (envp[i][varlen] == '=') {
      /* ok we found the var */
      free(envp[i]);
      for (j = i; envp[i] != NULL; j++) ;
      j--;
      envp[i] = envp[j];
      envp[j] = NULL;
      return 0;
    };
  };
  return -1;
};

static char *  myGetEnv(char ** envp, char * var)
{
  int varlen, i, rc;

  if ( (envp == NULL) || (var == NULL) ) return NULL;

  varlen = strlen(var);
  for (i = 0; envp[i] != NULL; i++) {
    rc = strncmp(var, envp[i], varlen);
    if (rc != 0) continue;
    if (envp[i][varlen] == '=') {
      /* ok we found the var */
      return &envp[i][varlen+1];
    };
  };
  return NULL;
};

static char ** dupEnv(char ** envp)
{
  char ** new_envp;
  int i;

  if (envp == NULL) return NULL;

  for (i = 0; envp[i] != NULL; i++) ;
  
  new_envp = malloc(sizeof(char *) * (i+1));

  for (i = 0; envp[i] != NULL; i++) {
    new_envp[i] = strdup(envp[i]);
  };

  new_envp[i] = NULL;
  return new_envp;
};

/************************************************************************/


void smgrBeginGroup(char * name)
{
  struct ServerGroup **top, * this;

  top = &grouproot;
  while(*top != NULL) {
    this = *top;
    if ((name == NULL) && (this->name == NULL)) {
      currentgroup = this;
      return;
    };
    if ((name != NULL) && (this->name != NULL) && (strcmp(name, this->name) == 0)) {
      currentgroup = this;
      return;
    };
    top = &((*top)->next);
  };
  this = (struct ServerGroup * ) malloc(sizeof(struct ServerGroup));
  *top = this;
  if (name != NULL) 
    this->name = strdup(name);
  else 
    this->name = "";

  this->autoboot = UNASSIGNED;
  this->env = dupEnv(defaultEnv);
  this->bindir = NULL;
  this->serverlist = NULL;
  this->next = NULL;
  Info("Server Group %s created", name);
  currentgroup = this;
  return;
}
  
  

int smgrSetGroupSetEnv(char * var, char * value)
{
  if (!currentgroup) {
    errno = EILSEQ;
    return -1;
  };

  return mySetEnv(&currentgroup->env, var, value); 
};

int smgrSetGroupUnSetEnv(char * var)
{
  if (!currentgroup) {
    errno = EILSEQ;
    return -1;
  };

  return myUnSetEnv(&currentgroup->env, var);
};


int smgrSetGroupAutoBoot(int flag)
{
  if ( (flag != UNASSIGNED) && (flag != FALSE) && (flag != TRUE) ) {
    errno = EINVAL;
    return -1;
  };

  if (!currentgroup) {
    errno = EILSEQ;
    return -1;
  };

  currentgroup->autoboot = flag;
  return 0;
};

void smgrEndGroup(void)
{
  if (currentgroup != NULL)
    Info("Server Group %s ends", currentgroup->name);
  currentgroup = NULL;
};


int smgrBeginServer(char * name)
{
  struct Server **top, * this;

  if (name == NULL) {
    errno = EINVAL;
    return -1;
  };

  if (currentgroup == NULL) smgrBeginGroup(NULL);

  top = &currentgroup->serverlist;

  while(*top != NULL) {
    this = *top;

    if ((strcmp(name, this->name) == 0)) {
      currentserver = this;
      return 0;
    };    
    top = &((*top)->next);
  };

  /* create a new server if we get here */
  this = (struct Server * ) malloc(sizeof(struct Server));
  *top = this;
  this->name = strdup(name);
  this->group = currentgroup;

  this->env = dupEnv(currentgroup->env);
  this->exec = NULL;
  this->libexec = NULL;
  this->stdout = NULL;
  this->stderr = NULL;
  this->args = NULL;
  this->activelist = NULL;
  this->active = 0;
  this->autoboot = UNASSIGNED;
  this->start_min = UNASSIGNED;
  this->start_max = UNASSIGNED;
  this->started = 0;
  this->booted = FALSE;
  this->next = NULL;

  Info("Server %s created in group %s", name, 
	this->group->name);
  currentserver = this;
  return 0;
};

int smgrSetServerSetEnv(char * var, char * value)
{
  if (!currentserver) {
    errno = EILSEQ;
    return -1;
  };

  return mySetEnv(&currentserver->env, var, value); 
};


int smgrSetServerUnSetEnv(char * var)
{
  if (!currentserver) {
    errno = EILSEQ;
    return -1;
  };

  return myUnSetEnv(&currentserver->env, var);
};

int smgrSetServerAutoBoot(int boolean)
{
  if (currentserver == NULL) return -1;

  currentserver->autoboot = boolean;
  return 0;
};

int smgrSetServerExec(char * path)
{
  
  if (currentserver == NULL) return -1;
  if (path == NULL) return -1;

  if (currentserver->exec != NULL) free( currentserver->exec);

  currentserver->exec = strdup(path);
  
  return 0;
};

static char ** parseargs(char * line, char * arg0)
{
  char * buffer;
  char ** args;
  int argcount;
  char quotechar;
  int l, di;
  int in_quote = 0;
  char * start, *here;

  l = 8;
  buffer = malloc(l);

  args = malloc(sizeof(char *)*2 );
  
  if (arg0) 
    args[0] = arg0;
  else 
    args[0] = "";
  argcount = 1;
  args[1] = NULL;

  di = 0;
  for (start = line, here = line; *here != '\0'; here++) {

    if (di > (l-4)) {
      l *= 2;
      buffer = realloc(buffer, l);
    };

    /* skip leading spaces */
    if ( (start == here) && isspace(*here) ) {
      start++;
      continue;
    };
    
    /* begin quote if we're in a quote look for the other end */
    if (in_quote) {
      if (*here == quotechar) {
	in_quote = 0;
	continue;
      };
      buffer[di++] = *here;
      continue;
    };

    /* / escapes */
    if (*here == '\\') {
      here++;

      switch (*here) {

      case '\\':
	buffer[di++] = '\\';
	break;
	
      case 'n':
	buffer[di++] = '\n';
	break;

      case 't':
	buffer[di++] = '\t';
	break;

      case 'r':
	buffer[di++] = '\r';
	break;

      case 'e':
	buffer[di++] = '\e';
	break;

      case 'f':
	buffer[di++] = '\f';
	break;

      default:
	buffer[di++] = *here;
	break;
      };
      continue;
    };

    /* begin quote */
    if ( (*here == '\'' ) || (*here == '"' ) ) {
      quotechar = *here;
      in_quote = 1;
      continue;
    };

    /* en of argument, start a new one. */
    if (isspace(*here)) {
      buffer[di] = '\0';
      args = realloc(args, sizeof(char *) * (argcount+2));
      args[argcount++] = buffer;
      args[argcount] = NULL;
      l = 8;
      buffer = malloc(l); 
      di = 0;
      start = here +1;
      continue;
    };
      
    buffer[di++] = *here;

  }; /* while arglist */

  if (di > 0) {
    buffer[di] = '\0';
    args = realloc(args, sizeof(char *) * (argcount+2));
    args[argcount++] = buffer;
    args[argcount] = NULL;
  };
  
  return args;
}


int smgrSetServerArgs(char * argline)
{
  char *arg0c, *arg0;
  int len, i;

  if (argline == NULL) return -1;
  len = strlen(argline);

  if (currentserver == NULL) return -1;

  if (currentserver->args && (currentserver->args[0]  != NULL)) {
    arg0 = currentserver->args[0];
  } else {
    arg0c = strdup(currentserver->exec);
    basename(arg0c);
    arg0 = strdup(basename(arg0c));
    free(arg0c);
  }

  currentserver->args = parseargs(argline, arg0);

  if (currentserver->args == NULL) {
    return -1;
  };

  DEBUG("for server %s exec path is \"%s\" and arg0 is %s", 
	currentserver->name, currentserver->exec, currentserver->args[0] );  
  for (i = 0; currentserver->args[i] != NULL; i++) {
    DEBUG("  arg %4d: \"%s\"", i, currentserver->args[i]);
  };

  return 0;
};

int smgrSetServerMinMax(int min, int max)
{
  if (currentserver == NULL) return -1;

  DEBUG("smgrSetServerMinMax: start_min = %d,  start_max = %d", min, max);
    
  currentserver->start_min = min;
  currentserver->start_max = max;
  return 0;
};

void smgrEndServer(void)
{
  if (currentserver != NULL) 
    Info("Server %s in group %s ends", 
	  currentserver->name, currentgroup->name);
  currentserver = NULL;
};

/************************************************************************
 *
 * default values
 */

void smgrSetDefaultBinDir(char *path);
void smgrSetDefaultAutoBoot(int ab)
{
  if (ab != 0) default_autoboot = 1;
  else  default_autoboot = 1;
};

void smgrInit(void)
{
  char tmp[1024];
  
  sprintf (tmp, "%s:%s/bin", _PATH_DEFPATH, MW_PREFIX);
  mySetEnv(&defaultEnv, "PATH", tmp);
  mySetEnv(&defaultEnv, "HOME", getenv("HOME"));
  mySetEnv(&defaultEnv, "LOGNAME", getenv("LOGNAME"));
  mySetEnv(&defaultEnv, "USER", getenv("USER"));
  mySetEnv(&defaultEnv, "LANG", getenv("LANG"));
  mySetEnv(&defaultEnv, "SHELL", getenv("SHELL"));
  mySetEnv(&defaultEnv, "TERM", getenv("TERM"));
};
  

/************************************************************************
 * here are the runtime command functions, also used at startup for
 * autoboot
 ************************************************************************/

static struct Server * smgrfindServerByName(char * groupname, char * servername)
{
  struct ServerGroup  * SG;
  struct Server * S;

  if (groupname == NULL) groupname = "";
  if (servername == NULL) return NULL;

  for (SG = grouproot; SG != NULL;  SG = SG->next) {

    if (strcmp(SG->name, groupname) == 0) {
      for (S = SG->serverlist; S != NULL;  S = S->next) {
	if (strcmp(S->name, servername) == 0) 
	  return S;
      };
    };
  };
  return NULL;
};

static struct Server * smgrfindServerByPid(pid_t pid)
{
  struct ServerGroup  * SG;
  struct Server * S;
  int i;

  for (SG = grouproot; SG != NULL;  SG = SG->next) {
    for (S = SG->serverlist; S != NULL;  S = S->next) {
      for ( i = 0; i < S->active; i++) {
	if (S->activelist[i].pid == pid) 
	  return S;
      };
    };
  }
  return NULL;
};

	  
int smgrExecServer(struct Server * S) 
{
  struct ActiveServer * AS;
  int size;
  
  DEBUG("Executing Server %s in Group %s with %d active",
	S->name, NONULL(S->group->name), S->active);

  size = sizeof(struct ActiveServer) * (S->active+2);
  S->activelist = realloc(S->activelist, size);
  AS = &S->activelist[S->active];
  S->active++;

  DEBUG(" AS=%p S->activelist=%p size = %d", AS, S->activelist, size);
  AS->status = BOOTING;
  AS->kills = 0;
  AS->pid = fork();


  if (AS->pid == -1) {
    Error("fork failed for server %s group %s, reason %s", 
	  S->name, NONULL(S->group->name), strerror(errno));
    S->active--;
    return -1;
  };
  
  if (AS->pid == 0) {
    int i;
    char ** envp, ** tmpenv;
    char tmp[1024];
    char * path;

    DEBUG("execve \"%s\",", S->exec);
    for (i = 0; S->args[i] != NULL; i++) {
      DEBUG(" arg %d = \"%s\"", i, S->args[i]);
    };

    /* last ditch env settings, these can't be overridden in config */

    envp = dupEnv(S->env);

    path = myGetEnv(envp, "PATH");
    sprintf(tmp, "%s/%s/bin:%s", ipcmain->mw_homedir, instancename, path);
    mySetEnv(&envp, "PATH", tmp);
    mySetEnv(&envp, "MWHOME", ipcmain->mw_homedir);
    mySetEnv(&envp, "MWADDRESS", uri);
    mySetEnv(&envp, "MWINSTANCE", instancename);

    sprintf(tmp, "%s/%s/run", ipcmain->mw_homedir, instancename);
    mySetEnv(&defaultEnv, "PWD", tmp);

    DEBUG("Environment:");
    for (tmpenv = envp; tmpenv[0] != NULL; tmpenv++) {
       DEBUG("  %s", tmpenv[0]);
    }

    execvpe(S->exec, S->args, envp);    
    Error("exec failed for \"%s\", reason %s", S->exec, strerror(errno));
    exit(-1);
  };
  Info("Started Server %s in Group %s, pid = %d", 
	S->name, NONULL(S->group->name), AS->pid);

  AS->status = RUNNING;
  return AS->pid;
};

/* StartServer and StopServer are used to order start and stop
   externally, like manually.  we inc and dec the booted counter which
   become the hard lower limit. The start_min and start_max are
   unrelated to this

   It also means that the calc of how many servers shal be started is
   a bit difficult. if the autoboot flag is set, the smgr process will
   start between start_min and start_max - forcestarted servers
   depending on load of the running servers. Upon shutdown all servers
   started by Start will be shutdown, and not restarted on BootServer.

   Note start_min <= 0 is defacto autoboot = 0.  */

int smgrStartServer(char * groupname, char * servername)
{
  struct Server * S;

  S = smgrfindServerByName(groupname, servername);
  
  if (S == NULL) {
    errno = ENOENT;
    return -1;
  };

  Info("Force Starting Server %s in Group %s", 
	S->name, S->group->name);

  if (!S->booted) return -1;

  S->started++;
  return smgrExecServer(S);
};

int smgrStopServer(char * groupname, char * servername)
{
  struct Server * S;
  struct ActiveServer * AS;
  int i = 0;

  S = smgrfindServerByName(groupname, servername);
  
  if (S == NULL) {
    errno = ENOENT;
    return -1;
  };

  if (S->started <= 0) return -1;
  S->started--;
  
  AS = S->activelist;
  for(i = 0; i < S->active; i++) 
    if (AS[i].status == RUNNING) {
      Info("Force Stopping an instance of server %s, group %s, pid = %d", 
	    servername, groupname, AS[i].pid);
      AS[i].status = SHUTWAIT;
      AS[i].kills = 1;
      killwaits++;
      kill(AS[i].pid, SIGTERM);
      return AS[i].pid;
    };
  return 0;
};

int smgrBootServer(char * groupname, char * servername)
{
  struct Server * S;
  int i, rc;

  S = smgrfindServerByName(groupname, servername);
  
  if (S == NULL) {
    errno = ENOENT;
    return -1;
  };
  
  /* already booted */
  if (S->booted == TRUE) return -1;

  Info("Booting Server %s in Group %s start_min = %d", 
	S->name, S->group->name, S->start_min);

  S->booted  = TRUE;
  S->started = S->start_min;
  if (S->started < 0) S->started = 0;
  for (i = 0; i < S->started; i++) {
    rc = smgrExecServer(S);    
  };
  return 0;
};

int smgrShutdownServer(char * groupname, char * servername)
{
  struct Server * S;
  struct ActiveServer * AS = NULL;
  int i;

  S = smgrfindServerByName(groupname, servername);
  
  if (S == NULL) {
    errno = ENOENT;
    return -1;
  };
  
  /* already down */
  if (S->booted == FALSE) return -1;

  S->booted  = FALSE;
  Info("Shutdown Server %s in Group %s", 
	S->name, NONULL(S->group->name));

  AS = S->activelist;
  S->started = 0;

  for(i = 0; i < S->active; i++) 
    if (AS[i].status == RUNNING) {
      Info("Stopping an instance of server %s, group %s, pid = %d", 
	    servername, NONULL(groupname), AS[i].pid);
      AS[i].status = SHUTWAIT;
      AS[i].kills = 1;
      killwaits++;
      kill(AS[i].pid, SIGTERM);
    };
  
  return 0;
};

/* functions that are used when a server dies. */

struct Server *  smgrServerFuneral(pid_t pid)
{
  struct Server * S;
  int i;

  S = smgrfindServerByPid(pid);
  if (S == NULL) return NULL;
  
  DEBUG("Funeral start AS=%p", S->activelist);

  for(i = 0; i < S->active; i++) {
    if (S->activelist[i].pid == pid) {
      Info("An instance of server %s, group %s, pid = %d died", 
	    S->name, S->group->name, S->activelist[i].pid);
      S->active--;
      if (S->active == 0) {
	free(S->activelist);
	S->activelist = NULL;
      } else {
	S->activelist[i] = S->activelist[S->active];
	S->activelist[S->active].pid  = -1;
      };
      DEBUG("Funeral good end AS=%p", S->activelist);
      // force watchdog to do a ipc clean
      trigger_watchdog();
      return S;
    };
  }
  DEBUG("Funeral bad end AS=%p", S->activelist);
  return NULL;
};

int smgrPendingSIGCHLD(void)
{
  return sigchldcount;
};

void smgrSigChildHandler(void)
{
  sigchldcount++;
};

int smgrDoWaitPid(void)
{
  struct Server * S;
  pid_t pid;
  int status;

  if (ipcmain == NULL) {
    Fatal("aborting, processing wait on server processes without a ipcmaininfo");
    abort();
  };

  errno = 0;
  while( (pid = waitpid(-1, &status, WNOHANG)) > 0)  {

     if (pid == ipcmain->mwwdpid) {
	Error ("Watchdog died");
	continue;
     };

    /* since sigs aren't queued we may get "merged" sigs */
    if (sigchldcount > 0)
      sigchldcount--;

    S = smgrServerFuneral(pid);
    if (S == NULL) {
      Error("got a sigchld for pid %d which is not one of my servers", pid);
      continue;
    };

    Info("Server %s in group %s exited with exitcode %d", 
	  S->name, S->group->name, WEXITSTATUS(status));
    
    /* don't restart if we're in a general shutdown */
    if (ipcmain->status == MWSHUTDOWN) continue;
    
    /* if active is less than expected, start anew, remember that if
       StopServer was called of the smgr stopped an idle server, we're
       still called to clean up. */

    if (S->active < S->started) {

      /* if the last time aninstance died is less than 2 secs we
	 assume the server failed to start an we postpone restart
	 until the next task */
      if ( (time(NULL) - S->lastdeath) > 2) 
	smgrExecServer(S) ;
    };
    S->lastdeath = time(NULL);    
  } 

  if (pid == -1) {
    Error("waidpid returned -1 errno = %d", errno);
    return -1;
  };
  
  /* if no pid waiting */
  if (pid == 0) return 0;
  
  /* interrupted */
  if ( (pid == -1) && ( (errno == ERESTART) || (errno == EINTR) ) ) return 0;
  
  Error("WE CAN'T GET HERE %s:%d", __FILE__, __LINE__);
  
  return -1;
}


/************************************************************************
 * Here are the function dealing with the task. mwd main call us every
 * whatever smgrTask returns.
 ************************************************************************/
    
int smgrTask(PTask pt) 
{
  int i;
  struct ServerGroup * SG;
  struct Server * S;

  DEBUG("smgrtask() beginning");

  i = _mwsystemstate();
  if (i != 0) {
    DEBUG("system is not in ready state, we don't try to restart any servers.");
    return 0;
  };

  for (SG = grouproot; SG != NULL; SG = SG->next) {
    DEBUG("  GROUP %s autoboot=%d", SG->name, SG->autoboot);
    
    for(S = SG->serverlist; S != NULL; S = S->next) {
      DEBUG("    SERVER %s autoboot=%d exec=%s ", 
	     S->name, S->autoboot, S->exec);
      DEBUG("       started=%d active=%d booted=%d ", 
	     S->started, S->active, S->booted);
      DEBUG("       min=%d max=%d", 
	     S->start_min, S->start_max);

      if (S->booted) {
	if ((time(NULL) - S->lastdeath) > 5)
	  for (i = 0; i < (S->started - S->active); i++) {
	    Info("    Task starting an instance of %s/%s", 
		  SG->name, S->name);
	    smgrExecServer(S);    
	  };
      };
    };
  };

  DEBUG("smgrtask() ending.");

  return 0;
};

/* called at startup in mwd, after all initalization is done, but
   before first smgrTask() */
void smgrAutoBoot(void)
{
  struct ServerGroup * SG;
  struct Server * S;
  
  DEBUG("Autoboot inprogress");
  for (SG = grouproot; SG != NULL; SG = SG->next) {
    DEBUG("  GROUP %s autoboot=%d", SG->name, SG->autoboot);
    if (SG->autoboot != 0) {
      for(S = SG->serverlist; S != NULL; S = S->next) {
	DEBUG("    SERVER %s autoboot=%d ", 
	       S->name, S->autoboot);
	if (S->autoboot != 0)
	  smgrBootServer(SG->name, S->name);    
	
      };
    };
  };
  DEBUG("Autoboot complete");
  return;
};


/************************************************************************
 * Here are the functions dealing with handling request to the
 * .mwsrvmgr service.
 ************************************************************************/

static void oc_list(urlmap * reqmap)
{
  char * group;
  char * repbuf;
  urlmap * replymap = NULL;
  Server * S;
  ServerGroup * SG;
  int moreflag = 1, rc, fullflag = 0, lastserver = 0;
  char * full;

  
  group = urlmapgetvalue(reqmap, "group");

  /* there are some hairy calc here to see if we sending the last
     reply.  I would be easier to send one extra reply at the end, but
     there is no use ful info to send at that point. */
  full = urlmapgetvalue(reqmap, "full");
  if ( (full != NULL) && (strcasecmp("no", full) != 0) ) fullflag = 1;

  DEBUG("Got a list commend to " MWSRVMGR " service, group = %s",
	 NONULL(group));
  
  for (SG = grouproot; SG != NULL; SG = SG->next) {
    if (group == NULL) goto match;
    if (group && SG->name && (strcmp(group, SG->name) == 0)) goto match;   
    continue;

  match:
    for (S = SG->serverlist; S != NULL;  S = S->next) {
      replymap = urlmapadd(NULL, "opcode", "list");
      replymap = urlmapadd(replymap, "group", SG->name);
      replymap = urlmapadd(replymap, "server", S->name);
      replymap = urlmapaddi(replymap, "active", S->active);
      replymap = urlmapaddi(replymap, "booted", S->booted);
      repbuf = urlmapencode(replymap);
      strcat(repbuf, "\r\n");

      if ( (S->next  == NULL) && ( (group == NULL) || (SG->next == NULL)))
	lastserver = 1;

      /* this is the last server, and we not requetsed full list, or
         there are no active on this server we set the moreflag to 0,
         and the next mwreply will be the last*/

      if ( (lastserver == 1) && ((fullflag == 0) || (S->active == 0)) ) moreflag = 0;
      DEBUG("if ( (lastserver(%d) == 1) && ((fullflag(%d) == 0) || (S->active(%d) == 0)) ) " 
	    "moreflag(%d) = 0;", lastserver, fullflag, S->active, moreflag);

      rc = mwreply(repbuf, 0, moreflag?MWMORE:MWSUCCESS, 0, 0);
      DEBUG("reply: %s, %s rc=%d", repbuf, moreflag?"more...":"", rc);
      
      free(repbuf);
      urlmapfree(replymap);
      
      if (fullflag) {
	int i;
	for (i = 0; i <  S->active; i++) {
	  replymap = urlmapadd(NULL, "opcode", "list");
	  replymap = urlmapadd(replymap, "group", SG->name);
	  replymap = urlmapadd(replymap, "server", S->name);
	  replymap = urlmapaddi(replymap, "instance", i+1);
	  replymap = urlmapaddi(replymap, "pid", S->activelist[i].pid);
	  switch (S->activelist[i].status) {
	  case DOWN:
	    replymap = urlmapadd(replymap, "status", "DOWN");
	    break;
	  case BOOTING:
	    replymap = urlmapadd(replymap, "status", "BOOTING");
	    break;
	  case RUNNING:
	    replymap = urlmapadd(replymap, "status", "UP");
	    break;
	  case FAILED:
	    replymap = urlmapadd(replymap, "status", "FAILED");
	    break;
	  default:
	    replymap = urlmapadd(replymap, "status", "UNKNOWN");
	    break;
	  };
	  repbuf = urlmapencode(replymap);
	  strcat(repbuf, "\r\n");

	  if ( (lastserver == 1) && (S->active -1 == i) ) moreflag = 0;	  
	  DEBUG("( (lastserver(%d) == 1) && (S->active(%d) -1 == i(%d)) ) moreflag = 0(%d)", 
	       lastserver, S->active, i, moreflag);
	  rc = mwreply(repbuf, 0, moreflag?MWMORE:MWSUCCESS, 0, 0);
	  DEBUG("reply: %s, %s rc=%d", repbuf, moreflag?"more...":"", rc);
	  
	  free(repbuf);
	  urlmapfree(replymap);
	}
      };
    };
  };
  return;
};

/*
 * combined start/stop func, if start is true, start, else stop 
 */
static void oc_startstop(int start, urlmap * reqmap)
{
  char * group;
  char * server;
  char * repbuf;
  urlmap * replymap = NULL;
  int pid;

  group = urlmapgetvalue(reqmap, "group");
  server = urlmapgetvalue(reqmap, "server");

  if (group == NULL) group = "";
  if (server == NULL) {
    Warning("Got a malformed start/stop request");
    repbuf = urlmapencode(reqmap);
    strcat(repbuf, "\r\n");
    mwreply (repbuf, 0, MWFAIL, EINVAL, 0);
  };


  Info("Got a %s command to " MWSRVMGR " service, group = %s server = %s",
	 start?"start":"stop", group, server);

  errno = 0;
  if (start)
    pid = smgrStartServer(group, server);
  else 
    pid = smgrStopServer(group, server);
  
  reqmap  = urlmapaddi(replymap, "pid", pid);
  repbuf = urlmapencode(reqmap);
  strcat(repbuf, "\r\n");

  mwreply (repbuf, 0, pid==-1?MWFAIL:MWSUCCESS, errno, 0);

  return;
};

static void oc_info(urlmap * reqmap)
{
  char * group;
  char * server;
  char * pid;
  char * repbuf;
  urlmap * replymap = NULL;
  Server * S;
  ServerGroup * SG;
  int moreflag = 1, rc;

  group = urlmapgetvalue(reqmap, "group");
  server = urlmapgetvalue(reqmap, "server");

  DEBUG("Got an info command to " MWSRVMGR " service, group = %s server = %s",
	 NONULL(group), server?server:"(NULL)");

  return;
};


/* TODO: no error indication in reply shoudn't we return the pids or n started? */

static void oc_boot_shutdown(int boot, urlmap * reqmap)
{
  char * group;
  char * server;
  char * repbuf;
  urlmap * replymap = NULL;
  Server * S;
  ServerGroup * SG;
  int rc;

  group = urlmapgetvalue(reqmap, "group");
  server = urlmapgetvalue(reqmap, "server");

  Info("Got a %s command to " MWSRVMGR " service, group = %s server = %s",
	 boot?"boot":"shutdown", NONULL(group), NONULL(server));
  

  if (server) {
    if (boot) {
      rc = smgrBootServer(group, server);
    } else {
      rc = smgrShutdownServer(group, server);
    };
    replymap = urlmapadd(NULL, "opcode", boot?"boot":"shutdown");
    replymap = urlmapadd(replymap, "group", SG->name);
    replymap = urlmapadd(replymap, "server", S->name);
    repbuf = urlmapencode(replymap);
    strcat(repbuf, "\r\n");
    urlmapfree(replymap);
    replymap = NULL;
    mwreply(repbuf, 0, rc==0?MWSUCCESS:MWFAIL, 0, 0);
    free(repbuf);
    return;
  };
  
  for (SG = grouproot; SG != NULL; SG = SG->next) {
    if (group == NULL) goto match;
    if (group && SG->name && (strcmp(group, SG->name) == 0)) goto match;   
    DEBUG("boot/shut skipping group %s", NONULL(group));

    continue;
    
  match:
    for (S = SG->serverlist; S != NULL;  S = S->next) {
      replymap = urlmapadd(NULL, "opcode", boot?"boot":"shutdown");
      replymap = urlmapadd(replymap, "group", SG->name);
      replymap = urlmapadd(replymap, "server", S->name);

      if (boot) {
	smgrBootServer(SG->name, S->name);
      } else {
	smgrShutdownServer(SG->name, S->name);
      };

      /* exit code? */
      repbuf = urlmapencode(replymap);
      strcat(repbuf, "\r\n");
      urlmapfree(replymap);
      replymap = NULL;
      mwreply(repbuf, 0, MWMORE, 0, 0);
      free(repbuf); 
    }

  }
  DEBUG("boot/shut completed");
  repbuf = urlmapencode(reqmap);
  strcat(repbuf, "\r\n");
  mwreply(repbuf, 0, MWSUCCESS, 0, 0);
  free(repbuf); 
  
  return;
};


int  smgrCall(mwsvcinfo * svcinfo)
{
  char * opcode;
  urlmap * reqmap = NULL;

  if (svcinfo->datalen <= 0) {
    mwreturn(svcinfo->data, svcinfo->datalen, MWFAIL, ENOMSG);
  };

  reqmap =  urlmapdecode(svcinfo->data);
  if (reqmap == NULL) {
    mwreturn(svcinfo->data, svcinfo->datalen, MWFAIL, ENOMSG);
  };

  opcode = urlmapgetvalue(reqmap, "opcode");
  if (opcode == NULL) {
    urlmapfree(reqmap);
    mwreturn(svcinfo->data, svcinfo->datalen, MWFAIL, EBADRQC);
  };

  if (strcasecmp(opcode, "list") == 0) {
    oc_list (reqmap);
  } else if (strcasecmp(opcode, "start") == 0) {
    oc_startstop (1, reqmap);
  } else if (strcasecmp(opcode, "stop") == 0) {
    oc_startstop (0, reqmap);
  } else if (strcasecmp(opcode, "info") == 0) {
    oc_info (reqmap);
  } else if (strcasecmp(opcode, "boot") == 0) {
    oc_boot_shutdown  (1, reqmap);
  } else if (strcasecmp(opcode, "shutdown") == 0) {
    oc_boot_shutdown (0, reqmap);
  } else {
    urlmapfree(reqmap);
    mwreturn(svcinfo->data, svcinfo->datalen, MWFAIL, EBADRQC);
  };
    
  return 0;
};




