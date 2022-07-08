#include "bp.h"

#include <stdlib.h>
#include <string.h>

uint32_t bps_add(bps_t *bps, const char *file, int line, const char **err)
{
#define E(msg) \
  do { \
    if (err != NULL) \
      *err = msg; \
    return 0; \
  } while (0)

  if (file == NULL || *file == '\0')
    E("invalid file name");
  if (line < 1)
    E("invalid line");

  size_t slen = strlen(file);
  char *sfile = malloc(slen + 1);
  if (sfile == NULL)
    E("malloc");
  memcpy(sfile, file, slen + 1);

  size_t nlen = bps->len + 1;
  bp_t *nbps = realloc(bps->data, nlen * sizeof(bp_t));
  if (nbps == NULL) {
    free(sfile);
    E("malloc");
  }

  uint32_t id = ++bps->id;
  nbps[nlen - 1] = (bp_t){ .id = id, .line = line, .file = sfile };
  bps->data = nbps;
  bps->len = nlen;
  return id;
}

void bps_free(bps_t *bps)
{
  if (bps == NULL)
    return;
  if (bps->data != NULL) {
    for (size_t i = 0; i < bps->len; ++i)
      free(bps->data[i].file);
    free(bps->data);
  }
  *bps = BPS_INIT;
}

// vim: sw=2 sts=2 et
