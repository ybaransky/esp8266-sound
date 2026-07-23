#include "log.h"

#include <string.h>

const char *logSourceName(const char *path) {
  const char *fileName = path;
  for (const char *cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') {
      fileName = cursor + 1;
    }
  }

  static char sourceName[32];
  const char *extension = strrchr(fileName, '.');
  size_t length =
      extension == nullptr ? strlen(fileName) : static_cast<size_t>(extension - fileName);
  length = min(length, sizeof(sourceName) - 1);
  memcpy(sourceName, fileName, length);
  sourceName[length] = '\0';
  return sourceName;
}
