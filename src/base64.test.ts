import * as assert from 'assert';
import { Base64Wasm } from './base64';


const AL_SIXEL = '?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
const AL_BASE64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';


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


describe('base64-sixel', () => {
  describe('encode', () => {
    console.log([toString(Base64Wasm.encode(toBytes('hello world!')))]);
    console.log(toString(Base64Wasm.decode(toBytes('YETkZE{_\\u|qZEO`'))));
  });
  describe('decode', () => {
    it('SIMD: 12 byte data equality to base64 standard', () => {
      // base-sixel decode must be 1:1 compatible to base64
      // necessity:
      //    b64_decode(AL_BASE64[i]) == b64_sixel_decode(AL_SIXEL[i])
      const data = new Uint8Array(12);
      for (let idx = 0; idx < 12; ++idx) {
        for (let byte = 0; byte < 256; ++byte) {
          data[idx] = byte;
          // console.log(data.join(','));
          const b64 = Buffer.from(data).toString('base64');
          let b64s = '';
          for (let i = 0; i < b64.length; ++i) {
            b64s += AL_SIXEL[AL_BASE64.indexOf(b64[i])];
          }
          // console.log(b64, '-->', b64s);
          assert.deepStrictEqual(Base64Wasm.decode(toBytes(b64s)), data);
        }
      }
    });
  });
});
