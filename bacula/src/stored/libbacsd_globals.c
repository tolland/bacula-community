/* Stub definitions for global variables referenced by libbacsd
 * These are normally defined in global.c which is part of the main executable
 * We provide weak definitions here so the library can link
 */
#include "bacula.h"
#include "stored.h"

STORES *me = NULL;
bool forge_on = false;
pthread_cond_t wait_device_release = PTHREAD_COND_INITIALIZER;
pthread_mutex_t device_release_mutex = PTHREAD_MUTEX_INITIALIZER;
char *configfile = NULL;
bool got_caps_needed = false;

