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
 *  Catalog DB header file
 *
 *  Written by Kern E. Sibbald
 *
 *  Anyone who accesses the database will need to include
 *   this file.
 */

#ifndef __CATS_H_
#define __CATS_H_ 1

/*
   Here is how database versions work.

   While I am working on a new release with database changes, the
   update scripts are in the src/cats directory under the names
   update_xxx_tables.in.  Most of the time, I make database updates
   in one go and immediately update the version, but not always.  If
   there are going to be several updates as is the case with version
   1.37, then I will often forgo changing the version until the last
   update otherwise I will end up with too many versions and a lot
   of confusion.

   When I am pretty sure there will be no more updates, I will
   change the version from 8 to 9 (in the present case), and when I
   am 100% sure there will be no more changes, the update script
   will be copied to the updatedb directory with the correct name
   (in the present case 8 to 9).
 */

/* Current database version number for all drivers */
#define BDB_VERSION 1026

typedef void (DB_LIST_HANDLER)(void *, const char *);
typedef int (DB_RESULT_HANDLER)(void *, int, char **);

/* What kind of database we have */
typedef enum {
   SQL_TYPE_MYSQL      = 0,
   SQL_TYPE_POSTGRESQL = 1,
   SQL_TYPE_SQLITE3    = 2,
   SQL_TYPE_UNKNOWN    = 99
} SQL_DBTYPE;

/* What kind of driver we have */
typedef enum {
   SQL_DRIVER_TYPE_MYSQL      = 0,
   SQL_DRIVER_TYPE_POSTGRESQL = 1,
   SQL_DRIVER_TYPE_SQLITE3    = 2,
   SQL_DRIVER_TYPE_DBI        = 3
} SQL_DRIVER;

void append_filter(POOLMEM **buf, char *cond);

/* Flags for find_next_volume_for_append() & sql_find */
enum {
  fnv_create_vol    = true,
  fnv_no_create_vol = false,
  fnv_prune         = true,
  fnv_no_prune      = false,
  fnv_not_encrypted = false,
  fnv_encrypted     = true,
  fnv_encrypted_any = -1 /* don't care if the volume is encrypted or not */
};

/* ==============================================================
 *
 *  What follows are definitions that are used "globally" for all
 *   the different SQL engines and both inside and external to the
 *   cats directory.
 */

#define faddr_t long

/*
 * Generic definition of a sql_row.
 */
typedef char **SQL_ROW;

/*
 * Generic definition of a a sql_field.
 */
typedef struct sql_field {
   char *name;                        /* name of column */
   int max_length;                    /* max length */
   uint32_t type;                     /* type */
   uint32_t flags;                    /* flags */
} SQL_FIELD;


/*
 * Structure used when calling db_get_query_ids()
 *  allows the subroutine to return a list of ids.
 */
class dbid_list : public SMARTALLOC {
public:
   DBId_t *DBId;                      /* array of DBIds */
   char *PurgedFiles;                 /* Array of PurgedFile flags */
   int num_ids;                       /* num of ids actually stored */
   int max_ids;                       /* size of id array */
   int num_seen;                      /* number of ids processed */
   int tot_ids;                       /* total to process */

   dbid_list();                       /* in sql.c */
   ~dbid_list();                      /* in sql.c */
};

/* Job information passed to create job record and update
 * job record at end of job. Note, although this record
 * contains all the fields found in the Job database record,
 * it also contains fields found in the JobMedia record.
 */
/* Job record */
struct JOB_DBR {
   JobId_t JobId;
   char Job[MAX_NAME_LENGTH];         /* Job unique name */
   char Name[MAX_NAME_LENGTH];        /* Job base name */
   char PriorJob[MAX_NAME_LENGTH];    /* PriorJob name if any */
   char Comment[MAX_NAME_LENGTH];     /* Comment */
   char StatusInfo[MAX_NAME_LENGTH];  /* More information about the status */
   char WriteDevice[MAX_NAME_LENGTH]; /* Device used to write */
   char LastReadDevice[MAX_NAME_LENGTH]; /* Device used to read */

   int JobType;                       /* actually char(1) */
   int JobLevel;                      /* actually char(1) */
   int JobStatus;                     /* actually char(1) */
   DBId_t ClientId;                   /* Id of client */
   DBId_t PoolId;                     /* Id of pool */
   DBId_t FileSetId;                  /* Id of FileSet */
   DBId_t PriorJobId;                 /* Id of migrated (prior) job */
   time_t SchedTime;                  /* Time job scheduled */
   time_t StartTime;                  /* Job start time */
   time_t EndTime;                    /* Job termination time of orig job */
   time_t RealEndTime;                /* Job termination time of this job */
   time_t RealStartTime;              /* Job start time of this job (even VirtualFull) */
   utime_t JobTDate;                  /* Backup time/date in seconds */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint32_t JobErrors;
   uint32_t JobMissingFiles;
   uint64_t JobBytes;
   uint64_t ReadBytes;
   int PurgedFiles;
   int HasBase;
   int Reviewed;                /* In list, values are 0 (unset), 1 and 2 */
   int isVirtualFull;
   double Rate;
   double CompressRatio;
   DBId_t WriteStorageId;
   DBId_t LastReadStorageId;
   int Encrypted;
   char Client[MAX_NAME_LENGTH]; /* Set in db_get_job_record() */

   /* Note, FirstIndex, LastIndex, Start/End File and Block
    * are only used in the JobMedia record.
    */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   uint32_t StartFile;
   uint32_t EndFile;
   uint32_t StartBlock;
   uint32_t EndBlock;

   char cSchedTime[MAX_TIME_LENGTH];
   char cStartTime[MAX_TIME_LENGTH];
   char cEndTime[MAX_TIME_LENGTH];
   char cRealEndTime[MAX_TIME_LENGTH];
   char cRealStartTime[MAX_TIME_LENGTH];
   /* Extra stuff not in DB */
   int     order;                     /* 0 ASC, 1 DESC */
   int     limit;                     /* limit records to display */
   faddr_t rec_addr;
   int32_t FileIndex;                 /* added during Verify */
   utime_t RunTime;
   char FromDate[MAX_TIME_LENGTH];    /* Used for filtering job records ('days' and 'hours' in list cmd) */

   int     CorrNbJob;                 /* used by dbd_get_job_statistics() */
   int     CorrJobBytes;              /* used by dbd_get_job_statistics() */
   int     CorrJobFiles;              /* used by dbd_get_job_statistics() */
};

/* Job Media information used to create the media records
 * for each Volume used for the job.
 */
/* JobMedia record */
struct JOBMEDIA_DBR {
   DBId_t JobMediaId;                 /* record id */
   JobId_t  JobId;                    /* JobId */
   DBId_t MediaId;                    /* MediaId */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   uint32_t StartFile;                /* File for start of data */
   uint32_t EndFile;                  /* End file on Volume */
   uint32_t StartBlock;               /* start block on tape */
   uint32_t EndBlock;                 /* last block */
   uint32_t VolIndex;                 /* Volume seqence no. */
};


/* File Media information used to create the file media records
 * for each file indexed in a volume
 */
struct FILEMEDIA_DBR {
   JobId_t  JobId;                    /* JobId */
   DBId_t MediaId;                    /* MediaId */
   uint32_t FileIndex;                /* File index in the job */
   uint32_t RecordNo;                 /* Record number in the block */
   uint64_t BlockAddress;             /* Address of the block on the volume */
   uint64_t FileOffset;               /* Offset in the file */
};

/* Volume Parameter structure */
struct VOL_PARAMS {
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char MediaType[MAX_NAME_LENGTH];   /* Media Type */
   char Storage[MAX_NAME_LENGTH];     /* Storage name */
   uint32_t VolIndex;                 /* Volume seqence no. */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   int32_t Slot;                      /* Slot */
   uint64_t StartAddr;                /* Start address */
   uint64_t EndAddr;                  /* End address */
   int32_t InChanger;                 /* InChanger flag */
};


/* Attributes record -- NOT same as in database because
 *  in general, this "record" creates multiple database
 *  records (e.g. pathname, filename, fileattributes).
 */
struct ATTR_DBR {
   char *fname;                       /* full path & filename */
   char *link;                        /* link if any */
   char *attr;                        /* attributes statp */
   int32_t FileIndex;
   uint32_t Stream;
   uint32_t FileType;
   uint32_t DeltaSeq;
   JobId_t  JobId;
   DBId_t ClientId;
   DBId_t PathId;
   char *Filename;
   FileId_t FileId;
   char *Digest;
   int DigestType;
};

class OBJECT_DBR {
public:
   OBJECT_DBR() {
      Path = get_pool_memory(PM_FNAME);
      Filename = get_pool_memory(PM_FNAME);
      PluginName = get_pool_memory(PM_FNAME);
      JobIds = get_pool_memory(PM_FNAME);
      reset();
   };

   ~OBJECT_DBR() {
      free_pool_memory(Path);
      free_pool_memory(Filename);
      free_pool_memory(PluginName);
      free_pool_memory(JobIds);
   };

   /* reset memory */
   void reset() {
      JobId = 0;
      ObjectId = 0;
      ObjectSize = 0;
      *JobIds = *Path = *Filename = *PluginName = 0;
      *ObjectCategory = *ObjectType = *ObjectName = *ObjectSource = *ObjectUUID = *ClientName = 0;
      limit = 0;
      order = 0;
      ObjectStatus = 0;
      ObjectCount = 0;
   };

   /* Parse OBJECT record from stream */
   bool parse_plugin_object_string(char **obj_str);
   /* Helper for creating the 'where' part of jcr's related sql query based on fields from the Object */
   void create_db_filter(JCR *jcr, POOLMEM **where);

   DBId_t ObjectId;
   JobId_t  JobId;
   POOLMEM *Path;
   POOLMEM *Filename;
   POOLMEM *PluginName;
   POOLMEM *JobIds;
   char ObjectCategory[MAX_PLUGINOBJ_NAME_LENGTH];
   char ObjectType[MAX_PLUGINOBJ_NAME_LENGTH];
   char ObjectName[MAX_PLUGINOBJ_NAME_LENGTH];
   char ObjectSource[MAX_PLUGINOBJ_NAME_LENGTH];
   char ObjectUUID[MAX_PLUGINOBJ_NAME_LENGTH];
   uint64_t ObjectSize;
   int32_t ObjectStatus;
   uint32_t ObjectCount;

   /* Fields not stored in db directly */
   char ClientName[MAX_NAME_LENGTH];

   int limit;
   int order;
};

struct ROBJECT_DBR {
   char *object_name;
   char *object;
   char *plugin_name;
   char *JobIds;
   uint32_t object_len;
   uint32_t object_full_len;
   uint32_t object_index;
   int32_t  object_compression;
   int32_t FileIndex;
   uint32_t Stream;
   uint32_t FileType;
   JobId_t  JobId;
   DBId_t RestoreObjectId;

   int limit;                   /* Needed to restrict a search */
   DBId_t clientid;
};


/* File record -- same format as database */
struct FILE_DBR {
   FileId_t FileId;
   int32_t FileIndex;
   int32_t FileIndex2;
   JobId_t  JobId;
   char *Filename;              /* Need external storage, should be escaped */
   DBId_t PathId;
   JobId_t  MarkId;
   uint32_t DeltaSeq;
   char LStat[256];
   char Digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
   int DigestType;                    /* NO_SIG/MD5_SIG/SHA1_SIG */
};

/* Pool record -- same format as database */
class POOL_DBR {
public:
   POOL_DBR() { bmemset(this, 0, sizeof(POOL_DBR)); };
   ~POOL_DBR() {  };

   DBId_t PoolId;
   char Name[MAX_NAME_LENGTH];        /* Pool name */
   uint32_t NumVols;                  /* total number of volumes */
   uint32_t MaxVols;                  /* max allowed volumes */
   int32_t LabelType;                 /* Bacula/ANSI/IBM */
   int32_t UseOnce;                   /* set to use once only */
   int32_t UseCatalog;                /* set to use catalog */
   int32_t AcceptAnyVolume;           /* set to accept any volume sequence */
   int32_t AutoPrune;                 /* set to prune automatically */
   int32_t Recycle;                   /* default Vol recycle flag */
   uint32_t ActionOnPurge;            /* action on purge, e.g. truncate the disk volume */
   utime_t  VolRetention;             /* retention period in seconds */
   utime_t  CacheRetention;           /* cache retention period in seconds */   
   utime_t  VolUseDuration;           /* time in secs volume can be used */
   uint32_t MaxVolJobs;               /* Max Jobs on Volume */
   uint32_t MaxVolFiles;              /* Max files on Volume */
   uint64_t MaxVolBytes;              /* Max bytes on Volume */
   uint64_t MaxPoolBytes;             /* Maximum Pool size */
   DBId_t RecyclePoolId;              /* RecyclePool destination when media is purged */
   DBId_t ScratchPoolId;              /* ScratchPool source when media is needed */
   char PoolType[MAX_NAME_LENGTH];
   char LabelFormat[MAX_NAME_LENGTH];
   /* Extra stuff not in DB */
   faddr_t rec_addr;
   uint64_t PoolBytes;            /* Updated after db_get_pool_numvols() */
};

class DEVICE_DBR {
public:
   DBId_t DeviceId;
   char Name[MAX_NAME_LENGTH];        /* Device name */
   DBId_t MediaTypeId;                /* MediaType */
   DBId_t StorageId;                  /* Storage id if autochanger */
   uint32_t DevMounts;                /* Number of times mounted */
   uint32_t DevErrors;                /* Number of read/write errors */
   uint64_t DevReadBytes;             /* Number of bytes read */
   uint64_t DevWriteBytes;            /* Number of bytew written */
   uint64_t DevReadTime;              /* time spent reading volume */
   uint64_t DevWriteTime;             /* time spent writing volume */
   uint64_t DevReadTimeSincCleaning;  /* read time since cleaning */
   uint64_t DevWriteTimeSincCleaning; /* write time since cleaning */
   time_t   CleaningDate;             /* time last cleaned */
   utime_t  CleaningPeriod;           /* time between cleanings */
};

class STORAGE_DBR {
public:
   DBId_t StorageId;
   char Name[MAX_NAME_LENGTH];        /* Device name */
   int AutoChanger;                   /* Set if autochanger */
   int UseProtect;                    /* Set if the Storage Daemon can handle Protected */

   /* Not in database */
   bool created;                      /* set if created by db_create ... */
};

class MEDIATYPE_DBR {
public:
   DBId_t MediaTypeId;
   char MediaType[MAX_NAME_LENGTH];   /* MediaType string */
   int ReadOnly;                      /* Set if read-only */
};

/* Media record -- same as the database */
class MEDIA_DBR {
public:
   MEDIA_DBR() { memset(this, 0, sizeof(MEDIA_DBR)); };
   ~MEDIA_DBR() {  };
   void clear() { memset(this, 0, sizeof(MEDIA_DBR)); };
   void copy(MEDIA_DBR *omr) { memcpy(this, omr, sizeof(MEDIA_DBR)); sid_group = NULL; };

   DBId_t MediaId;                    /* Unique volume id */
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char MediaType[MAX_NAME_LENGTH];   /* Media type */
   char Pool[MAX_NAME_LENGTH];        /* Pool name directly available */
   DBId_t PoolId;                     /* Pool id */
   time_t   FirstWritten;             /* Time Volume first written this usage */
   time_t   LastWritten;              /* Time Volume last written */
   time_t   LabelDate;                /* Date/Time Volume labeled */
   time_t   InitialWrite;             /* Date/Time Volume first written */
   int32_t  LabelType;                /* Label (Bacula/ANSI/IBM) */
   uint32_t VolJobs;                  /* number of jobs on this medium */
   uint32_t VolFiles;                 /* Number of files */
   uint32_t VolBlocks;                /* Number of blocks */
   uint32_t VolParts;                 /* Number of cache parts */
   uint32_t VolCloudParts;            /* Number of cloud parts */
   uint32_t VolMounts;                /* Number of times mounted */
   uint32_t VolErrors;                /* Number of read/write errors */
   uint64_t VolWrites;                /* Number of writes */
   uint64_t VolReads;                 /* Number of reads */
   uint64_t VolBytes;                 /* Number of bytes written */
   uint64_t VolABytes;                /* Size of aligned volume */
   uint64_t VolHoleBytes;             /* The size of Holes */
   uint32_t VolHoles;                 /* Number of holes */
   uint32_t VolType;                  /* Device type of where Volume labeled */
   uint64_t MaxVolBytes;              /* Max bytes to write to Volume */
   uint64_t VolCapacityBytes;         /* capacity estimate */
   uint64_t LastPartBytes;            /* Bytes in last part */
   uint64_t VolReadTime;              /* time spent reading volume */
   uint64_t VolWriteTime;             /* time spent writing volume */
   utime_t  VolRetention;             /* Volume retention in seconds */
   utime_t  CacheRetention;           /* Cache retention period in second */
   utime_t  VolUseDuration;           /* time in secs volume can be used */
   uint32_t ActionOnPurge;            /* action on purge, e.g. truncate the disk volume */
   uint32_t MaxVolJobs;               /* Max Jobs on Volume */
   uint32_t MaxVolFiles;              /* Max files on Volume */
   int32_t  Recycle;                  /* recycle yes/no */
   int32_t  Protected;                /* Protected yes/no */
   int32_t  UseProtect;               /* Use Protected feature yes/no */
   int32_t  VolEncrypted;             /* Volume is encrypted */
   int32_t  Slot;                     /* slot in changer */
   int32_t  Enabled;                  /* 0=disabled, 1=enabled, 2=archived */
   int32_t  InChanger;                /* Volume currently in changer */
   DBId_t   StorageId;                /* Storage record Id */
   uint32_t EndFile;                  /* Last file on volume */
   uint32_t EndBlock;                 /* Last block on volume */
   uint32_t RecycleCount;             /* Number of times recycled */
   char     VolStatus[20];            /* Volume status */
   DBId_t   DeviceId;                 /* Device where Vol last written */
   DBId_t   LocationId;               /* Where Volume is -- user defined */
   DBId_t   ScratchPoolId;            /* Where to move if scratch */
   DBId_t   RecyclePoolId;            /* Where to move when recycled */
   /* Extra stuff not in DB */
   faddr_t rec_addr;                  /* found record address */
   /* Since the database returns times as strings, this is how we pass
    *   them back.
    */
   char    cFirstWritten[MAX_TIME_LENGTH]; /* FirstWritten returned from DB */
   char    cLastWritten[MAX_TIME_LENGTH];  /* LastWritten returned from DB */
   char    cLabelDate[MAX_TIME_LENGTH];    /* LabelData returned from DB */
   char    cInitialWrite[MAX_TIME_LENGTH]; /* InitialWrite returned from DB */
   char   *exclude_list;                   /* Optionnal exclude list for db_find_next_volume() */
   char   *sid_group;                 /* Storageid group string */
   char    sid[30];                   /* edited StorageId */
   bool    set_first_written;
   bool    set_label_date;
   int     limit;                /* Limit the number of records returned with search */
};

#define MAX_UNAME_LENGTH 256
#define MAX_ESCAPE_UNAME_LENGTH MAX_UNAME_LENGTH*2+1

#define MAX_PLUGIN_LENGTH 1024
#define MAX_ESCAPE_PLUGIN_LENGTH MAX_PLUGIN_LENGTH*2+1

/* Client record -- same as the database */
struct CLIENT_DBR {
   DBId_t ClientId;                   /* Unique Client id */
   int AutoPrune;
   utime_t FileRetention;
   utime_t JobRetention;
   char Name[MAX_NAME_LENGTH];        /* Client name */
   char Uname[MAX_UNAME_LENGTH];      /* Uname for client */
   char Plugins[MAX_PLUGIN_LENGTH];   /* Plugin list for this client */

   int limit;                         /* used by search */
};

/* Counter record as in database */
struct COUNTER_DBR {
   char Counter[MAX_NAME_LENGTH];
   int32_t MinValue;
   int32_t MaxValue;
   int32_t CurrentValue;
   char WrapCounter[MAX_NAME_LENGTH];
};


/* FileSet record -- same as the database */
struct FILESET_DBR {
   DBId_t FileSetId;                  /* Unique FileSet id */
   char FileSet[MAX_NAME_LENGTH];     /* FileSet name */
   char MD5[50];                      /* MD5 signature of include/exclude */
   time_t CreateTime;                 /* date created */
   char Content[MAX_PLUGIN_LENGTH];   /* List of plugins in a fileset record */
   /*
    * This is where we return CreateTime
    */
   char cCreateTime[MAX_TIME_LENGTH]; /* CreateTime as returned from DB */
   /* Not in DB but returned by db_create_fileset() */
   bool created;                      /* set when record newly created */
};

class SNAPSHOT_DBR {
public:
   SNAPSHOT_DBR() {
      memset(this, 0, sizeof(SNAPSHOT_DBR));
   };
   ~SNAPSHOT_DBR() {
      reset();
   };
   void debug(int level) {
      Dmsg8(DT_SNAPSHOT|level,
            "Snapshot      %s:\n"
            "  Volume:     %s\n"
            "  Device:     %s\n"
            "  Id:         %d\n"
            "  FileSet:    %s\n"
            "  CreateDate: %s\n"
            "  Client:     %s\n"
            "  Type:       %s\n",
            Name, NPRT(Volume), NPRT(Device), SnapshotId,
            FileSet, CreateDate, Client, Type);
   };
   char *as_arg(POOLMEM **out) {
      bash_spaces(Name);
      bash_spaces(Type);

      if (Volume) {
         bash_spaces(Volume);
      }
      if (Device) {
         bash_spaces(Device);
      }

      Mmsg(out, "name=%s volume=%s device=%s tdate=%d type=%s",
           Name, NPRTB(Volume), NPRTB(Device), CreateTDate, Type);

      unbash_spaces(Name);
      unbash_spaces(Type);
      if (Volume) {
         unbash_spaces(Volume);
      }
      if (Device) {
         unbash_spaces(Device);
      }
      return *out;
   };
   void reset() {
      if (need_to_free) {
         if (Volume) {
            free(Volume);
         }
         if (Device) {
            free(Device);
         }
         if (errmsg) {
            free(errmsg);
         }
         errmsg = Volume = Device = NULL;
      }
      need_to_free = false;
   };
   bool    need_to_free;             /* Need to free the internal memory */
   /* Used when searching snapshots */
   char    created_after[MAX_TIME_LENGTH];
   char    created_before[MAX_TIME_LENGTH];
   bool    expired;                 /* Look for CreateTDate > (NOW - Retention) */
   bool    sorted_client;           /* Results sorted by Client, SnapshotId */
   int     status;                  /* Status of the snapshot */

   DBId_t  SnapshotId;              /* Unique Snapshot ID */
   DBId_t  JobId;                   /* Related JobId */
   DBId_t  FileSetId;               /* FileSetId if any */
   DBId_t  ClientId;                /* From which client this snapshot comes */
   char    Name[MAX_NAME_LENGTH];   /* Snapshot Name */
   char    FileSet[MAX_NAME_LENGTH];/* FileSet name if any */
   char    Client[MAX_NAME_LENGTH]; /* Client name */
   char    Type[MAX_NAME_LENGTH];   /* zfs, btrfs, lvm, netapp, */
   char    Comment[MAX_NAME_LENGTH];/* Comment */
   char    CreateDate[MAX_TIME_LENGTH]; /* Create date as string */
   time_t  CreateTDate;             /* Create TDate (in sec, since epoch) */
   char   *Volume;                  /* Volume taken in snapshot */
   char   *Device;                  /* Device, Pool, Directory, ...  */
   char   *errmsg;                  /* Error associated with a snapshot */
   utime_t Retention;               /* Number of second before pruning the snapshot */
   uint64_t Size;                   /* Snapshot Size */
};

class TAG_DBR
{
public:
   char    Client[MAX_NAME_LENGTH];  /* Client name */
   char    Job[MAX_NAME_LENGTH];     /* Job name */
   char    Pool[MAX_NAME_LENGTH];    /* Pool name */
   char    Volume[MAX_NAME_LENGTH];  /* Volume name */
   char    Comment[MAX_NAME_LENGTH]; /* Comment */
   char    Name[MAX_NAME_LENGTH];    /* Name */
   char    Object[MAX_NAME_LENGTH];  /* Object name */
   JobId_t JobId;                    /* JobId */

   bool    all;
   int     limit;                    /* Used in search */

   TAG_DBR() {
      zero();
   };
   void zero() {
      limit = JobId = 0;
      all = false;
      *Object = *Client = *Job = *Pool = *Volume = *Comment = *Name = '\0';
   };
   virtual ~TAG_DBR(){};

   /* Scan a TAG_DBR object to help with SQL queries */
   void gen_sql(JCR *jcr, BDB *db,
                const char **table, /* Table name (Client, Job, Media...) */
                const char **name,  /* Name of the record (Name, VolumeName, JobId) */
                const char **id,    /* Id of the record (ClientId, JobId, MediaId) */
                char *esc,          /* Escaped name of the resource */
                char *esc_name,     /* Escaped name of the tag */
                uint64_t *aclbits,  /* ACL used */
                uint64_t *aclbits_extra); /* Extra ACL used */
};



/* Used to search in MetaEmail and MetaAttachment table */
#define MAX_SEARCH_LENGTH 512
#define MAX_SEARCH_LENGTH_ESCAPED (MAX_SEARCH_LENGTH * 2 + 1)

class META_DBR: public SMARTALLOC
{
public:
   int64_t MinSize;
   int64_t MaxSize;
   int     HasAttachment;
   int     isDraft;
   int     isRead;
   int     isInline;
   uint64_t offset;
   uint32_t limit;
   int     order;
   int     orderby;             // 0: JobId, FileIndex  1: EmailTime
   bool    all;
   bool    alljobs;             // query all jobs, else return only the last version
   char   *JobIds;

   char    Id[MAX_SEARCH_LENGTH];
   char    Tenant[MAX_SEARCH_LENGTH];
   char    Owner[MAX_SEARCH_LENGTH];
   char    ClientName[MAX_NAME_LENGTH];
   char    From[MAX_SEARCH_LENGTH];
   char    To[MAX_SEARCH_LENGTH];
   char    Cc[MAX_SEARCH_LENGTH];
   char    Tags[MAX_SEARCH_LENGTH];
   char    Subject[MAX_SEARCH_LENGTH];
   char    BodyPreview[MAX_SEARCH_LENGTH];
   char    Type[16];            // Email or Attachment
   char    ConversationId[MAX_NAME_LENGTH];
   char    Category[MAX_SEARCH_LENGTH];
   char    MinTime[MAX_NAME_LENGTH];
   char    MaxTime[MAX_NAME_LENGTH];
   char    Plugin[MAX_NAME_LENGTH];
   char    Name[MAX_SEARCH_LENGTH];
   char    FolderName[MAX_SEARCH_LENGTH];
   char    ContentType[MAX_SEARCH_LENGTH];
   char    errmsg[MAX_NAME_LENGTH];
   META_DBR(): MinSize(-1), MaxSize(-1), HasAttachment(-1),
               isDraft(-1), isRead(-1), isInline(-1), offset(0), limit(512),
               order(0), orderby(0), all(false),alljobs(false)
   {
      JobIds = NULL;
      *Id = *Tenant = *Owner = 0;
      *ClientName = *From = *To = *Cc = *Subject = *Tags = 0;
      *BodyPreview = *Type = *ConversationId = *Category = 0;
      *FolderName = *Name = *MinTime = *MaxTime = *Plugin = 0;
      *ContentType = 0;
      *errmsg = 0;
   };
   ~META_DBR() {};
   void get_important_keys(POOLMEM **dest);
   void get_all_keys(POOLMEM **dest);
   void create_db_filter(JCR *jcr, BDB *db, POOLMEM **dest);
   bool check();                /* check if valid */
};

class FILEEVENT_DBR: public SMARTALLOC
{
public:
   DBId_t FileIndex;            // FileIndex of the File
   char *JobId;                // JobId of the file. For list only: can specify list or range of jobs
   DBId_t SourceJobId;          // Verify/Restore JobId
   char   Type;                 // antivirus, malware, lost file
   int    Severity;             // level of severity (0 OK, 100 Important)
   char   Description[MAX_NAME_LENGTH];
   char   Source[MAX_NAME_LENGTH];
   FILEEVENT_DBR(): FileIndex(0), JobId(NULL), Type(0),
                    Severity(0)
   {
      *Description = *Source = 0;
   };
   ~FILEEVENT_DBR() {};

   bool unpack(uint32_t stream, const char *data, int data_len) {
      SourceJobId = FileIndex = 0;
      if (scan_string(data, "%c %d %127s %127s 0", &Type, &Severity, Source, Description) == 4) {
         unbash_spaces(Source);
         unbash_spaces(Description);
         return true;
      }
      Dmsg1(50, "Got incorrect stream for FileEvent %s\n", data);
      return false;
   };
};

/* Call back context for getting a 32/64 bit value from the database */
class db_int64_ctx {
public:
   int64_t value;                     /* value returned */
   int count;                         /* number of values seen */

   db_int64_ctx() : value(0), count(0) {};
   ~db_int64_ctx() {};
private:
   db_int64_ctx(const db_int64_ctx&);            /* prohibit pass by value */
   db_int64_ctx &operator=(const db_int64_ctx&); /* prohibit class assignment */
};

/* Call back context for getting a list of comma separated strings from the
 * database
 */
class db_list_ctx {
public:
   POOLMEM *list;                     /* list */
   int count;                         /* number of values seen */

   db_list_ctx() { list = get_pool_memory(PM_FNAME); reset(); }
   ~db_list_ctx() { free_pool_memory(list); list = NULL; }
   void reset() { *list = 0; count = 0;}
   void add(const db_list_ctx &str) {
      if (str.count > 0) {
         if (*list) {
            pm_strcat(list, ",");
         }
         pm_strcat(list, str.list);
         count += str.count;
      }
   }
   void add(const char *str) {
      if (count > 0) {
         pm_strcat(list, ",");
      }
      pm_strcat(list, str);
      count++;
   }
private:
   db_list_ctx(const db_list_ctx&);            /* prohibit pass by value */
   db_list_ctx &operator=(const db_list_ctx&); /* prohibit class assignment */
};

/* sql_query flags */
#define QF_STORE_RESULT 0x01
 
/* sql_list.c */
enum e_list_type {
   HORZ_LIST,                   /* list */
   VERT_LIST,                   /* llist */
   ARG_LIST,                    /* key1=v1 key2=v2 key3=v3 */
   JSON_LIST,
   FAILED_JOBS,
   INCOMPLETE_JOBS,
   LAST_JOBS
};
 
#include "bdb.h"
#include "protos.h"
#include "jcr.h"
#include "sql_cmds.h"


/* Object used in db_list_xxx function */
class LIST_CTX {
public:
   char line[256];              /* Used to print last dash line */
   int32_t num_rows;

   e_list_type type;            /* Vertical/Horizontal */
   DB_LIST_HANDLER *send;       /* send data back */
   bool once;                   /* Used to print header one time */
   void *ctx;                   /* send() user argument */
   BDB *mdb;
   JCR *jcr;

   void empty() {
      once = false;
      line[0] = '\0';
   }

   void send_dashes() {
      if (*line) {
         send(ctx, line);
      }
   }

   LIST_CTX(JCR *j, BDB *m, DB_LIST_HANDLER *h, void *c, e_list_type t) {
      line[0] = '\0';
      once = false;
      num_rows = 0;
      type = t;
      send = h;
      ctx = c;
      jcr = j;
      mdb = m;
   }
};

class META_JSON: public SMARTALLOC
{
public:
   META_JSON(){};

   virtual ~META_JSON(){};
   bool parse(JCR *jcr, BDB *db,
              DBId_t jid, int64_t fidx,
              const char *value,
              int len,
              POOLMEM **dest);
};


/* Functions exported by sql.c for use within the cats directory. */
void json_list_begin(void *vctx, const char *title);
void json_list_end(void *vctx, int errcode, const char *errmsg);
int list_result(void *vctx, int cols, char **row);
int list_result(JCR *jcr, BDB *mdb, const char *title, DB_LIST_HANDLER *send, void *ctx, e_list_type type);
int get_sql_record_max(JCR *jcr, BDB *mdb);
void list_dashes(BDB *mdb, DB_LIST_HANDLER *send, void *ctx);

void print_dashes(BDB *mdb);
void print_result(BDB *mdb);
int QueryDB(const char *file, int line, JCR *jcr, BDB *db, char *select_cmd);
int InsertDB(const char *file, int line, JCR *jcr, BDB *db, char *select_cmd);
int DeleteDB(const char *file, int line, JCR *jcr, BDB *db, char *delete_cmd);
void split_path_and_file(JCR *jcr, BDB *mdb, const char *fname);

/* Helper functions */
void parse_restore_object_string(char **obj_str, ROBJECT_DBR *r_r);

#endif  /* __CATS_H_ */
