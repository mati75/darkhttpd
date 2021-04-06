// Wrapper around make_safe_url() for fuzzing with LLVM.
// Aborts if the output is deemed safe but contains /../ or /./
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Consolidate slashes in-place by shifting parts of the string over repeated
 * slashes.
 */
static void consolidate_slashes(char *s) {
  size_t left = 0, right = 0;
  int saw_slash = 0;

  assert(s != NULL);
  while (s[right] != '\0') {
    if (saw_slash) {
      if (s[right] == '/')
        right++;
      else {
        saw_slash = 0;
        s[left++] = s[right++];
      }
    } else {
      if (s[right] == '/') saw_slash++;
      s[left++] = s[right++];
    }
  }
  s[left] = '\0';
}

/* Resolve /./ and /../ in a URL, in-place.  Also strip out query params.
 * Returns NULL if the URL is invalid/unsafe, or the original buffer if
 * successful.
 */
static char *make_safe_url(char *url) {
  struct {
    char *start;
    size_t len;
  } * chunks;
  unsigned int num_slashes, num_chunks;
  size_t urllen, i, j, pos;
  int ends_in_slash;

  /* strip query params */
  for (pos = 0; url[pos] != '\0'; pos++) {
    if (url[pos] == '?') {
      url[pos] = '\0';
      break;
    }
  }

  if (url[0] != '/') return NULL;

  consolidate_slashes(url);
  urllen = strlen(url);
  if (urllen > 0)
    ends_in_slash = (url[urllen - 1] == '/');
  else
    ends_in_slash = 1;

  /* count the slashes */
  for (i = 0, num_slashes = 0; i < urllen; i++)
    if (url[i] == '/') num_slashes++;

  /* make an array for the URL elements */
  assert(num_slashes > 0);
  chunks = malloc(sizeof(*chunks) * num_slashes);

  /* split by slashes and build chunks array */
  num_chunks = 0;
  for (i = 1; i < urllen;) {
    /* look for the next slash */
    for (j = i; j < urllen && url[j] != '/'; j++)
      ;

    /* process url[i,j) */
    if ((j == i + 1) && (url[i] == '.'))
      /* "." */;
    else if ((j == i + 2) && (url[i] == '.') && (url[i + 1] == '.')) {
      /* ".." */
      if (num_chunks == 0) {
        /* unsafe string so free chunks */
        free(chunks);
        return (NULL);
      } else
        num_chunks--;
    } else {
      chunks[num_chunks].start = url + i;
      chunks[num_chunks].len = j - i;
      num_chunks++;
    }

    i = j + 1; /* url[j] is a slash - move along one */
  }

  /* reassemble in-place */
  pos = 0;
  for (i = 0; i < num_chunks; i++) {
    assert(pos <= urllen);
    url[pos++] = '/';

    assert(pos + chunks[i].len <= urllen);
    assert(url + pos <= chunks[i].start);

    if (url + pos < chunks[i].start)
      memmove(url + pos, chunks[i].start, chunks[i].len);
    pos += chunks[i].len;
  }
  free(chunks);

  if ((num_chunks == 0) || ends_in_slash) url[pos++] = '/';
  assert(pos <= urllen);
  url[pos] = '\0';
  return url;
}

char *make_safe_url_new(char *const url) {
    char *src = url, *dst;
    #define ends(c) ((c) == '/' || (c) == '\0')

    /* URLs not starting with a slash are illegal. */
    if (*src != '/')
        return NULL;

    /* Fast case: skip until first double-slash or dot-dir. */
    for ( ; *src && *src != '?'; ++src) {
        if (*src == '/') {
            if (src[1] == '/')
                break;
            else if (src[1] == '.') {
                if (ends(src[2]))
                    break;
                else if (src[2] == '.' && ends(src[3]))
                    break;
            }
        }
    }

    /* Copy to dst, while collapsing multi-slashes and handling dot-dirs. */
    dst = src;
    while (*src && *src != '?') {
        if (*src != '/')
            *dst++ = *src++;
        else if (*++src == '/')
            ;
        else if (*src != '.')
            *dst++ = '/';
        else if (ends(src[1]))
            /* Ignore single-dot component. */
            ++src;
        else if (src[1] == '.' && ends(src[2])) {
            /* Double-dot component. */
            src += 2;
            if (dst == url)
                return NULL; /* Illegal URL */
            else
                /* Backtrack to previous slash. */
                while (*--dst != '/' && dst > url);
        }
        else
            *dst++ = '/';
    }

    if (dst == url)
        ++dst;
    *dst = '\0';
    return url;
    #undef ends
}

char* dup(const uint8_t *data, size_t size) {
  char *buf = malloc(size + 1);
  memcpy(buf, data, size);
  buf[size] = 0;
  return buf;
}

void check(char* safe) {
  if (safe == NULL) return;
  if (strstr(safe, "/../") != NULL) __builtin_trap();
  if (strstr(safe, "/./") != NULL) __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *buf1 = dup(data, size);
  char *buf2 = dup(data, size);
  // Enable this to make sure the fuzzer is working.
#if 0
  if (size > 2 && buf1[2] == 'x') __builtin_trap();
#endif
  char *safe1 = make_safe_url(buf1);
  char *safe2 = make_safe_url(buf2);
  check(safe1);
  check(safe2);
  if (safe1 == NULL) {
    if (safe2 != NULL) __builtin_trap();
  } else {
    if (strcmp(safe1, safe2) != 0) __builtin_trap();
  }
  // Don't leak memory.
  free(buf1);
  free(buf2);
  return 0;
}

/* vim:set ts=2 sw=2 sts=2 expandtab tw=78: */
