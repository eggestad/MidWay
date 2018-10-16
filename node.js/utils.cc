
#include <stdio.h>


#include <node_api.h>


namespace MidWay {

   /* helper function meant to deal with object keys
    */ 
   napi_value tmpStrVal;
   napi_value * valFromString(napi_env env, const char * str, napi_value *val) {
      if (val == NULL) val = &tmpStrVal;
      napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, val);
      return val;
   }

   
   /* 
    * just a helper function to make mwlog messages with javascript data type 
    */
    int JSValtoString(napi_env env, napi_value val, char * buf, size_t len) {

      napi_value v;
      napi_coerce_to_string(env, val, &v);
      val = v;

	 
      napi_valuetype type;
      napi_typeof(env, val, &type);
      napi_status sts;

      switch(type)  {

      case napi_undefined:
	 return snprintf(buf, len, "(undefined)");      case napi_null:
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

}
