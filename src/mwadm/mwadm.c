/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

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
 * Revision 1.11  2004/05/31 19:48:33  eggestad
 * main API changes
 *
 * Revision 1.10  2003/07/06 18:59:56  eggestad
 * introduced a table api for commands.c to return data in
 *
 * Revision 1.9  2003/06/12 07:22:55  eggestad
 * fix for loglevel option
 *
 * Revision 1.8  2003/06/05 21:52:56  eggestad
 * commonized handling of -l option
 *
 * Revision 1.7  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.6  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.5  2001/10/03 22:56:12  eggestad
 * added multicast query
 * added view of gateway table
 *
 * Revision 1.4  2001/08/29 17:57:59  eggestad
 * had declared a shutdown() function that collided with the syscall, renamed to cmd_shutdown
 *
 * Revision 1.3  2000/11/29 23:32:04  eggestad
 * mwadm created it own default ipc url
 *
 * Revision 1.2  2000/07/20 19:43:31  eggestad
 * CVS keywords were missing
 *
 * Revision 1.1.1.1  2000/03/21 21:04:21  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
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

#include "dtbl.h"
#include "commands.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

struct ipcmaininfo * ipcmain = NULL;

static int extended = 0;
#define NORMALPROMPT "MWADM> "
#define EXTDPROMPT   "MWADM+> "
static char * prompt = NORMALPROMPT;
static int echo = 0;
static int autorepeat = 0;

static int sigflag = 0;

static char * url = NULL;

int usage(char *);

struct command {
  char * commandname;
  int (*commandfunc) (int, char **);
  int extended;
  char * usage;
  char * doc;
};


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

struct command  commands[] = 
{
  { "info",     info,        0, "info",    "prints out IPC maininfo data"},
  { "clients",  clients,     0, "clients", "Lists attached clients" },
  { "servers",  servers,     0, "servers", "Lists attached servers" },
  { "services", services,    0, "services", "List provided services." },
  { "gateways", gateways,    0, "gw", "List gateways." },
  { "boot",     boot,        0, "boot  [-- [any used for mwd]]", "Boot the mwd" },
  { "shutdown", cmd_shutdown,    0, "shutdown", "Shutdown the mwd" },
  { "buffers",  heapinfo,    1, "buffers",    "prints out info on the shm buffer area"},
  { "help",     help,        0, "help [command]", 
    "list available commands and gives online help on spesific commands"},
  { "query",     query,        0, "query",    "perform a multicast discovery of running instances"},
  { "quit",     quit,        0, "quit",    "exits mwadm"},
  { "q",        quit,        0, "quit",    "exits mwadm"},
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
  NB: On som e command we have completions on more than arg, that is foremost on those
  command that require completin on a server, client or service names. 
  e.g. call.
  However things like help have hardcoded what the first arg may be.
  THese hardcodings must be closely synced with commands.c */

char * command_generator(char * text, int state)
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
  
char ** command_completion(char * text, int start, int end)
{
  char ** matches = NULL;

  if (start != 0) return NULL;

  matches = completion_matches(text,command_generator);
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
  };
  return 0;
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
      printf ("%s\n", commands[i].usage);
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

  while ((option = getopt(argc, argv, "l:RA:")) != EOF) {
    
    switch (option) {
      
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
