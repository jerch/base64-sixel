"""
TODO:
- move to own python binding folder
- bundle things to ready-to-go wheels?
- do we need simd feature detection?
- finish stream encoder
- should this get an explicit c binding / low to no perf benefit?
- tests
"""


from ctypes import CDLL
from typing import Iterable, Generator

lib = CDLL('./libbase64sixel.so')


class Base64SixelException(Exception):
    pass


def encode(b: bytes) -> bytes:
    """
    Encode bytes ``b`` with base64sixel.
    Returns encoded bytes.
    """
    length = len(b)
    parts, rem = divmod(length, 3)
    tail = 0
    if rem == 1:
        tail = 2
    elif rem == 2:
        tail = 3
    target = b'\x00' * (parts * 4 + tail)
    lib.base64sixel_encode(b, length, target)
    return target


def decode(b: bytes) -> bytes:
    """
    Decode base64sixel encoded bytes ``b``.
    Returns decoded bytes or raises ``Base64SixelException``.
    """
    length = len(b)
    parts, rem = divmod(length, 4)
    tail = 0
    if rem == 2:
      tail = 1
    elif rem == 3:
      tail = 2
    target = b'\x00' * (parts * 3 + tail)
    written = lib.base64sixel_decode(b, length, target)
    if written < 0:
        raise Base64SixelException(f'decode error at position {~written}')
    return target


def transcode(b: bytes) -> bytes:
    """
    Transcode base64 bytes ``b`` to base64-sixel.
    Returns transcoded bytes or raises ``Base64SixelException``.
    """
    length = len(b)
    target = b'\x00' * length
    written = lib.base64sixel_transcode(b, length, target)
    if written < 0:
        raise Base64SixelException(f'transcode error at position {~written}')
    if written < length:
        target[:written]
    return target


class StreamEncoder:
    def __init__(self, maxsize) -> None:
        self.maxsize = ((maxsize + 11) // 12) * 12
        self._target = b'\x00' * self.maxsize
        self._mem = memoryview(self._target)
        self.reset()
    
    def reset(self) -> None:
        pass
    
    def encode(self, b: bytes) -> memoryview:
        pass
    
    def end(self) -> memoryview:
        pass
    
    def iterator(self, data_source: Iterable) -> Generator[memoryview, None, None]:
        pass


class StreamDecoder:
    """
    Stream decoder for chunked base64-sixel data.

    The stream decoder can be used under memory pressure with rather big data
    to chain decoding into chunkwise handling, e.g. with generators/iterables.

    The constructor understands a ``maxsize`` argument denoting the maximum chunk size
    that may occur. The default value is set to 64kB, which should work with most
    low level APIs, e.g. data from file objects. Providing bigger chunks than specified
    in ``maxsize`` will raise an exception (either split data prehand or adjust the value
    to a more appropriate value).

    Feed the chunk data to the decoder with ``decode`` and finalize the decoding with ``end``.
    Both methods will return a memoryview on the decoded bytes and may be empty
    (no actual decoding happened).

    Note that the memoryview points to borrowed memory in the decoder instance,
    thus the decoded data will get destroyed by the next ``decode`` or ``end`` call.
    Do an explicit copy / bytes conversion, if you have to preserve decoded data over
    multiple ``decode`` invocations.
    
    explicit usage eample:

        decoder = StreamDecoder(MAX_CHUNKSIZE)
        for chunk in some_iterable_src:
            decoded = decoder.decode(chunk)
            if decoded:
                yield decoded   # or decoded.tobytes()
        decoded = decoder.end()
        if decoded:
            yield decoded       # or decoded.tobytes()
    
    For convenience ``iterator`` can be used with an iterable data source,
    which produces same results as above:
    
        for decoded in StreamDecoder(MAX_CHUNKSIZE).iterator(some_iterable_src):
            yield decoded       # or decoded.tobytes()
    """
    def __init__(self, maxsize: int = 65536) -> None:
        self.maxsize = (maxsize + 15) & ~0xF
        self._target = b'\x00' * self.maxsize
        self._mem = memoryview(self._target)
        self._left = b''
    
    def reset(self) -> None:
        """
        Reset stream decoder to use it with another data stream.
        """
        self._left = b''
    
    def decode(self, b: bytes) -> memoryview:
        """
        Decode bytes in chunk ``b``.
        Returns a memoryview of decoded bytes or raises ``Base64SixelException``.
        """
        if len(b) > self.maxsize:
            raise Base64SixelException('bytelength may not exceed maxsize')
        if self._left:
            b = self._left + b
            self._left = b''
        length = len(b)
        length_aligned = length & ~0xF
        if length_aligned < length:
          self._left = b[length_aligned:]
        written = 0
        if length_aligned:
            written = lib.base64sixel_decode(b, length_aligned, self._target)
            if written < 0:
                raise Base64SixelException(f'decode error at position {~written}')
        return self._mem[:written]
    
    def end(self) -> memoryview:
        """
        Finalize stream decoding.
        Returns a memoryview of decoded tail bytes or raises ``Base64SixelException``.
        """
        written = 0
        if self._left:
            written = lib.base64sixel_decode(self._left, len(self._left), self._target)
            self._left = b''
        if written < 0:
            raise Base64SixelException(f'decode error at position {~written}')
        return self._mem[:written]

    def iterator(self, data_source: Iterable) -> Generator[memoryview, None, None]:
        """
        Convenient iterator to write less noisy code.
        """
        for chunk in data_source:
            mv = self.decode(chunk)
            if mv:
                yield mv
        mv = self.end()
        if mv:
            yield mv


s = b'Hello World!123'
print([encode(s)])
print([decode(encode(s))])

from time import time
from base64 import b64decode, b64encode

def test_decode():
    CHUNK = 65536
    data = b'A' * CHUNK
    start = time()
    for i in range(10000):
        #decode(data)
        #b64decode(data)
        #encode(data)
        #b64encode(data)
        decode(encode(data))
        #b64decode(b64encode(data))
        #transcode(data)
    duration = time() - start
    print(duration, CHUNK*10000/1024./1024/duration)

test_decode()

def test_sd():
    L = 100000000
    CHUNK = 65536
    sd = StreamDecoder()
    data = b'?' * L
    c = 0
    start = time()
    for i in range(0, L, CHUNK):
        c += len(sd.decode(data[i:i+CHUNK]))
    c += len(sd.end())
    duration = time() - start
    print(duration, L/1024./1024/duration)
    print(c)
    # streamed
    def gen():
        for i in range(0, L, CHUNK):
            yield data[i:i+CHUNK]
    start = time()
    decoded = [len(d) for d in StreamDecoder().iterator(gen())]
    duration = time() - start
    print(duration, L/1024./1024/duration)
    print(sum(decoded))

test_sd()
