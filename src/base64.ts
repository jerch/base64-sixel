export type UintTypedArray = Uint8Array | Uint16Array | Uint32Array | Uint8ClampedArray;


import * as WASM_DATA from './wasm.json';

interface IWasmInternalExports extends Record<string, WebAssembly.ExportValue> {
  memory: WebAssembly.Memory;
  get_chunk_address(): number;
  get_target_address(): number;
  encode(length: number): number;
  decode(length: number): number;
  transcode(length: number): number;
}

/* istanbul ignore next */
function decodeBase64(s: string): Uint8Array {
  if (typeof Buffer !== 'undefined') {
    return Buffer.from(s, 'base64');
  }
  const bytestring = atob(s);
  const result = new Uint8Array(bytestring.length);
  for (let i = 0; i < result.length; ++i) {
    result[i] = bytestring.charCodeAt(i);
  }
  return result;
}

const WASM = {
  CHUNK_SIZE: WASM_DATA.chunkSize,
  BYTES: decodeBase64(WASM_DATA.bytes)
};

export class Base64Wasm {
  static _instance = new WebAssembly.Instance(new WebAssembly.Module(WASM.BYTES));
  static _wasm = Base64Wasm._instance.exports as IWasmInternalExports;
  static _chunk = new Uint8Array(Base64Wasm._wasm.memory.buffer, Base64Wasm._wasm.get_chunk_address(), WASM.CHUNK_SIZE);
  static _target = new Uint8Array(Base64Wasm._wasm.memory.buffer, Base64Wasm._wasm.get_target_address(), WASM.CHUNK_SIZE);

  static encode(data: Uint8Array, start: number = 0, end: number = data.length): Uint8Array {
    // FIXME: needs proper overflow check!
    if (end - start > WASM.CHUNK_SIZE) {
      throw new Error('data too big');
    }
    Base64Wasm._chunk.set(data.subarray(start, end));
    const length = Base64Wasm._wasm.encode(end - start);
    return Base64Wasm._target.subarray(0, length);
  }

  static decode(data: Uint8Array, start: number = 0, end: number = data.length): Uint8Array {
    if (end - start > WASM.CHUNK_SIZE) {
      throw new Error('data too big');
    }
    Base64Wasm._chunk.set(data.subarray(start, end));
    const length = Base64Wasm._wasm.decode(end - start);
    if (length < 0) {
      throw new Error(`wrong byte at position [${~length}]`);
    }
    return Base64Wasm._target.subarray(0, length);
  }

  static transcode(data: Uint8Array, start: number = 0, end: number = data.length): Uint8Array {
    if (end - start > WASM.CHUNK_SIZE) {
      throw new Error('data too big');
    }
    Base64Wasm._chunk.set(data.subarray(start, end));
    const length = Base64Wasm._wasm.transcode(end - start);
    if (length < 0) {
      throw new Error(`wrong byte at position [${~length}]`);
    }
    return Base64Wasm._target.subarray(0, length);
  }
}

function toBytes(s: string): Uint8Array {
  const bytes = new Uint8Array(s.length);
  for (let i = 0; i < s.length; ++i) {
    bytes[i] = s.charCodeAt(i) & 0xFF;
  }
  return bytes;
}
function toString(bytes: Uint8Array): string {
  let result = '';
  for (let i = 0; i < bytes.length; ++i) {
    result += String.fromCharCode(bytes[i]);
  }
  return result;
}

function playground() {

  // speed test
  const input1 = 'AAAAAAAAAAAAAAAA'.repeat(1024*4);
  const input2 = 'BBBBBBBBBBBBBBBB'.repeat(1024*4);
  const data1 = new Uint8Array(input1.length);
  const data2 = new Uint8Array(input2.length);
  for (let i = 0; i < input1.length; ++i) {
    data1[i] = input1.charCodeAt(i);
    data2[i] = input2.charCodeAt(i);
  }
  const ROUNDS = 10000;
  const start = Date.now();
  let l = 0;
  for (let i = 0; i < ROUNDS; ++i) {
    l+= Base64Wasm.decode(i%2 ? data1 : data2).length;
    //const d = Base64Wasm.transcode(i%2 ? data1 : data2).slice(0);
    //l+= Base64Wasm.decode(Base64Wasm.transcode(i%2 ? data1 : data2)).length;
  }
  const duration = Date.now() - start;
  console.log((input1.length * ROUNDS / duration * 1000 / 1024 / 1024).toFixed(0), 'MB/s', l, duration);


  console.log('encoding:');
  const some_data = new Uint8Array([1,2]);
  console.log([toString(Base64Wasm.encode(some_data))]);
  console.log([Buffer.from(some_data).toString('base64')]);
  console.log([toString(Base64Wasm.transcode(toBytes(Buffer.from(some_data).toString('base64'))))]);


  const encode_input = new Uint8Array(49152);
  const s = Date.now();
  l = 0;
  for (let i = 0; i < ROUNDS; ++i) {
    l += Base64Wasm.encode(encode_input).length;
  }
  const dur = Date.now() - s;
  console.log((encode_input.length * ROUNDS / dur * 1000 / 1024 / 1024).toFixed(0), 'MB/s', l, dur);

}
playground();
