#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "base64.cpp"

/**
 * TODO:
 * - flags: -d for decode, -t for transcode
 * - default mode encode
 * - aligned decoding: needs better read story
 * - better writing: how to get less stalls from underfull writes?
 * - proper makefile & sources cleanup
 * 
 * build
 * - SIMD   g++ -O3 -DCHUNK_SIZE=65536 -msse -msse2 -mssse3 -msse4.1 main.cpp -o base64
 * - scalar g++ -O3 -DCHUNK_SIZE=65536 main.cpp -o base64
 */


int main__(int argc, char **argv) {
  for (int k = 0; k < 65536; ++k) CHUNK[k] = 65;
  int total = atoi(argv[1]);
  long long decoded = 0;
  for (int i = 0; i < total; ++i) {
    decoded += decode(65536);
  }
  printf("%d,%d,%d  %lld  %d %d, %d\n", TARGET[0], TARGET[1], TARGET[2], decoded, total, CHUNK[65535], CHUNK_SIZE);
  printf("%p %p", &CHUNK[65536], TARGET);
  return 0;
}

int main(int argc, char **argv) {
  int errnum;
  int fd = 0;
  if (argc == 2) {
    // TODO: better argparse with support for -d and -t flags
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
      errnum = errno;
      fprintf(stderr, "input error %d: %s\n", errnum, strerror(errnum));
      return 1;
    }
  }
  setvbuf(stdout, NULL, _IONBF, 0);

  while (1) {
    int read_bytes = read(fd, CHUNK, CHUNK_SIZE);
    if (read_bytes < 0) {
      errnum = errno;
      fprintf(stderr, "read error %d: %s\n", errnum, strerror(errnum));
      if (fd) close(fd);
      return 1;
    }
    if (read_bytes == 0) {
      break;
    }
    if (read_bytes < CHUNK_SIZE) {
      fprintf(stderr, "Warning: read less than CHUNKSIZE - %d\n", read_bytes);
      // do something here about it - merge later? --> better perf with aligned decoding
    }
    int decoded_bytes = decode(read_bytes);
    //printf("decoded result: %d\n", decoded_bytes);
    if (decoded_bytes < 0) {
      // TODO: needs counter over all bytes so far
      fprintf(stderr, "exiting due to decoding error at: %d\n", ~decoded_bytes);
      if (fd) close(fd);
      return 1;
    }
    int left = decoded_bytes;
    while (left > 0) {
      int written_bytes = write(1, TARGET+(decoded_bytes-left), left);
      if (written_bytes < 0) {
        errnum = errno;
        fprintf(stderr, "write error %d: - %s\n", errnum, strerror(errnum));
        if (fd) close(fd);
        return 1;
      }
      left -= written_bytes;
    }
  }

  if (fd) close(fd);
  return 0;
}
