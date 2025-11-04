/* Symbols needed from ini.c when TEST_PROGRAM is not defined */
/* These are needed for linking with libbaccfg */
#include "bacula.h"

int32_t r_last;
int32_t r_first;
RES_HEAD **res_head;
bool save_resource(RES_HEAD **rhead, int type, RES_ITEM *items, int pass){return false;}
bool save_resource(CONFIG*, int, RES_ITEM*, int) {return false;}
void dump_resource(int type, RES *ares, void sendit(void *sock, const char *fmt, ...), void *sock){}
void free_resource(RES *rres, int type){}
union URES {};
RES_TABLE resources[] = {};
URES res_all;

