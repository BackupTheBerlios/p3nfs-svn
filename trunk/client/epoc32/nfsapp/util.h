#ifndef __UTIL_H
#define __UTIL_H

void debugline(const char *a, ...);
#define DEBUGLINE debugline("%s:%s: %d", __BASE_FILE__, __FUNCTION__, __LINE__)

#endif
