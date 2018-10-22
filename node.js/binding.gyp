{
  "targets": [
      {
	  "target_name": "midway",
	  "sources": [ "mw_wrapper.cc", "mw_client_wrapper.cc", "mw_server_wrapper.cc", "utils.cc" ],
          "libraries": [		   
              "-lMidWay"
          ],
	  "cflags": ["-g"],
      }
  ]
}
