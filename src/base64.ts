export type UintTypedArray = Uint8Array | Uint16Array | Uint32Array | Uint8ClampedArray;


import * as WASM_DATA from './wasm.json';

interface IWasmInternalExports extends Record<string, WebAssembly.ExportValue> {
  memory: WebAssembly.Memory;
  get_chunk_address(): number;
  get_target_address(): number;
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
  
  static decode(data: Uint8Array, start: number = 0, end: number = data.length): Uint8Array {
    if (end - start > WASM.CHUNK_SIZE) {
      throw new Error('data too big');
    }
    Base64Wasm._chunk.set(data.subarray(start, end));
    const decodedLength = Base64Wasm._wasm.decode(end - start);
    return Base64Wasm._target.subarray(0, decodedLength);
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

  // FIXME: quickly hacked in TS for now, needs wasm impl
  static encode(data: Uint8Array, start: number = 0, end: number = data.length): Uint8Array {
    let length = end - start;
    const target = new Uint8Array(Math.ceil(length / 3) * 4);
    if (!length) {
      return target;
    }
    const padding = length % 3;
    if (padding) {
      length -= padding;
    }
    let j = 0;
    for (let i = 0; i < length; i += 3) {
      // load 3x 8 bit values
      let accu = data[i] << 16 | data[i + 1] << 8 | data[i + 2];

      // write 4x 6 bit values
      target[j] = (accu >> 18) + 63;
      target[j + 1] = ((accu >> 12) & 0x3F) + 63;
      target[j + 2] = ((accu >> 6) & 0x3F) + 63;
      target[j + 3] = (accu & 0x3F) + 63;
      j += 4;
    }
    if (padding) {
      if (padding === 2) {
        let accu = data[length] << 8 | data[length + 1];
        accu <<= 2;
        target[j++] = (accu >> 12) + 63;
        target[j++] = ((accu >> 6) & 0x3F) + 63;
        target[j++] = (accu & 0x3F) + 63;
      } else {
        let accu = data[length];
        accu <<= 4;
        target[j++] = (accu >> 6) + 63;
        target[j++] = (accu & 0x3F) + 63;
      }
    }
    return target.subarray(0, j);
  }
}

function toBytes(s: string): Uint8Array {
  const bytes = new Uint8Array(s.length);
  for (let i = 0; i < s.length; ++i) {
    bytes[i] = s.charCodeAt(i) & 0xFF;
  }
  return bytes;
}

function dec(s: string) {
  //console.log(Array.from(Base64Wasm.decode(toBytes(s))).map(el => el.toString(2).padStart(8, '0')).join(','));
  console.log(s, Base64Wasm.decode(toBytes(s)).join(','));
}
function dec2(s: string) {
  const ALs = '?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
  const AL = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  let b64 = '';
  for (let i = 0; i < s.length; ++i) {
    b64 += AL[ALs.indexOf(s[i])];
  }
  console.log(b64, Array.from(Buffer.from(b64, 'base64')).join(','));
}

// '@ACGO_' --> 1 2 4 8 16 32
// '?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'

console.log('base64-sixel');
dec('@???????????????');
dec('?C??????????????');
dec('??O?????????????');
dec('????@???????????');
dec('?????C??????????');
dec('??????O?????????');
dec('????????@???????');
dec('?????????C??????');
dec('??????????O?????');
dec('????????????@???');
dec('?????????????C??');
dec('??????????????O?');
dec('~~~~~~~~~~~~~~~~');
dec('??????????????x~');

console.log('base64');
dec2('@???????????????');
dec2('?C??????????????');
dec2('??O?????????????');
dec2('????@???????????');
dec2('?????C??????????');
dec2('??????O?????????');
dec2('????????@???????');
dec2('?????????C??????');
dec2('??????????O?????');
dec2('????????????@???');
dec2('?????????????C??');
dec2('??????????????O?');
dec2('~~~~~~~~~~~~~~~~');
dec2('??????????????x~');


console.log('############');
dec ('?O??????????????');
dec2('?O??????????????');


// speed test
const input = 'AAAAAAAAAAAAAAAA'.repeat(1024*4);
const data = new Uint8Array(input.length);
for (let i = 0; i < input.length; ++i) data[i] = input.charCodeAt(i);

const ROUNDS = 1000;
const start = Date.now();
for (let i = 0; i < ROUNDS; ++i) {
  Base64Wasm.transcode(data);
}
const duration = Date.now() - start;
console.log((input.length * ROUNDS / duration * 1000 / 1024 / 1024).toFixed(0), 'MB/s');
