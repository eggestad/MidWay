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
 * Revision 1.1  2000/03/21 21:04:21  eggestad
 * Initial revision
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

struct ipcmaininfo * ipcmain = NULL;

static int extended = 0;
#define NORMALPROMPT "MWADM> "
#define EXTDPROMPT   "MWADM+> "
static char * prompt = NORMALPROMPT;
static int echo = 1;
static int autorepeat = 0;

static int ipckey;

static int sigflag = 0;

static char * url = NULL;

/* the functions that implement the commands. They calling convention is based on 
   The UNIC convention from exec() to programs.
   The basis is for this use is:
   - other programs than mwadm may call them
   - it is simple to convert a commandline from readlin() to thsi by filling inn \0 for 
   white space, and just adding a pointer array.
   - a single command can be given on mwadm arguments.
*/
int info(int, char **);
int boot(int, char **);
int clients(int, char **);
int servers(int, char **);
int services(int, char **);
int heapinfo(int, char **);
int dumpipcmain(int, char **);
int call(int, char **);
int quit(int, char **);
int toggleRandD(int, char **);
int attach(int, char **);
int detach(int, char **);
int help(int, char **);
int shutdown(int, char **);


int usage(char *);

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
  { "clients",  clients,     0, "clients", "Lists attached clients" },
  { "servers",  servers,     0, "servers", "Lists attached servers" },
  { "services", services,    0, "services", "List provided services." },
  { "boot",     boot,        0, "boot  [-- [any used for mwd]]", "Boot the mwd" },
  { "shutdown", shutdown,    0, "shutdown", "Shutdown the mwd" },
  { "buffers",  heapinfo,    1, "buffers",    "prints out info on the shm buffer area"},
  { "help",     help,        0, "help [command]", 
    "list available commands and gives online help on spesific commands"},
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

void init_readline()
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
  int ipckey;
  char durl [1024];
    
  if (url == NULL) {
    ipckey = getuid();
    sprintf (durl, "ipc://%d", ipckey);
    url = durl;
  };
  rc = mwattach(url, "mwadm", NULL, NULL, 0 );
  printf("mwattach on url %s returned %d\n", url, rc);
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
  int start, end;
  char * token = NULL, ** argv = NULL;
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
  printf("commandline is %d bytes long and has %d args\n", len, argc);

  argv = malloc((argc+1)*sizeof(char *));
  intoken = 0;
  tokencount = 0;
  for (i = 0; i < len; i++) {
    if ((!intoken) && (arglist[i] != '\0')) {
      argv[tokencount++] = &arglist[i];
      intoken = 1;
      printf (" arg %d: %s\n", tokencount-1, &arglist[i]);
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

  
main(int argc, char ** argv)
{
  char * thiscommand, * lastcommand = NULL, * token;
  int option, rc, i;

  extern char *optarg;
  extern int optind, opterr, optopt;

  while ((option = getopt(argc, argv, "dRA:")) != EOF) {
    
    switch (option) {
      
    case 'd':
      mwsetloglevel(MWLOG_DEBUG);
      mwlog (MWLOG_DEBUG, "mwadm client starting");
      break;

    case 'R':
      toggleRandD(0, NULL);
      break;

    case 'A':
      url = optarg;
      break;

    case '?':
      fprintf (stderr, "usage: %s [-d] [-R] [-A URL] [command [args ..]]\n", argv[0]);
      exit(-1);
    }
  }

  /* spesifically we don't want INT (^C) to abort the program. */
  signal(SIGTERM, sig_handler);
  signal(SIGINT, sig_handler);

  mwsetlogprefix("./mwlog");
  attach(0, NULL);
  
  printf ("optind = %d argc = %d\n", optind, argc);
  for (i = optind; i < argc; i++) {
    printf("argv[%d] = \"%s\"\n", i, argv[i]);
  };

  if (optind < argc) {
    fprintf(stderr, "got an commandline command, not running interactivly\n");
    rc = exec_command(argc - optind, &argv[optind]); 
    detach(0, NULL);
    exit(rc);
  }
  /* if interactive */
  init_readline();

  while(1) {

    
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
    
    parse_commandline(thiscommand);

    if (sigflag) {
      printf("\n");
      sigflag = 0;
    }

    free(thiscommand);
  };
  
  detach(0, NULL);
};
