#pragma once

#include <stdint.h>

/// breakpoint
typedef struct {
  int32_t id;
  int32_t line;
  char *file;
} bp_t;

/// breakpoint list
typedef struct {
  bp_t *data;
  uint32_t len;
  int32_t id;
} bps_t;

/// init breakpoint list
#define BPS_INIT (bps_t){ \
  .data = NULL, \
  .len = 0, \
  .id = 0, \
}

void bps_free(bps_t *bps);
uint32_t bps_add(bps_t *bps, const char *file, int line, const char **err);

// vim: sw=2 sts=2 et
