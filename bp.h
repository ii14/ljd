#pragma once

#include <stdint.h>

/// breakpoint
typedef struct {
  int32_t id;
  int32_t line;
  char *file;
} bp_t;

/// breakpoints list
typedef struct {
  bp_t *data;
  uint32_t len;
  int32_t id;
} bps_t;

/// init breakpoints list
#define BPS_INIT (bps_t){ \
  .data = NULL, \
  .len = 0, \
  .id = 0, \
}

uint32_t bps_add(bps_t *bps, const char *file, int line, const char **err);
void bps_free(bps_t *bps);

// vim: sw=2 sts=2 et
