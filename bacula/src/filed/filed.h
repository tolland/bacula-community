/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2025 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 * Bacula File Daemon specific configuration and defines
 *
 *     Kern Sibbald, Jan MMI
 *
 */

//#define TEST_WORKER
#ifdef  TEST_WORKER
#define ERROR_BUFFER_OVERFLOW 1
#define ERROR_SUCCESS 0
#endif

/* acl errors to report per job. */
#define ACL_MAX_ERROR_PRINT_PER_JOB   25

/* xattr errors to report per job. */
#define XATTR_MAX_ERROR_PRINT_PER_JOB 25

#define FILE_DAEMON 1
#include  "lib/htable.h"
#if BEEF
#include  "bee_filed_dedup.h"
#else
#include  "org_filed_dedup.h"
#endif
#include  "filed_conf.h"
#include  "fd_plugins.h"
#include  "fd_snapshot.h"
#include  "findlib/find.h"
#include  "bacgpfs.h"
#include  "bacl.h"
#include  "bxattr.h"
#include  "jcr.h"
#include  "protos.h"                   /* file daemon prototypes */
#include  "lib/runscript.h"
#include  "lib/breg.h"
#include  "suspend.h"
#ifdef HAVE_LIBZ
#include <zlib.h>                     /* compression headers */
#else
#define uLongf uint32_t
#endif
#ifdef HAVE_ZSTD
#include <zstd.h>
#endif
#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#endif

extern CLIENT *me;                    /* "Global" Client resource */
extern bool win32decomp;              /* Use decomposition of BackupRead data */
extern bool no_win32_write_errors;    /* Ignore certain errors */

void terminate_filed(int sig);

struct s_cmds {
   const char *cmd;
   int (*func)(JCR *);
   int access; /* specify if monitors/restricted have access to this function */
};

typedef struct {
   int bacula_client_memory_bufs;
   int bacula_client_memory_heap;
   int bacula_client_memory_maxbufs;
   int bacula_client_memory_maxbytes;
   int bacula_client_memory_smbytes;
   int bacula_client_test_metric;
   int bacula_client_test_metric2;
} fdstatmetrics_t;

class bnet_poll_manager: public SMARTALLOC
{
private:
   pthread_cond_t  m_cond;
   pthread_mutex_t m_mutex;
   int32_t         m_check_done; /* small state machine to sync the two threads */

   int32_t         m_count;     /* Current value, at 0 we send the POLL */
   int32_t         m_check;     /* Configuration file value */
   btime_t         m_last_call; /* Adjust settings dynanically if needed */
   uint64_t        m_sent;      /* Bytes sent since the last call */
   
public:
   bnet_poll_manager(int32_t val);
   ~bnet_poll_manager();

   void init(int32_t val);
   void destroy();
   void send(JCR *jcr, BSOCK *sd);
   void recv(JCR *jcr, const char *msg);
};


void allow_os_suspensions();
void prevent_os_suspensions();
bool update_permanent_stats(void *data);

extern bstatcollect *statcollector;
extern fdstatmetrics_t fdstatmetrics;

