// hello.js
const mw = require('./build/Release/midway');

console.log(mw.log("test of log", "arg1", 66, 88.4, "arg4"));
console.log(mw.attach());
console.log(mw.attach("22"));
console.log(mw.attach("22", 42, 999.0, function() { console.log("in CB"); return 666;}));
// Prints: 'world'
