// SPDX-License-Identifier: Apache-2.0
// Part of Fingertip-dit: https://github.com/galetaire/fingertip-dit
// hnsd wrapper for Fingertip on macOS.
// Fingertip launches "hnsd" (next to its own executable) with fixed arguments.
// This wrapper execs the real binary (hnsd-core, same folder) with --checkpoint
// and --prefix added, so the chain syncs from a recent checkpoint and headers
// persist across restarts. exec() keeps the PID, so when Fingertip kills
// "hnsd" it kills hnsd-core directly.
//
// Headers are stored in ~/Library/Application Support/Fingertip-dit/hnsd-data,
// because the signed .app bundle is read-only.
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mach-o/dyld.h>

static int self_dir(char *out, size_t len) {
  char buf[PATH_MAX];
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) != 0) return -1;
  char real[PATH_MAX];
  if (!realpath(buf, real)) return -1;
  char *slash = strrchr(real, '/');
  if (!slash) return -1;
  *slash = 0;
  if (snprintf(out, len, "%s", real) >= (int)len) return -1;
  return 0;
}

// mkdir -p
static int make_dirs(char *path) {
  for (char *p = path + 1; *p; p++) {
    if (*p != '/') continue;
    *p = 0;
    int r = mkdir(path, 0700);
    *p = '/';
    if (r != 0 && errno != EEXIST) return -1;
  }
  if (mkdir(path, 0700) != 0 && errno != EEXIST) return -1;
  return 0;
}

static int data_dir(char *out, size_t len) {
  const char *home = getenv("HOME");
  if (!home || !*home) return -1;
  int n = snprintf(out, len, "%s/Library/Application Support/Fingertip-dit/hnsd-data", home);
  if (n < 0 || n >= (int)len) return -1;
  return make_dirs(out);
}

int main(int argc, char **argv) {
  char dir[PATH_MAX], core[PATH_MAX], data[PATH_MAX];
  if (self_dir(dir, sizeof(dir)) != 0) {
    fprintf(stderr, "hnsd wrapper: cannot locate own directory\n");
    return 1;
  }
  if (snprintf(core, sizeof(core), "%s/hnsd-core", dir) >= (int)sizeof(core)) return 1;

  char **args = calloc((size_t)argc + 4, sizeof(char *));
  if (!args) return 1;
  int i = 0;
  args[i++] = core;
  args[i++] = "--checkpoint";
  if (data_dir(data, sizeof(data)) == 0) {
    args[i++] = "--prefix";
    args[i++] = data;
  } else {
    // Still works without a header store, it just re-syncs on every start.
    fprintf(stderr, "hnsd wrapper: no writable data dir, headers won't persist\n");
  }
  for (int j = 1; j < argc; j++) args[i++] = argv[j];
  args[i] = NULL;

  execv(core, args);
  fprintf(stderr, "hnsd wrapper: exec %s: %s\n", core, strerror(errno));
  return 127;
}
