// hello.js
const mw = require('./build/Release/midway');

console.log(mw.log("test of log", "arg1", 66, 88.4, "arg4", new Object(), [1,2], function()   { return 3;}  ));


var attach_data = {
    url: "ipc://test",
    name: "nodejs_test",
    type: "server", // "server", "serveronly",
    authtype: "",
    username: "user_name",
    password: "psswd",
    credentials: "so kind of credentials for auth plugin"
}

//console.log(mw.attach());
//console.log(mw.attach(data));
//console.log(mw.attach(function() { console.info(" CB test"); }, "nodejs_test"));
console.log(mw.attach( undefined, "nodejs_test"));


function callback0() {
    console.log(global);
    return true;
}
function callback1(svcinfo, a, b, c, d) {
    console.log("got call with ", svcinfo, a, b, c, d, this);
}

console.log(mw.provide("node0", callback0));
console.log(mw.provide("node1", callback1));

console.log(mw.provide("nodel", function () {} ));
console.log("RUNNUNG SERVER");
mw.runServer();
	    
console.log(mw.unprovide("node0"));
console.log(mw.unprovide("node1"));





// Prints: 'world'
console.log(mw.detach());

