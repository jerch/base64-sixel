#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <getopt.h>

#include "base64.c"

/**
 * TODO:
 * - better writing: how to get less stalls from underfull writes?
 * - proper makefile & sources cleanup
 * 
 * build
 * - SIMD   g++ -O3 -DCHUNK_SIZE=65536 -msse -msse2 -mssse3 -msse4.1 main.cpp -o base64-sixel
 * - scalar g++ -O3 -DCHUNK_SIZE=65536 main.cpp -o base64-sixel
 * 
 * build C
 * clang -std=c99 -Wall -Wextra -march=native -O3 -DCHUNK_SIZE=65536 main.c -o base64-sixel
 */


void usage(FILE *target, const char *executable) {
  fprintf(target,
  "usage: %s [-edht] [-o OUTFILE] [INFILE]\n"
  "  -e         encode (default action)\n"
  "  -d         decode\n"
  "  -t         transcode base64 to base64-sixel\n"
  "  -h         print help message\n"
  "  -o FILE    output to FILE, default is <stdout>\n"
  "If INFILE is not provided, data is read from <stdin>.\n",
  executable
  );
}

int main(int argc, char **argv) {
  int errnum;
  int fd_in = 0;
  int fd_out = 1;
  int (*operate)(int) = &encode;
  int read_limit = ENCODE_LIMIT;
  int read_align = 12;

  int option;
  char *outfile = NULL;

  size_t remaining_read = 0;
  size_t global_read_pos = 0;
  int finished = 0;

  while ((option = getopt(argc, argv, "edhto:")) != -1) {
    switch (option) {
      case 'e':
        operate = &encode;
        read_limit = ENCODE_LIMIT;
        read_align = 12;
        break;
      case 'd':
        operate = &decode;
        read_limit = DECODE_LIMIT;
        read_align = 16;
        break;
      case 'h':
        usage(stdout, argv[0]);
        return 0;
      case 'o':
        outfile = optarg;
        break;
      case 't':
        operate = &transcode;
        read_limit = TRANSCODE_LIMIT;
        read_align = 8;
        break;
      case '?':
        usage(stderr, argv[0]);
        return 1;
      case ':':
        usage(stderr, argv[0]);
        return 1;
      default:
        usage(stderr, argv[0]);
        return 1;
    }
  }

  /* infile */
  if (argv[optind]) {
    if (argv[optind+1]) {
      fprintf(stderr, "too many arguments\n");
      usage(stderr, argv[0]);
      return 1;
    }
    if (strcmp(argv[optind], "-") != 0) {
      fd_in = open(argv[optind], O_RDONLY);
      if (fd_in < 0) {
        errnum = errno;
        fprintf(stderr, "input error %d: %s\n", errnum, strerror(errnum));
        return 1;
      }
    }
  }

  /* outfile */
  if (outfile) {
      fd_out = open(
        outfile,
        O_CREAT | O_WRONLY,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
      );
      if (fd_out < 0) {
        errnum = errno;
        fprintf(stderr, "output error %d: %s\n", errnum, strerror(errnum));
        if (fd_in) close(fd_in);
        return 1;
      }
  } else {
    setvbuf(stdout, NULL, _IONBF, 0);
  }

  /* read/operate/write loop */
  while (1) {

    /* read */
    ssize_t read_bytes = read(fd_in, CHUNK + remaining_read, read_limit - remaining_read);
    if (read_bytes < 0) {
      errnum = errno;
      fprintf(stderr, "read error %d: %s\n", errnum, strerror(errnum));
      if (fd_in) close(fd_in);
      if (fd_out > 1) close(fd_out);
      return 1;
    }
    if (read_bytes == 0) {
      if (!remaining_read) break;
      finished = 1;
    }

    /**
     * operate
     *
     * special case: we read less than expected, possible reasons:
     * - data not ready yet
     * - pipebuf is smaller
     * - end of data
     * We dont know until next read, if we are at the real end of data.
     * This creates issues for decode and encode, which need 4 resp. 3 sequential bytes.
     * Therefore we align data as follows:
     * - decode to 16 bytes
     * - encode to 12 bytes
     * - transcode to 8 bytes (technically not needed)
     * Remaining bytes are carried over to the next read cycle(move to CHUNK start + offset read).
     * If the next read cycle produces 0 bytes, we hit end of data.
     *
     */
    ssize_t handled_bytes = 0;
    if (read_bytes + remaining_read < read_limit && !finished) {
      // TODO: can we avoid div/mul here?
      size_t to_handle = ((read_bytes + remaining_read) / read_align) * read_align;
      handled_bytes = operate(to_handle);
      remaining_read = read_bytes + remaining_read - to_handle;
      if (remaining_read) memmove(CHUNK, CHUNK + to_handle, remaining_read);
    } else {
      handled_bytes = operate(read_bytes + remaining_read);
      remaining_read = 0;
    }
    // error handling from operate
    if (handled_bytes < 0) {
      fprintf(stderr, "operation error at byte position: %lu\n", global_read_pos + ~handled_bytes);
      if (fd_in) close(fd_in);
      if (fd_out > 1) close(fd_out);
      return 1;
    }
    global_read_pos += handled_bytes;

    /* write */
    size_t written = 0;
    while (written < handled_bytes) {
      ssize_t written_bytes = write(fd_out, TARGET + written, handled_bytes - written);
      if (written_bytes < 0) {
        errnum = errno;
        fprintf(stderr, "write error %d: - %s\n", errnum, strerror(errnum));
        if (fd_in) close(fd_in);
        if (fd_out > 1) close(fd_out);
        return 1;
      }
      written += written_bytes;
    }

    if (finished) break;
  }

  if (fd_in) close(fd_in);
  if (fd_out > 1) close(fd_out);
  return 0;
}
