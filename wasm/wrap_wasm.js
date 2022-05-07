const fs = require('fs');

const wasmData = JSON.stringify({
  chunkSize: parseInt(process.argv[2]),
  bytes: fs.readFileSync('base64.wasm').toString('base64')
});

// also overwrite src target in case a bundler pulls from TS source folders
fs.writeFileSync('../lib/wasm.json', wasmData);
fs.writeFileSync('../src/wasm.json', wasmData);
