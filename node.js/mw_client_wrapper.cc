#include <iostream>
#include <string>
#include <map>

#include <node_api.h>
#include <uv.h>

#include <assert.h>

#include <stdio.h>

#include <MidWay.h>
#include "mw_wrapper.h"

namespace MidWay {

 
   /*************************************
    *
    *  Actuall wrapper funcs 
    *
    *************************************/


   /**
    * AsyncCall
    * 
    */ 
   napi_value ACall(napi_env env, napi_callback_info info) {

      mwlog(MWLOG_DEBUG2,  (char*) "Beginning Acall");

      char * url = NULL;

      char * data = NULL;
      size_t datalen = 0;
      int flags = 0;
      
      napi_status status;
      napi_value rv;

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      size_t urllen = MWMAXSVCNAME;
      char urlbuf[urllen];
      
      if (argc == 0) {
	 napi_throw_type_error(env, NULL, "missing arguments servicename must be present." );
	 return NULL;
      }
 
	 
      napi_valuetype type;
      napi_typeof(env, args[0], &type);
      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[0], urlbuf, urllen, &rlen);
	 CHECK_STATUS;
	 url = urlbuf;
      } else {
	 napi_throw_type_error(env, NULL, "acall got illegal type on first argument." );
	 status = napi_create_int32(env, 0, &rv);
	 assert(status == napi_ok);
	 return rv;
      }

      if (argc >= 2) {

	 napi_typeof(env, args[1], &type);

      if (type == napi_string) {
	 size_t rlen;
	 status = napi_get_value_string_utf8(env, args[1], NULL, 0,  &rlen);
	 if (status != napi_ok) {
	    mwlog(MWLOG_ERROR, (char *) "status = %d", status);
	    napi_throw_type_error(env, NULL, "unable to get data length." );
	 }
	 mwlog(MWLOG_DEBUG2, (char *) "datalen = %ld", rlen);
	 rlen++;
	 data = (char *) mwalloc(rlen);

	 status = napi_get_value_string_utf8(env, args[1], data, rlen,  &datalen);
	 CHECK_STATUS;
	 
      } else if (type == napi_undefined) {
	 // no  data
      } else {
	 napi_throw_type_error(env, NULL, "data must be a string or undefined." );
	 return NULL;
      }
	 
      mwlog(MWLOG_DEBUG2, (char*) "calling mwacall with %s %s %d", url, data, datalen, flags);

      int rc = mwacall(url, data, flags);
      
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending acall returning %d", rc);
      return rv;
   };

  /**
    * Fetch
    * 
    */ 
   napi_value Fetch(napi_env env, napi_callback_info info) {

      mwlog(MWLOG_DEBUG2,  (char*) "Beginning Fetch");

      char * url = NULL;
      char * name = NULL;
      int flags = MWSERVER;
      int handle = 0;
      
      
      napi_status status;
      napi_value rv;

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;

      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      size_t urllen = 1000;
      char urlbuf[urllen];
      size_t namelen = 1000;
      char namebuf[urllen];
      
      if (argc == 1) {
	 
	 napi_valuetype type;
	 napi_typeof(env, args[0], &type);
	 if (type == napi_string) {
	    size_t rlen;
	    status = napi_get_value_string_utf8(env, args[0], urlbuf, urllen, &rlen);
	    CHECK_STATUS;
	    url = urlbuf;
	 } else if (type == napi_object) {
	    bool hasprop;
	    napi_value obj = args[0], key, val;

	    valFromString(env, "url", &key);
	    status = napi_has_property(env, obj, key, &hasprop);	   
	    CHECK_STATUS;
	    if (hasprop) {
	       size_t rlen;
	       status = napi_get_property(env, obj, key, &val);
	       CHECK_STATUS;
	       status = napi_get_value_string_utf8(env, val, urlbuf, urllen, &rlen);
	       CHECK_STATUS;
	       url = urlbuf;
	    };
	    
	    valFromString(env, "name", &key);
	    status = napi_has_property(env, obj, key, &hasprop);	   
	    CHECK_STATUS;
	    if (hasprop) {
	       size_t rlen;
	       status = napi_get_property(env, obj, key, &val);
	       CHECK_STATUS;
	       status = napi_get_value_string_utf8(env, val, urlbuf, urllen, &rlen);
	       CHECK_STATUS;
	       url = urlbuf;
	    }
	    
	 
	 } else if (type == napi_undefined) {
	    
	 } else if (type == napi_null) {
	       
	 } else {
	    napi_throw_type_error(env, NULL, "attach got illegal type on first argument." );
	    status = napi_create_int32(env, 0, &rv);
	    assert(status == napi_ok);
	    return rv;
	 }
      }
      mwlog(MWLOG_DEBUG2, (char*) "calling mwattach with %s %s %d", url, name, flags);

      int rc = mwattach(url, name, flags);
      
      status = napi_create_int32(env, rc, &rv);
      assert(status == napi_ok);

      mwlog(MWLOG_DEBUG2, (char*) "Ending Attach");
      return rv;
   };




}  // namespace MidWay
