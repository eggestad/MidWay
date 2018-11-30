/*
  MidWay
  Copyright (C) 2000-2004 Terje Eggestad

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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

/* needed for Gnu Readline lib */
#include <readline/readline.h>
#include <readline/history.h>

#include <MidWay.h>
#include <ipctables.h>

#include "mwadm.h"
#include "dtbl.h"
#include "commands.h"

struct ipcmaininfo * ipcmain = NULL;

static int extended = 0;
#define NORMALPROMPT "MWADM> "
#define EXTDPROMPT   "MWADM+> "
static char * prompt = NORMALPROMPT;
static int echo = 0;
static int autorepeat = 0;

static int sigflag = 0;

static char * url = NULL;

int dbgflag = 0;
int usage(char *);


int quit          (int argc, char ** argv);
int attach        (int argc, char ** argv);
int detach        (int argc, char ** argv);
int toggleRandD   (int argc, char ** argv);
int help          (int argc, char ** argv);
static int info          (int argc, char ** argv);
int boot          (int argc, char ** argv);
static int clients       (int argc, char ** argv);
static int servers       (int argc, char ** argv);
static int services      (int argc, char ** argv);
static int gateways      (int argc, char ** argv);
int heapinfo      (int argc, char ** argv);
int dumpipcmain   (int argc, char ** argv);
int call          (int argc, char ** argv);
int event         (int argc, char ** argv);
int subscribe     (int argc, char ** argv);
int unsubscribe   (int argc, char ** argv);
int query         (int argc, char ** argv);
int cmd_shutdown  (int argc, char ** argv);

int clients       (int argc, char ** argv)
{
   DTable dt;
   char * errstr;
   int rc;

   dt = dtbl_new(0);
   
   rc = do_clients(argc, argv, dt, &errstr);
   if (rc < 0) {
      printf ("%s\n", errstr);
      return rc;
   };

   dtbl_printf(dt, stdout);

   dtbl_delete(dt);
   return rc;
};

int servers(int argc, char ** argv)
{
   DTable dt;
   char * errstr;
   int rc;

   dt = dtbl_new(0);
   
   rc = do_servers(argc, argv, dt, &errstr);
   if (rc < 0) {
      printf ("%s\n", errstr);
      return rc;
   };

   dtbl_printf(dt, stdout);

   dtbl_delete(dt);
   return rc;
};

int services(int argc, char ** argv)
{
   DTable dt;
   char * errstr;
   int rc;

   dt = dtbl_new(0);
   
   rc = do_services(argc, argv, dt, &errstr);
   if (rc < 0) {
      printf ("%s\n", errstr);
      return rc;
   };

   dtbl_printf(dt, stdout);

   dtbl_delete(dt);
   return rc;
};

int gateways(int argc, char ** argv)
{
   DTable dt;
   char * errstr;
   int rc;

   dt = dtbl_new(0);
   
   rc = do_gateways(argc, argv, dt, &errstr);
   if (rc < 0) {
      printf ("%s\n", errstr);
      return rc;
   };

   dtbl_printf(dt, stdout);

   dtbl_delete(dt);
   return rc;
};

int info(int argc, char ** argv)
{
   DTable dt;
   char * errstr;
   int rc;

   dt = dtbl_new(2);
   
   rc = do_info(argc, argv, dt, &errstr);
   if (rc < 0) {
      printf ("%s\n", errstr);
      return rc;
   };

   dtbl_printf(dt, stdout);

   dtbl_delete(dt);
   return rc;
};

struct command {
  char * commandname;
  int (*commandfunc) (int, char **);
  int extended;
  char * usage;
  char * doc;
};

struct command  commands[] = 
{
  { "info",     info,        0, "info",    "prints out IPC maininfo data"},
  { "clients",  clients,     0, "clients|lc", "Lists attached clients" },
  { "lc",       clients,     0, "lc", "Lists attached clients" },
  { "servers",  servers,     0, "servers", "Lists attached servers" },
  { "lr",       servers,     0, "lr", "Lists attached servers" },
  { "services", services,    0, "services | ls", "List provided services." },
  { "ls",       services,    0, "ls", "List provided services." },
  { "gateways", gateways,    0, "gateways", "List gateways." },
  { "lg",       gateways,    0, "lg", "List gateways." },
  { "boot",     boot,        0, "boot  [-- [any used for mwd]]", "Boot the mwd" },
  { "shutdown", cmd_shutdown,    0, "shutdown", "Shutdown the mwd" },
  { "buffers",  heapinfo,    1, "buffers",    "prints out info on the shm buffer area"},
  { "help",     help,        0, "help [command]", 
    "list available commands and gives online help on spesific commands"},
  { "query",     query,        0, "query",    "perform a multicast discovery of running instances"},
  { "quit",     quit,        0, "quit",    "exits mwadm"},
  { "q",        quit,        0, "q",    "exits mwadm"},
  { "R&D",      toggleRandD, 0, "R&D",     "Toggle mwadm into R&D mode."},
  { "ipcmain",  dumpipcmain, 1, "ipcmain", 
    "dumps out the ipcmain structure from the main SHM segment"},
  { "attach",   attach,      0, "attach [url]", 
    "attaches mwadm as a client to MidWay and allows calls." }, 
  { "detach",   detach,      0, "detach",  "detaches mwadm as client"},
  { "call",     call,        1, "call servicename data", 
    "perform a mwcall, data string and reply use url % encoding" },
  { "event",    event,       0, "event eventname [data]", 
    "perform a mwevent" },
  { "subscribe", subscribe,  0, "subscribe [-grR] eventname", 
    "subscribe to a mwevent" },
  { "unsubscribe", unsubscribe,  0, "unsubscribe subscriptionid", 
    "unsubscribe to a mwevent" },
  { NULL ,      NULL,        0, NULL, NULL }, 
  NULL
};

int quit(int argc, char ** argv)
{
  mwdetach();
  exit(0);
};
/*
  Completions: We use readline extensivly here.
  NB: On some commands we have completions on more than arg, that is foremost on those
  command that require completin on a server, client or service names. 
  e.g. call.
  However things like help have hardcoded what the first arg may be.
  THese hardcodings must be closely synced with commands.c */

char * command_generator(const char * text, int state)
{
  static int index;
  static int len;
  char * name;
  if (state == 0) {
    index = 0;
    len = strlen(text);
  };

  while( (name = commands[index].commandname) != NULL) {
    index++;
    if (strncmp(name,text,len)== 0) return strdup(name);
  };
  return NULL;
};
  
char ** command_completion(const char * text, int start, int end)
{
  char ** matches = NULL;

  if (start != 0) return NULL;

  matches = rl_completion_matches( text, command_generator);
  return matches;
};

void init_readline(void)
{
  /*  rl_completion_entry_function = command_completion;*/
  rl_attempted_completion_function = command_completion;
};

/*
  Reseach and development operations.
  R&D are an extended mode with additional functions.
  the extende flag in the command struct spesifies what 
  operations are R&D. 
  info/print is internal info are also extended with additional data
  in R&D mode.
*/
   
int toggleRandD(int argc, char **argv)
{
  if (extended) {
    prompt = NORMALPROMPT;
    extended = 0;
    return 0;
  };
  prompt = EXTDPROMPT;
  extended = 1;
  return 1;
};

int attach(int argc, char ** argv)
{
  int rc;
    
  rc = mwattach(url, "mwadm", 0 );
  DEBUG("mwattach on url %s returned %d", url, rc);
  if (rc == 0) {
    ipcmain = _mw_ipcmaininfo();
  } else {
    printf ("System is not up\n");
    rc  = -1;
  };
  return rc;
}

int detach(int argc, char ** argv)
{
  ipcmain = NULL;
  return mwdetach();
}

int usage(char * command)
{
  int i = 0;
  printf ("\n Available commands\n");
  while(commands[i].commandname != NULL) {
    if (!extended && commands[i].extended == 1) {
      i++;
      continue;
    };
    if ( 
	(command == NULL)  || 
	((command != NULL) && (strcmp(command, commands[i].commandname) == 0) )
	)
       printf ("%-20s : %s\n", commands[i].usage, commands[i].doc);
    i++;
  } 
  printf ("\n");
  return 0;
};

int help(int argc, char ** argv)
{
  if (argc == 1) usage(NULL);
  else if (argc == 2) usage(argv[1]);
  else usage("help");
  return 0;
};
  
void sig_handler(int signo)
{
  sigflag = 1;
  return;
};

int exec_command(int argc, char ** argv)
{
  int i;

  i = 0;
  while(commands[i].commandname != NULL) {
    /*printf("testing to see if command %s is %s\n", argv[0], commands[i].commandname);*/
    if (strcmp(argv[0], commands[i].commandname) == 0) 
      return commands[i].commandfunc(argc, argv);
    i++;
  };
  usage(NULL);
  return 0;
};

int parse_commandline(char *arglist)
{
  int i = 0;
  int tokencount = 0, len, intoken = 0;
  char ** argv = NULL;
  int argc = 0;



  /* first we must know how many white space seperated tokens there are
     and we fill in while space with null chars.
     We can choose between doing two passes on the commandline, 
     or do realloc()'s on argv, I choose the former.*/

  /* we really should honor quotes and possibly wildcards */
  len = 0;
  while(arglist[len] != '\0') {
    if (isspace(arglist[len])) {
      arglist[len] = '\0';
      intoken = 0;
    } else {
      if (!intoken) { 
	intoken = 1;
	argc++;
      };
    }
    len++;
  };
  DEBUG("commandline is %d bytes long and has %d args", len, argc);

  argv = malloc((argc+1)*sizeof(char *));
  intoken = 0;
  tokencount = 0;
  for (i = 0; i < len; i++) {
    if ((!intoken) && (arglist[i] != '\0')) {
      argv[tokencount++] = &arglist[i];
      intoken = 1;
      DEBUG (" arg %d: %s", tokencount-1, &arglist[i]);
      continue;
    };
    if (intoken && (arglist[i] == '\0')) {
      intoken = 0;
      continue;
    };
  };
  argv[tokencount] = NULL;

  /* just checking for blank lines */ 
  if (tokencount == 0) return 0;
  return exec_command(argc, argv);
};

static void _usage(char * prog)
{
   fprintf (stderr, "usage: %s [-l loglevel] [-R] [-A URL] [command [args ..]]\n", prog);
   exit(-1);
};

int main(int argc, char ** argv)
{
  char * thiscommand, * lastcommand = NULL;
  int option, rc, i;
  int loglevel = MWLOG_INFO;
  extern char *optarg;
  extern int optind, opterr, optopt;

  while ((option = getopt(argc, argv, "dl:RA:")) != EOF) {
    
    switch (option) {
      
    case 'd':
       dbgflag = 1;
       break;

    case 'l':
       loglevel = _mwstr2loglevel(optarg);
       if (loglevel == -1) _usage(argv[0]);
       break;

    case 'R':
      toggleRandD(0, NULL);
      break;

    case 'A':
      url = optarg;
      break;

    case '?':
      _usage(argv[0]);
    }
  }

  /* spesifically we don't want INT (^C) to abort the program. */
  signal(SIGTERM, sig_handler);
  signal(SIGINT, sig_handler);

  mwopenlog(argv[0], "mwlog", loglevel);
  DEBUG("mwadm client starting");

  attach(0, NULL);
  
  DEBUG ("optind = %d argc = %d", optind, argc);
  for (i = optind; i < argc; i++) {
     DEBUG("argv[%d] = \"%s\"", i, argv[i]);
  };

  if (optind < argc) {
     DEBUG("got an commandline command, not running interactivly\n");
     rc = exec_command(argc - optind, &argv[optind]); 
     detach(0, NULL);
     exit(rc);
  }
  /* if interactive */
  init_readline();

  while(1) {

    mwrecvevents();
    thiscommand = readline(prompt);
    if (thiscommand == NULL) break;

    if (strcmp(thiscommand ,  "") == 0) {
      if (! autorepeat) continue;
      if (lastcommand == NULL) continue;
      thiscommand = strdup(lastcommand);
    } else {
      free(lastcommand); 
      lastcommand = strdup(thiscommand);
      add_history(thiscommand);
    };

    if (echo)
       printf ("%s\n", thiscommand);
    else 
       printf("\n");

    parse_commandline(thiscommand);

    if (sigflag) {
      printf("\n");
      sigflag = 0;
    }

    free(thiscommand);
  };
  
  detach(0, NULL);
};
