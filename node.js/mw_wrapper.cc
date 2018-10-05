
#include <node_api.h>
#include <assert.h>

#include <stdio.h>

#define CHECK_STATUS					\
   if (status != napi_ok) {				\
      napi_throw_error(env, "1", "N-API call failed");	\
      return undefinedval;				\
   }

namespace MidWay {


   static int JSValtoString(napi_env env, napi_value val, char * buf, size_t len) {
      	 napi_valuetype type;
	 napi_typeof(env, val, &type);
	 napi_status sts;
	 
	 switch(type)  {

	 case napi_undefined:
	    return snprintf(buf, len, "(undefined)");

	 case napi_null:
	    return snprintf(buf, len, "(null)");
	    
	 case napi_boolean:
	    bool b;
	    napi_get_value_bool(env, val, &b);
	    return snprintf(buf, len, val ? "true":"false");
	    
	 case napi_number:
	    int64_t i;
	    sts =  napi_get_value_int64(env, val, &i);
	    if (sts == napi_ok)
	       return snprintf(buf, len, "%ld", i);
	    double d;
	    sts =  napi_get_value_double(env, val, &d);
	    if (sts == napi_ok)
	       return snprintf(buf, len, "%f", d);
	    return snprintf(buf, len, "(some number)");
	    
	 case napi_string:
	    size_t rc;
	    napi_get_value_string_utf8(env, val, buf, len, &rc);
	    return rc;
	    
	 case napi_object:
	    return snprintf(buf, len, "[object]");
	   	    
	 case napi_function:
	    return snprintf(buf, len, "(function)");

	 case napi_external:
	    return snprintf(buf, len, "(external");

	 // case napi_bigint:
	 //    return snprintf(buf, len, "(bigint)");

	 default:
	    return snprintf(buf, len, "{unknown javascript type}");
	 }
   }

   
   napi_value undefinedval;

   
   napi_value log_to_midway(napi_env env, napi_callback_info info) {
      napi_status status;

      size_t argc = 100;
      napi_value args[argc];
      size_t len = 16000;
      char buf[len];
      int offset = 0;
      
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;
      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      for (uint32_t i = 0; i < argc; i++) {
	 printf("-- %d\n", offset);
	 offset += JSValtoString(env, args[i] , buf+offset, len -offset);
	 if (i == argc-1) {
	    buf[offset++] = '\0';
	    printf("%s\n", buf);
	 }
	 buf[offset++] = ' ';
      }
      return undefinedval;
   }

   napi_value Attach(napi_env env, napi_callback_info info) {
      napi_status status;
      napi_value world;

      napi_value args[10];
      size_t argc = 10;
      status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
      CHECK_STATUS;
      napi_value global;
      status = napi_get_global(env, &global);
      CHECK_STATUS;

      int i = 0;
      while(args[i] != undefinedval) {
	 napi_valuetype type;
	 napi_typeof(env, args[i], &type);
	 printf(" arg %d type %d\n", i, type);

	 if (type == napi_function) {
	    napi_value res;
	    napi_value callargs[1];
	    status = napi_call_function(env, global, args[i], 0, callargs, &res);
	          CHECK_STATUS;

	 }
	 i++;
      }
      printf("got %d args\n", i);

      status = napi_create_string_utf8(env, "world", 5, &world);
      assert(status == napi_ok);
      return world;
   }

#define DECLARE_NAPI_METHOD(name, func)		\
   { name, 0, func, 0, 0, 0, napi_default, 0 }


   napi_value init(napi_env env, napi_value exports) {


      napi_status status;

      status = napi_get_undefined( env, &undefinedval);

      napi_property_descriptor desc = DECLARE_NAPI_METHOD("log", log_to_midway);
      status = napi_define_properties(env, exports, 1, &desc);
      assert(status == napi_ok);

      desc = DECLARE_NAPI_METHOD("attach", Attach);
      status = napi_define_properties(env, exports, 1, &desc);
      assert(status == napi_ok);

      desc = DECLARE_NAPI_METHOD("detach", Attach);
      status = napi_define_properties(env, exports, 1, &desc);
      assert(status == napi_ok);

      desc = DECLARE_NAPI_METHOD("provide", Attach);
      status = napi_define_properties(env, exports, 1, &desc);
      assert(status == napi_ok);
      return exports;
   }

   NAPI_MODULE(NAPI_GYP_MODULE_NAME, init)

}  // namespace demo
