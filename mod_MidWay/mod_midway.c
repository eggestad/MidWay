/*
  MidWay Apache Module
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
 * Revision 1.2  2000/09/24 14:02:53  eggestad
 * Default URL fix
 *
 * Revision 1.1.1.1  2000/03/21 21:04:34  eggestad
 * Initial Release
 *
 *
 */

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_main.h"
#include "util_script.h"
#include "util_md5.h"

#include <MidWay.h>
 
module MODULE_VAR_EXPORT midway_module;

typedef struct {
  char *url;
  char *username;
  char *password;
} midway_server_config;

midway_server_config * module_config = NULL;

static int attached = 0;

/* Configuration fuctions.
   So far we have only two server wide config statemenats in addition to the handlers.

   midwayurl url                         which sets the URL, 
   midwayuserpass username [passowrd]    in case the instance require login.

   We need to have location overrides in order to be able to connect to 
   multiple MW instance.
   
   Future problem.
*/   
static void *midway_create_server_config(pool *p, server_rec *s)
{
  midway_server_config *cfg = 
    (midway_server_config *)ap_pcalloc(p, sizeof(midway_server_config));

  fprintf(stderr, "X\n");
  fflush(stderr);

  cfg->url = NULL;
  cfg->username = NULL;
  cfg->password = NULL;
  module_config = cfg;
  return (void *)cfg;
}

static void *midway_merge_server_config (pool *p, void *basev, void *addv)
{
  midway_server_config *new = 
    (midway_server_config *)ap_pcalloc (p, sizeof(midway_server_config));
  midway_server_config *base = (midway_server_config *)basev;
  midway_server_config *add = (midway_server_config *)addv;
    
  new->url = add->url ?
    add->url : base->url;

  new->username = add->username ?
    add->username : base->username;

  new->password = add->password ?
    add->password : base->password;

  module_config = new;
  return (void*)new; 
}

static const char *midway_url(cmd_parms *parms, void *mconfig,
			      char *url) 
{
  midway_server_config *cfg = module_config;

  cfg->url = (char *)ap_pstrdup(parms->pool, url);
  ap_log_error(APLOG_MARK, APLOG_INFO, NULL, "MidWay URL %s", cfg->url);
  return NULL;
}

static const char *midway_user(cmd_parms *parms, void *mconfig,
			       char *username, char * passwd) 
{
  midway_server_config *cfg = module_config;

  cfg->username = (char *)ap_pstrdup(parms->pool, username);
  if (passwd != NULL) 
    cfg->password = (char *)ap_pstrdup(parms->pool, passwd);

  ap_log_error(APLOG_MARK, APLOG_DEBUG, NULL, "MidWay username is %s and password %s", 
	       cfg->username, cfg->password?cfg->password:"(NULL)");
  return NULL;
}

static command_rec midway_cmds[] =
{
  {
    "MidWayURL",              /* directive name */
    midway_url,            /* config action routine */
    NULL,                   /* argument to include in call */
    RSRC_CONF,             /* where available */
    TAKE1,                /* arguments */
    "The URL for the midway instance, like ipc:500"
                                /* directive description */
  },
  {
    "MidWayUserPass",              /* directive name */
    midway_user,            /* config action routine */
    NULL,                   /* argument to include in call */
    RSRC_CONF,             /* where available */
    TAKE12,                /* arguments */
    "Username with optional passwd (username [password])"
                                /* directive description */
  },
  {NULL}
};
/********************************************************
 * End of config fouction, now we have functions 
 * that deal with request handling
 ********************************************************/
  
/* routines for reading POST data */
static int util_read(request_rec *r, const char **rbuf)
{
  int rc = OK;
    
  if ((rc = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR))) {
    return rc;
  }
    
  if (ap_should_client_block(r)) {
    char argsbuffer[HUGE_STRING_LEN];
    int rsize, len_read, rpos=0;
    long length = r->remaining;
    *rbuf = ap_pcalloc(r->pool, length + 1); 
      
    ap_hard_timeout("util_read", r);
      
    while ((len_read =
	    ap_get_client_block(r, argsbuffer, sizeof(argsbuffer))) > 0) {
      ap_reset_timeout(r);
      if ((rpos + len_read) > length) {
	rsize = length - rpos;
      }
      else {
	rsize = len_read;
      }
      memcpy((char*)*rbuf + rpos, argsbuffer, rsize);
      rpos += rsize;
    }
      
    ap_kill_timeout(r);
  }
    
  return rc;
}


/* here's the content handlers.
   We have three, one for handling url-encoded data, one for raw data, and
   one that will try to automatically determine which encoding are used.*/

/* we are now passing pretty much everything straight thru.
   If the return buffer don't start with content-type
   We set it to x-octet-stream */

#define OCTET_ENCTYPE "application/x-octet-stream"
static int midway_raw_handler(request_rec *r) 
{
  midway_server_config *cfg = 
    ap_get_module_config(r->server->module_config, &midway_module);     
 
  char * svc, *data;
  const char *type;
  int rc; 
  char * rbuffer = NULL, *clen;
  int len, rlen = 0, appcode;


  switch (r->method_number) {

  case M_GET:
    /* unescape args on url. */
    /* this don't allow \0 in input data */
    if (ap_unescape_url(r->args) != OK) 
      return HTTP_BAD_REQUEST;
    data = r->args;
    break;

  case M_POST:
    type = ap_table_get(r->headers_in, "Content-Type");
    ap_log_error(APLOG_MARK, APLOG_ERR, r->server, 
		 "content type %s", type);
    
    if(strcasecmp(type, OCTET_ENCTYPE) != 0) {
      return DECLINED;
    }
    rc = util_read(r, (const char**) &data);
    ap_log_error(APLOG_MARK, APLOG_ERR, r->server, 
		 "read from post %s, rc = %d", data, rc);

    if (rc != OK) return rc;
    break;
    
  default:    
    return HTTP_METHOD_NOT_ALLOWED;
  };

  svc = r->path_info;
  if (svc[0] =='/') svc = &svc[1];
  
  ap_log_error(APLOG_MARK, APLOG_ERR, r->server, 
	       "about to mwcall(%s, %s,...)", svc, data);

  rc = mwcall(svc, data, 0, &rbuffer , &rlen, &appcode, 0);
  switch(rc) {

  case(0):
    break;
  case -2:
    return HTTP_SERVICE_UNAVAILABLE;

  default:
    ap_log_error(APLOG_MARK, APLOG_ERR, r->server, 
		 "mwcall(%s, %s,...) failed with %d", svc, data, rc);
    return HTTP_INTERNAL_SERVER_ERROR;

  };
  
  if (ap_strcmp_match(rbuffer, "Content-type") != 0) {
    r->content_type = "application/x-octet-stream";
    ap_send_http_header(r);
  };
  
  ap_rputs(rbuffer, r);
  return OK;
};

    
#define URL_ENCTYPE "application/x-www-form-urlencoded"
static int midway_urlencoded_handler(request_rec *r) 
{
  midway_server_config *cfg = 
    ap_get_module_config(r->server->module_config, &midway_module);     
  
  char * svc, *data;
  const char *type;
  int rc; 
  char * rbuffer = NULL, *clen;
  int len, rlen = 0, appcode;

  switch (r->method_number) {

  case M_GET:
    /* unescape args on url. */
    /* this don't allolow \0 in input data */
    if (ap_unescape_url(r->args) != OK) 
      return HTTP_BAD_REQUEST;
    data = r->args;
    break;

  case M_POST:
    type = ap_table_get(r->headers_in, "Content-Type");
    if(strcasecmp(type, URL_ENCTYPE) != 0) {
      return DECLINED;
    }
    rc = util_read(r, (const char**) &data);
    if (rc != OK) return rc;
    break;
    
  default:    
    return HTTP_METHOD_NOT_ALLOWED;
  };

  svc = r->path_info;
  if (svc[0] =='/') svc = &svc[1];
  

  rc = mwcall(svc, data, 0, &rbuffer , &rlen, &appcode, 0);
  switch(rc) {

  case(0):
    break;
  case -2:
    ap_log_error(APLOG_MARK, APLOG_ERR, r->server, 
		 "MidWay service %s don't exist.", svc);

    return HTTP_SERVICE_UNAVAILABLE;

  default:
    ap_log_error(APLOG_MARK, rc?APLOG_ERR:APLOG_DEBUG, r->server, 
		 "mwcall(%s, %s,...) failed with %d", svc, r->args);
    return HTTP_INTERNAL_SERVER_ERROR;

  };
  
  if (ap_strcmp_match(rbuffer, "Content-type") != 0) {
    r->content_type = "text/html";
    ap_send_http_header(r);
  };

  ap_rputs(rbuffer, r);
  return OK;
};

static int midway_handler(request_rec *r) 
{
  midway_server_config *cfg = 
    ap_get_module_config(r->server->module_config, &midway_module);     
  const char* hostname;
 
  char * url, * u, *p, * svc;
  int rc; 
  char * rbuffer = NULL;
  int rlen = 0, appcode;


  if (cfg->url == NULL) url = "(NULL)";
  else  url = cfg->url;
  if (cfg->username == NULL) u = "(NULL)";
  else  u = cfg->username;
  if (cfg->password == NULL) p = "(NULL)";
  else  p = cfg->password;

  r->content_type = "text/html";
  ap_send_http_header(r);
  hostname = ap_get_remote_host(r->connection, 
				r->per_dir_config, REMOTE_NAME);
 
  ap_rputs("<HTML>\n", r);
  ap_rputs("<HEADER>\n", r);
  ap_rputs("<TITLE>midway There</TITLE>\n", r);
  ap_rputs("</HEADER>\n", r);
  ap_rputs("<BODY>\n", r);
  ap_rprintf(r, "<H1>midway %s</H1>\n", hostname);

  svc = r->path_info;
  if (svc[0] =='/') svc = &svc[1];
  rc = mwcall(svc, "EGGESTAD", 0, &rbuffer , &rlen, &appcode, 0);
  ap_rprintf(r, "call returned %s<p>", rbuffer);

  if (rc < 0) {
    ap_rprintf(r, "call returned %s<p>", rbuffer);
  } else {
    ap_rprintf(r, "call failed with %d<p>", rc);
  };
  ap_rprintf(r, "unparsed_uri %s<p>", r->unparsed_uri);
  ap_rprintf(r, "uri %s<p>", r->uri);
  ap_rprintf(r, "filename %s<p>", r->filename);
  ap_rprintf(r, "path_info %s<p>", r->path_info);
  ap_rprintf(r, "args %s<p>", r->args);

  ap_rprintf(r, "method %s<p>", r->method);
  ap_rprintf(r, "content_type %s<p>", ap_table_get(r->headers_in, "Content-type"));
  ap_rprintf(r, "content_encoding %s<p>", r->content_encoding);
  ap_rputs("</BODY>\n", r);
  ap_rputs("</HTML>\n", r);

  return OK;
}

/**************************************************************
 * Startup functions.
 * when a child starts we try to attach the midway instance.
 *************************************************************/

void child_init(server_rec *s, pool *p)
{
  midway_server_config *cfg = 
    ap_get_module_config(s->module_config, &midway_module);     

  int rc = 0;

  ap_log_error(APLOG_MARK, rc?APLOG_ERR:APLOG_DEBUG, s, 
	       "goint to mwattach(%s, %s, %s, %s,...)", 
	       cfg->url, "Apache/mod_midway", cfg->username,  cfg->password);

  rc = mwattach(cfg->url, "Apache/mod_midway", 
		cfg->username, cfg->password, 0L);

  if (rc == 0) attached = 1;

  ap_log_error(APLOG_MARK, rc?APLOG_ERR:APLOG_DEBUG, s, 
	       "mwattach(%s, %s, %s, %s,...) returned %d", 
	       cfg->url, "Apache/mod_midway", cfg->username, cfg->password,rc);
}


void child_exit(server_rec *s, pool *p)
{
  midway_server_config *cfg = 
    ap_get_module_config(s->module_config, &midway_module);     
  int rc;

  rc = mwdetach();

  ap_log_error(APLOG_MARK, APLOG_DEBUG, s, "mwdetach() returned %d", rc);
};


/* future cookie handling.
 * Since it is a pain, and unatural to handle sessions from a MW service, 
 * we will provide a session consept here, along with authenticaion. Some day
 * /
table *util_parse_cookie(request_rec *r)
{
  const char *data = ap_table_get(r->headers_in, "Cookie");
  table *cookies;
  const char *pair;
  if(!data) return NULL;

  cookies = ap_make_table(r->pool, 4);
  while(*data && (pair = ap_getword(r->pool, &data, ';'))) {
    const char *key, *value;
    if(*data == ' ') ++data;
    key = ap_getword(r->pool, &pair, '=');
    while(*pair && (value = ap_getword(r->pool, &pair, '&'))) {
      ap_unescape_url((char *)value);
      ap_table_add(cookies, key, value);
    }
  }

  return cookies;
}

/* Make the name of the content handler known to Apache */
static handler_rec midway_handlers[] =
{
  {"midway-handler", midway_handler},
  {"midway-raw-handler", midway_raw_handler},
  {"midway-urlencoded-handler", midway_urlencoded_handler},
  {NULL}
};

/* Tell Apache what phases of the transaction we handle */
module MODULE_VAR_EXPORT midway_module =
{
  STANDARD_MODULE_STUFF,
  NULL,               /* module initializer                 */
  NULL,               /* per-directory config creator       */
  NULL,               /* dir config merger                  */
  midway_create_server_config,      /* server config creator              */
  midway_merge_server_config,    /* server config merger               */
  midway_cmds,         /* command table                      */
  midway_handlers,     /* [7]  content handlers              */
  NULL,               /* [2]  URI-to-filename translation   */
  NULL,               /* [5]  check/validate user_id        */
  NULL,               /* [6]  check user_id is valid *here* */
  NULL,               /* [4]  check access by host address  */
  NULL,               /* [7]  MIME type checker/setter      */
  NULL,               /* [8]  fixups                        */
  NULL,               /* [9]  logger                        */
  NULL,               /* [3]  header parser                 */
  child_init,         /* process initialization             */
  child_exit,         /* process exit/cleanup               */
  NULL                /* [1]  post read_request handling    */
};
