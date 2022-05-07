## base64-sixel

Base64 variant using the SIXEL characters.


### Motivation

While _base64_ with its default charmap is an ubiquitous standard and used for most transport needs
of arbitrary bytes in textual environments, it is quite costly to calculate. This mainly results from
the shape of its charmap, which is hard to process without dedicated lookup tables or CPU-exhausting conditions.
While there has been some success to speed up _base64_ decoding on beefy CPUs
(see https://arxiv.org/abs/1910.05109), it is surprisingly hard to run fast for a quite simple looking specification.

On the other hand environments like terminals have dealt with the same underlying issue,
how to transport full bytes without collision with terminal sequences, for a very long time.

The SIXEL graphics sequence from the 80s contains a 6-bit data encoding, that can be used safely in terminals
without side effects. The advantage of its characters is a direct contiguous mapping into ASCII,
from position 63 to position 126.

This proposal merges the bit encoding pattern used by _base64_ with the charmap of SIXEL.
It shall help to solve performance issues with base64 while staying usable in terminals for binary transports.
While it most likely will work in many other environments and transports as well, the compatibility is focused
on the terminal interface.

The idea for a dedicated 6-bit encoding based on SIXEL characters was originally brought up by @j4james
in terminal-wg while discussing some fundamental aspects of new terminal sequences (needs link...).


### Description

_base64-sixel_ is a 6-bit encoding compatible to _base64_ as defined by
[RFC4648 §4](https://datatracker.ietf.org/doc/html/rfc4648#section-4).

The charmap is a mapping of the contiguous ASCII positions 63 - 126 to the index value.
Any character outside this range is invalid and shall be treated as error stopping further decoding.
In particular this means:
- separator characters are not supported
- a padding character is not allowed (optional in RFC4648)

Beside these restrictions its bit encoding is fully base64-stable allowing a direct character
transformation as of:

  > BASE64_CHARMAP[i] <--> BASE64_SIXEL_CHARMAP[i]


The character table  reduces to these characters:

  > ?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~


<details>
  <summary>Full Character Table Mappings</summary>

|  Index | base64-standard | base64-sixel |
| ------ | :-------------: | :----------: |
|      0 |               A |            ? |
|      1 |               B |            @ |
|      2 |               C |            A |
|      3 |               D |            B |
|      4 |               E |            C |
|      5 |               F |            D |
|      6 |               G |            E |
|      7 |               H |            F |
|      8 |               I |            G |
|      9 |               J |            H |
|     10 |               K |            I |
|     11 |               L |            J |
|     12 |               M |            K |
|     13 |               N |            L |
|     14 |               O |            M |
|     15 |               P |            N |
|     16 |               Q |            O |
|     17 |               R |            P |
|     18 |               S |            Q |
|     19 |               T |            R |
|     20 |               U |            S |
|     21 |               V |            T |
|     22 |               W |            U |
|     23 |               X |            V |
|     24 |               Y |            W |
|     25 |               Z |            X |
|     26 |               a |            Y |
|     27 |               b |            Z |
|     28 |               c |            [ |
|     29 |               d |            \ |
|     30 |               e |            ] |
|     31 |               f |            ^ |
|     32 |               g |            _ |
|     33 |               h |            ` |
|     34 |               i |            a |
|     35 |               j |            b |
|     36 |               k |            c |
|     37 |               l |            d |
|     38 |               m |            e |
|     39 |               n |            f |
|     40 |               o |            g |
|     41 |               p |            h |
|     42 |               q |            i |
|     43 |               r |            j |
|     44 |               s |            k |
|     45 |               t |            l |
|     46 |               u |            m |
|     47 |               v |            n |
|     48 |               w |            o |
|     49 |               x |            p |
|     50 |               y |            q |
|     51 |               z |            r |
|     52 |               0 |            s |
|     53 |               1 |            t |
|     54 |               2 |            u |
|     55 |               3 |            v |
|     56 |               4 |            w |
|     57 |               5 |            x |
|     58 |               6 |            y |
|     59 |               7 |            z |
|     60 |               8 |            { |
|     61 |               9 |           \| |
|     62 |               + |            } |
|     63 |               / |            ~ |

</details>



### Example Implementation

Some notes on example implementations contained in the repo TODO...


### Performance

TODO...


### Status

Currently WIP/PoC, far from being usable by any means.
