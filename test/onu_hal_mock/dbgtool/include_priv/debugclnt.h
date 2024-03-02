#ifndef __debugclnt_h__
#define __debugclnt_h__

#include <stddef.h> // size_t
#include <stdbool.h>

bool dbg_handle_command(const char* server_path, const void* msg, size_t len);

#endif
