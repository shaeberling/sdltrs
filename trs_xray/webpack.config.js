const path = require('path');
module.exports = {
   mode: "production",
   entry: "./dist/ts/trs_xray.js",
   output: {
       filename: "trs_xray.js",
       path: path.resolve(__dirname, 'dist/webpack'),
   },
};