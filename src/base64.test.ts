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
  describe('transcode', () => {
    it('errorcode on non base64 character - 16 bytes', () => {
      // exclude skipped chars here
      const PASS = AL_BASE64 + ' \n\r=';
      for (let c = 0; c < 256; ++c) {
        const invalid = String.fromCharCode(c);
        if (PASS.includes(invalid)) continue;
        if (63 <= c && c <= 126) continue;
        const zero = 'A';
        for (let i = 0; i < 16; ++i) {
          const inp = zero.repeat(16).split('');
          inp[i] = invalid;
          assert.throws(
            () => Base64Wasm.transcode(toBytes(inp.join(''))),
            new RegExp(`^Error: wrong byte at position \\[${i}\\]$`)
          );
        }
      }
    });
  });
  describe('encode', () => {});
  describe('decode', () => {
    it('decode underfull', () => {
      for (let i = 1; i < 16; ++i) {
        // skip single bytes here (tested below)
        if ((i % 4) == 1) continue;
        const l = Math.floor(i/4)*3 + ((i%4)==2 ? 1 : (i%4)==3 ? 2 : 0);
        const d1 = Base64Wasm.decode(toBytes('?'.repeat(i)));
        assert.strictEqual(d1.length, l);
        assert.deepStrictEqual(d1, new Uint8Array(l));
        const d2 = Base64Wasm.decode(toBytes('~'.repeat(i)));
        assert.strictEqual(d2.length, l);
        assert.deepStrictEqual(d2, new Uint8Array(l).fill(255));
      }
    });
    it('errorcode on underfull from single byte', () => {
      for (const i of [1, 5, 9]) {
        assert.throws(
          () => Base64Wasm.decode(toBytes('?'.repeat(i))),
          new RegExp(`^Error: wrong byte at position \\[${i}\\]$`) // Note: reports position+1!
        );
      }
    });
    it('errorcode on non sixel character - 16 bytes', () => {
      for (let c = 0; c < 256; ++c) {
        const invalid = String.fromCharCode(c);
        if (AL_SIXEL.includes(invalid)) continue;
        if (63 <= c && c <= 126) continue;
        const zero = '?';
        for (let i = 0; i < 16; ++i) {
          const inp = zero.repeat(16).split('');
          inp[i] = invalid;
          assert.throws(
            () => Base64Wasm.decode(toBytes(inp.join(''))),
            new RegExp(`^Error: wrong byte at position \\[${i}\\]$`)
          );
        }
      }
    });
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
