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
 * Generic catalog class methods. 
 * 
 * Note: at one point, this file was assembled from parts of other files 
 *  by a programmer, and other than "wrapping" in a class, which is a trivial  
 *  change for a C++ programmer, nothing substantial was done, yet all the  
 *  code was recommitted under this programmer's name.  Consequently, we  
 *  undo those changes here.  
 */ 
 
#include "bacula.h" 
 
#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL 
 
#include "cats.h" 

static int dbglvl=100;
 
void append_filter(POOLMEM **buf, char *cond)
{
   if (*buf[0] != '\0') {
      pm_strcat(buf, " AND ");
   } else {
      pm_strcpy(buf, " WHERE ");
   }

   pm_strcat(buf, cond);
}

static void append_AND_OR_filter(bool and_or, POOLMEM **buf, char *cond)
{
   if (*buf[0] != '\0') {
      if (and_or) {
         pm_strcat(buf, " OR ");
      } else {
         pm_strcat(buf, " AND ");
      }
   } else {
      if (and_or) {
         pm_strcpy(buf, " WHERE ( ");
      } else {
         pm_strcat(buf, " WHERE ");
      }
   }

   pm_strcat(buf, cond);
}

bool BDB::bdb_match_database(const char *db_driver, const char *db_name, 
                             const char *db_address, int db_port) 
{ 
   BDB *mdb = this; 
   bool match; 
 
   if (db_driver) { 
      match = strcasecmp(mdb->m_db_driver, db_driver) == 0 && 
              bstrcmp(mdb->m_db_name, db_name) && 
              bstrcmp(mdb->m_db_address, db_address) && 
              mdb->m_db_port == db_port && 
              mdb->m_dedicated == false; 
   } else { 
      match = bstrcmp(mdb->m_db_name, db_name) && 
              bstrcmp(mdb->m_db_address, db_address) && 
              mdb->m_db_port == db_port && 
              mdb->m_dedicated == false; 
   } 
   return match; 
} 
 
BDB *BDB::bdb_clone_database_connection(JCR *jcr, bool mult_db_connections) 
{ 
   BDB *mdb = this; 
   /* 
    * See if its a simple clone e.g. with mult_db_connections set to false 
    * then we just return the calling class pointer. 
    */ 
   if (!mult_db_connections) { 
      mdb->m_ref_count++; 
      return mdb; 
   } 
 
   /* 
    * A bit more to do here just open a new session to the database. 
    */ 
   return db_init_database(jcr, mdb->m_db_driver, mdb->m_db_name, 
             mdb->m_db_user, mdb->m_db_password, mdb->m_db_address,
             mdb->m_db_port, mdb->m_db_socket,
             mdb->m_db_ssl_mode, mdb->m_db_ssl_key,
             mdb->m_db_ssl_cert, mdb->m_db_ssl_ca,
             mdb->m_db_ssl_capath, mdb->m_db_ssl_cipher,
             true, mdb->m_disabled_batch_insert);
} 
 
const char *BDB::bdb_get_engine_name(void) 
{ 
   BDB *mdb = this; 
   switch (mdb->m_db_driver_type) { 
   case SQL_DRIVER_TYPE_MYSQL: 
      return "MySQL"; 
   case SQL_DRIVER_TYPE_POSTGRESQL: 
      return "PostgreSQL"; 
   case SQL_DRIVER_TYPE_SQLITE3: 
      return "SQLite3"; 
   default: 
      return "Unknown"; 
   } 
} 
 
/* 
 * Lock database, this can be called multiple times by the same 
 * thread without blocking, but must be unlocked the number of 
 * times it was locked using db_unlock(). 
 */ 
void BDB::bdb_lock(const char *file, int line) 
{ 
   int errstat; 
   BDB *mdb = this; 
 
   if ((errstat = rwl_writelock_p(&mdb->m_lock, file, line)) != 0) { 
      berrno be; 
      e_msg(file, line, M_FATAL, 0, "rwl_writelock failure. stat=%d: ERR=%s\n", 
            errstat, be.bstrerror(errstat)); 
   } 
} 
 
/* 
 * Unlock the database. This can be called multiple times by the 
 * same thread up to the number of times that thread called 
 * db_lock()/ 
 */ 
void BDB::bdb_unlock(const char *file, int line) 
{ 
   int errstat; 
   BDB *mdb = this; 
 
   if ((errstat = rwl_writeunlock(&mdb->m_lock)) != 0) { 
      berrno be; 
      e_msg(file, line, M_FATAL, 0, "rwl_writeunlock failure. stat=%d: ERR=%s\n", 
            errstat, be.bstrerror(errstat)); 
   } 
} 
 
bool BDB::bdb_sql_query(const char *query, int flags) 
{ 
   bool retval; 
   BDB *mdb = this; 
 
   bdb_lock(); 
   retval = sql_query(query, flags); 
   if (!retval) { 
      Mmsg(mdb->errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror()); 
   } 
   bdb_unlock(); 
   return retval; 
} 
 
void BDB::print_lock_info(FILE *fp) 
{ 
   BDB *mdb = this; 
   if (mdb->m_lock.valid == RWLOCK_VALID) { 
      fprintf(fp, "\tRWLOCK=%p w_active=%i w_wait=%i\n",  
         &mdb->m_lock, mdb->m_lock.w_active, mdb->m_lock.w_wait); 
   } 
} 

bool OBJECT_DBR::parse_plugin_object_string(char **obj_str)
{
   bool ret = false;
   int fnl, pnl;
   uint64_t val = 0;

   char *tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }

   if (tmp[strlen(tmp) - 1] == '/') {
      pm_strcpy(Path, tmp);
      unbash_spaces(Path);
   } else {
      split_path_and_filename(tmp, &Path, &pnl, &Filename, &fnl);
      unbash_spaces(Path);
      unbash_spaces(Filename);
   }

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   pm_strcpy(PluginName, tmp);
   unbash_spaces(PluginName);

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   bstrncpy(ObjectCategory, tmp, sizeof(ObjectCategory));
   unbash_spaces(ObjectCategory);

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   bstrncpy(ObjectType, tmp, sizeof(ObjectType));
   unbash_spaces(ObjectType);

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   bstrncpy(ObjectName, tmp, sizeof(ObjectName));
   unbash_spaces(ObjectName);

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   bstrncpy(ObjectSource, tmp, sizeof(ObjectSource));
   unbash_spaces(ObjectSource);

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   bstrncpy(ObjectUUID, tmp, sizeof(ObjectUUID));
   unbash_spaces(ObjectUUID);

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   val = str_to_uint64(tmp);
   ObjectSize = (val > 9223372036854775808ULL /*2^63 */) ? 0 : val;

   /* We should have status and count in the end of the stream */
   tmp = get_next_tag(obj_str);
   if (!tmp) {
      /* We want to work with plugins that does not send the status and count since it's not required,
       * so we're good to proceed here - simply return success */
      ret = true;
      goto bail_out;
   }
   ObjectStatus = (int)*tmp;

   tmp = get_next_tag(obj_str);
   if (!tmp) {
      goto bail_out;
   }
   ObjectCount = str_to_uint64(*obj_str);

   ret = true;

bail_out:
   /* Print whatever was parsed */
   Dmsg11(dbglvl, "Parsed PluginObject: Path: %s Fname: %s PluginName: %s Category: %s "
                  "Type: %s Name: %s Source: %s  UUID: %s Size: %lld Status: %d Count: %lld\n",
                  Path, Filename, PluginName, ObjectCategory, ObjectType, ObjectName, ObjectSource,
                  ObjectUUID, ObjectSize, (char)ObjectStatus, ObjectCount);

   if (!ret) {
      /* Reset parsed fields */
      reset();
   }

   return ret;
}

void OBJECT_DBR::create_db_filter(JCR *jcr, POOLMEM **where)
{
   POOL_MEM esc(PM_MESSAGE), tmp(PM_MESSAGE);

   if (ObjectId > 0) {
      Mmsg(tmp, " Object.ObjectId=%lu", ObjectId);
      append_filter(where, tmp.c_str());
   } else {
      if (JobId != 0) {
         Mmsg(tmp, " Object.JobId=%lu", JobId);
         append_filter(where, tmp.c_str());
      }
      if (is_a_number_list(JobIds)) {
         Mmsg(tmp, " Object.JobId IN (%s) ", JobIds);
         append_filter(where, tmp.c_str());
      }
      if (Path[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), Path, strlen(Path));
         Mmsg(tmp, " Object.Path='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (Filename[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), Filename, strlen(Filename));
         Mmsg(tmp, " Object.Filename='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (PluginName[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), PluginName, strlen(PluginName));
         Mmsg(tmp, " Object.PluginName='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ObjectCategory[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ObjectCategory, strlen(ObjectCategory));
         Mmsg(tmp, " Object.ObjectCategory='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ObjectType[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ObjectType, strlen(ObjectType));
         Mmsg(tmp, " Object.ObjectType='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ObjectName[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ObjectName, strlen(ObjectName));
         Mmsg(tmp, " Object.Objectname='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ObjectSource[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ObjectSource, strlen(ObjectSource));
         Mmsg(tmp, " Object.ObjectSource='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ObjectUUID[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ObjectUUID, strlen(ObjectUUID));
         Mmsg(tmp, " Object.ObjectUUID='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ObjectSize > 0) {
         Mmsg(tmp, " Object.ObjectSize=%llu", ObjectSize);
         append_filter(where, tmp.c_str());
      }

      if (ObjectStatus != 0) {
         Mmsg(tmp, " Object.ObjectStatus='%c'", ObjectStatus);
         append_filter(where, tmp.c_str());
      }
   }
}

void parse_restore_object_string(char **r_obj_str, ROBJECT_DBR *robj_r)
{
   char *p = *r_obj_str;
   int len;

   robj_r->FileIndex = str_to_int32(p);        /* FileIndex */
   skip_nonspaces(&p);
   skip_spaces(&p);
   robj_r->FileType = str_to_int32(p);        /* FileType */
   skip_nonspaces(&p);
   skip_spaces(&p);
   robj_r->object_index = str_to_int32(p);    /* Object Index */
   skip_nonspaces(&p);
   skip_spaces(&p);
   robj_r->object_len = str_to_int32(p);      /* object length possibly compressed */
   skip_nonspaces(&p);
   skip_spaces(&p);
   robj_r->object_full_len = str_to_int32(p); /* uncompressed object length */
   skip_nonspaces(&p);
   skip_spaces(&p);
   robj_r->object_compression = str_to_int32(p); /* compression */
   skip_nonspaces(&p);
   skip_spaces(&p);

   robj_r->plugin_name = p;                      /* point to plugin name */
   len = strlen(robj_r->plugin_name);
   robj_r->object_name = &robj_r->plugin_name[len+1]; /* point to object name */
   len = strlen(robj_r->object_name);
   robj_r->object = &robj_r->object_name[len+1];      /* point to object */
   robj_r->object[robj_r->object_len] = 0;            /* add zero for those who attempt printing */
   Dmsg7(dbglvl, "oname=%s stream=%d FT=%d FI=%d JobId=%ld, obj_len=%d\nobj=\"%s\"\n",
      robj_r->object_name, robj_r->Stream, robj_r->FileType, robj_r->FileIndex, robj_r->JobId,
      robj_r->object_len, robj_r->object);
}

/****************************************************************
 * Interface to the MetaXXX tables (MetaEmail, MetaAttachments
 *
 * The main idea is to get a JSON object fron the FD/SD that represents data to
 * index in our catalog. We need to verify the input and convert/insert the
 * data into the right table.
 * 
 ****************************************************************/

/* The cJSON lib is a small JSON parser/writer */
#include "lib/cJSON.h"

/* We need one scanner per type and per version of data */
struct json_sql {
   const char *json_name;
   const char *sql_name;
   OutputType  type;
};

/* This class is the common interface for all JSON parser. It can be used in
 * simple mapping. For more complex handling, we can subclass it
 * 
 * We will have a specific implementation for email, attachments, ...
 * Each JSON input must be very carefully checked!!
 */
class META_JSON_SCANNER: public SMARTALLOC
{
public:
   const char      *m_table;
   struct json_sql *m_j2s;

   META_JSON_SCANNER(const char *table, struct json_sql *j2s):
      m_table(table), m_j2s(j2s) {};

   virtual ~META_JSON_SCANNER(){};

   /* Parse a JSON node and validate the input */
   virtual bool parse(JCR *jcr, BDB *db,
                      DBId_t jid, int64_t fidx,
                      cJSON *root,
                      POOLMEM **dest);
};

/* Email JSON data

 {
   "Version": 1,
   "Type" : "EMAIL",
   "EmailBodyPreview" : "-------- Forwarded Message --------\r\nSubject: Re: [Bacula-devel] When 5.1?\r\nDate: Sun, 5 Jun 2011 17:36:33 +0200\r\nFrom: Bastian Friedrich <bastian.friedrich@collax.com>\r\nTo: bacula-devel@lists.sourceforge.net\r\nCC: Kern Sibbald <kern@sibbald.com>",
   "EmailCc" : "",
   "EmailConversationId" : "AAQkADkzMjFhOGZjLTM4NWQtNDU1OC1iODA0LWRmNzRiOTRkZjEzMgAQAFu5M6Q-lKhMhvlmDMcwbug=",
   "EmailFolderName" : "Inbox",
   "EmailFrom" : "eric.bollengier@baculasystems.com",
   "EmailId" : "AAMkADkzMjFhOGZjLTM4NWQtNDU1OC1iODA0LWRmNzRiOTRkZjEzMgBGAAAAAACLSt6kXFgwSaU_laiYbZmvBwBUvb47c4T-R5BYeUNUiTxqAADsn4zsAABUvb47c4T-R5BYeUNUiTxqAADsn8WxAAA=",
   "EmailImportance" : "NORMAL",
   "EmailInternetMessageId" : "<2fe3fc53-e83d-70e6-ebcf-38859c84ec26@baculasystems.com>",
   "EmailIsDraft" : 0,
   "EmailIsRead" : 0,
   "EmailTime" : "Sep 10, 2021, 2:44:36 PM",
   "EmailSubject" : "Fwd: [Bacula-devel] When 5.1?",
   "EmailTags" : "category1, category2",
   "EmailTo" : "jorgegea@jorgegea.onmicrosoft.com",
   "EmailHasAttachment" : 0,
   "Plugin" : "m365"
}
*/

#define SAME_KW(keyword, type) {keyword, keyword, type}

static struct json_sql email_json_v1[] = {
   SAME_KW("EmailTenant", OT_STRING),
   SAME_KW("EmailOwner", OT_STRING),
   SAME_KW("EmailBodyPreview", OT_STRING),
   SAME_KW("EmailCc", OT_STRING),
   SAME_KW("EmailConversationId", OT_STRING),
   SAME_KW("EmailFolderName", OT_STRING),
   SAME_KW("EmailFrom", OT_STRING),
   SAME_KW("EmailId", OT_STRING),
   SAME_KW("EmailImportance", OT_STRING),
   SAME_KW("EmailInternetMessageId", OT_STRING),
   SAME_KW("EmailIsRead", OT_BOOL),
   SAME_KW("EmailIsDraft", OT_BOOL),
   SAME_KW("EmailTime", OT_DATE),
   SAME_KW("EmailSubject", OT_STRING),
   SAME_KW("EmailTags", OT_STRING),
   SAME_KW("EmailTo", OT_STRING),
   SAME_KW("EmailSize", OT_INT),
   SAME_KW("EmailHasAttachment", OT_INT),
   SAME_KW("Plugin", OT_STRING),
   {NULL, NULL, OT_END}
};

/*
{
   "AttachmentContentType" : "application/octet-stream",
   "AttachmentEmailId" : "AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQBGAAAAAAChUr1sDFmcSYm7PK3nvLVxBwB-S4yOymgVRpR5CA4-eilAAABABKybAAB-S4yOymgVRpR5CA4-eilAAABABUnfAAA=",
   "AttachmentId" : "AAMkAGZmZjBlMjI0LTMxMmEtNDFkMi1hM2YxLWEzNjI5MjY4M2JkMQBGAAAAAAChUr1sDFmcSYm7PK3nvLVxBwB-S4yOymgVRpR5CA4-eilAAABABKybAAB-S4yOymgVRpR5CA4-eilAAABABUnfAAABEgAQAKT86cEi1S9PgA8I5xS0vKA=",
   "AttachmentIsInline" : 0,
   "AttachmentName" : "Ancillae.gen",
   "Plugin" : "m365",
   "Type" : "ATTACHMENT",
   "Version" : 1
}
 */
static struct json_sql email_attachment_json_v1[] = {
   SAME_KW("AttachmentTenant", OT_STRING),
   SAME_KW("AttachmentOwner", OT_STRING),
   SAME_KW("AttachmentContentType", OT_STRING),
   SAME_KW("AttachmentEmailId", OT_STRING),
   //SAME_KW("AttachmentId", OT_STRING),
   SAME_KW("AttachmentIsInline", OT_BOOL),
   SAME_KW("AttachmentName", OT_STRING),
   SAME_KW("AttachmentSize", OT_INT),
   SAME_KW("Plugin", OT_STRING),
   {NULL, NULL, OT_END}
};

bool META_JSON_SCANNER::parse(JCR *jcr, BDB *db,
                              DBId_t jid, int64_t fidx,
                              cJSON *root,
                              POOLMEM **dest)
{
   POOL_MEM values, tmp, esc;
   bool status = false;
   bool first = true;
   cJSON *val;
   int len;
   Mmsg(dest, "INSERT INTO %s (", m_table);
   for (int i=0; m_j2s[i].json_name ; i++) {
      if (!first) {
         pm_strcat(dest, ",");
      }
      pm_strcat(dest, m_j2s[i].sql_name);

      val = cJSON_GetObjectItemCaseSensitive(root, m_j2s[i].json_name);
      switch(m_j2s[i].type) {
      case OT_BOOL:
         if (!cJSON_IsNumber(val)) {
            Mmsg(dest, "JSON Error: Unable to find %s", m_j2s[i].json_name);
            goto bail_out;
         }
         Mmsg(tmp, "%c%d",
              first?' ':',',
              val->valuedouble == 0 ? 0 : 1);
         break;
      case OT_DATE:
      {
         const char *def = sql_now[db_get_type_index(db)];
         if (!cJSON_IsString(val) || (val->valuestring == NULL)) {
            Mmsg(dest, "JSON Error: Unable to find %s", m_j2s[i].json_name);
            goto bail_out;
         }
         // TODO: Need to check the date format
         if (val->valuestring[0] != 0) { // not empty string
            def = val->valuestring;
         }
         len = strlen(def);
         esc.check_size(len*2+1);
         db_escape_string(jcr, db, esc.c_str(), def, len);

         Mmsg(tmp, "%c'%s'",
              first?' ':',',
              esc.c_str());
      }
      break;
      case OT_STRING:
         if (!cJSON_IsString(val) || (val->valuestring == NULL)) {
            Mmsg(dest, "JSON Error: Unable to find %s", m_j2s[i].json_name);
            goto bail_out;
         }
         len = strlen(val->valuestring);
         esc.check_size(len*2+1);
         db_escape_string(jcr, db, esc.c_str(), val->valuestring, len);

         Mmsg(tmp, "%c'%s'",
              first?' ':',',
              esc.c_str());

         break;
      case OT_INT:
         if (!cJSON_IsNumber(val)) {
            Mmsg(dest, "JSON Error: Unable to find %s", m_j2s[i].json_name);
            goto bail_out;
         }
         Mmsg(tmp, "%c%lld",
              first?' ':',',
              (int64_t)val->valuedouble);
         break;
      default:
         Mmsg(dest, "Implenentation issue with type %d", m_j2s[i].type);
         goto bail_out;
      }
      first = false;
      pm_strcat(values, tmp.c_str());
   }
   /* Finish the query with job information */
   pm_strcat(dest, ",JobId,FileIndex) VALUES (");
   pm_strcat(dest, values.c_str());
   Mmsg(tmp, ", %lld, %lld)", jid, fidx);
   pm_strcat(dest, tmp.c_str());
   status = true;

bail_out:
   return status;
}

static void *cats_malloc(size_t size)
{
   return malloc(size);
}

/****************************************************************
 * The META_JSON class is here to initilize and find the META_XXX_JSON
 * implementation to use to decode the JSON stream.
 ****************************************************************/
bool META_JSON::parse(JCR *jcr, BDB *db,
                      DBId_t jid, int64_t fidx,
                      const char *string,
                      int len,
                      POOLMEM **dest)
{
   bool status = false;
   cJSON *type = NULL;
   cJSON *version = NULL;
   META_JSON_SCANNER *impl = NULL;

   /* We use our own memory allocator to track orphan buffers */
   cJSON_Hooks hook;
   hook.malloc_fn = cats_malloc;
   hook.free_fn = bfree;
   cJSON_InitHooks(&hook);

   cJSON *json = cJSON_ParseWithLength(string, len);
   if (json == NULL) {
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr != NULL) {
         Mmsg(dest, "JSON Error before: %s\n", error_ptr);
      }
      goto bail_out;
   }
   type = cJSON_GetObjectItemCaseSensitive(json, "Type");
   if (!cJSON_IsString(type) || (type->valuestring == NULL))
   {
      Mmsg(dest, "JSON Error: Unable to find Type");
      goto bail_out;
   }
   version = cJSON_GetObjectItemCaseSensitive(json, "Version");
   if (!cJSON_IsNumber(version) || (version->valueint == 0))
   {
      Mmsg(dest, "JSON Error: Unable to find Version");
      goto bail_out;
   }
   if (strcmp(type->valuestring, "EMAIL") == 0) {
      if (version->valueint >= 1) {
         /* If the parser cannot handle a new format, adjust it */
         impl = New(META_JSON_SCANNER("MetaEmail", email_json_v1));
      }
   } else if (strcmp(type->valuestring, "ATTACHMENT") == 0) {
      if (version->valueint >= 1) {
         impl = New(META_JSON_SCANNER("MetaAttachment", email_attachment_json_v1));
      }
   }
   if (!impl) {
      Mmsg(dest, "JSON Error: Incorrect Type");
      goto bail_out;
   }
   if (!impl->parse(jcr, db, jid, fidx, json, dest)) {
      goto bail_out;
   }
   status = true;

bail_out:
   if (impl) {
      delete impl;
   }
   /* TODO: Need to see if we need to free all members */
   if (json) {
      cJSON_Delete(json);
   }
   return status;
}

void META_DBR::get_important_keys(POOLMEM **where)
{
   if (bstrcasecmp(Type, "email")) {
      Mmsg(where, "EmailTenant, EmailOwner, EmailFrom, EmailTo, EmailTime, EmailSubject, MetaEmail.FileIndex, MetaEmail.JobId");
   } else {
      Mmsg(where, "AttachmentEmailId, AttachmentSize, AttachmentName, MetaAttachment.FileIndex, MetaAttachment.JobId");
   }
}

void META_DBR::get_all_keys(POOLMEM **where)
{
   struct json_sql *p;
   const char *sep=NULL;
   if (bstrcasecmp(Type, "email")) {
      p = email_json_v1;
      sep = ",MetaEmail.";
   } else {
      p = email_attachment_json_v1;
      sep = ",MetaAttachment.";
   }

   Mmsg(where, "Meta%s.JobId,Meta%s.FileIndex", Type, Type);
   for (int i = 0; p[i].sql_name ; i++) {
      pm_strcat(where, sep);
      pm_strcat(where, p[i].sql_name);
   }
}

bool META_DBR::check()
{
   if (!Type[0]) {
      bsnprintf(errmsg, sizeof(errmsg), _("Type is not set"));
      return false;
   }
   if (!Tenant[0]) {
      bsnprintf(errmsg, sizeof(errmsg), _("Tenant not set"));
      return false;
   }
   return true;
}

void META_DBR::create_db_filter(JCR *jcr, BDB *db, POOLMEM **where)
{
   bool and_or = false;
   POOL_MEM esc(PM_MESSAGE), tmp(PM_MESSAGE);

   if (bstrcasecmp(Type, "email")) {
      bstrncpy(Type, "Email", sizeof(Type));
   } else {
      bstrncpy(Type, "Attachment", sizeof(Type));
   }

   if (strcmp(Type, "Email") == 0)
   {
      if (all && (*From || *To || *Cc || *Subject || *Tags || *BodyPreview || *Category)) {
         and_or = true;
      }

      if (Id[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailId", Id, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (From[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailFrom", From, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (To[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailTo", To, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (Cc[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailCc", Cc, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (Subject[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailSubject", Subject, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (FolderName[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailFolderName", FolderName, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (Tags[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailTags", Tags, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (BodyPreview[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailBodyPreview", BodyPreview, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }
#if 0
      if (Category[0] != 0) {
         db->search_op(jcr, "MetaEmail.EmailCategory", Category, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }
#endif

      if (and_or) {
         pm_strcat(where, ") ");
      }

      if (ClientName[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ClientName, strlen(ClientName));
         Mmsg(tmp, " Client.Name='%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (ConversationId[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ConversationId, strlen(ConversationId));
         Mmsg(tmp, " MetaEmail.EmailConversationId = '%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (HasAttachment > 0) {
         Mmsg(tmp, " MetaEmail.EmailHasAttachment = %d", HasAttachment);
         append_filter(where, tmp.c_str());
      }

      if (isDraft > 0) {
         Mmsg(tmp, " MetaEmail.EmailIsDraft = %d", isDraft);
         append_filter(where, tmp.c_str());
      }

      if (isRead > 0) {
         Mmsg(tmp, " MetaEmail.EmailIsRead = %d", isRead);
         append_filter(where, tmp.c_str());
      }

      if (MinTime[0]) {
         db_escape_string(jcr, jcr->db, esc.c_str(), MinTime, strlen(MinTime));
         Mmsg(tmp, " MetaEmail.EmailTime >= '%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }

      if (MaxTime[0]) {
         db_escape_string(jcr, jcr->db, esc.c_str(), MaxTime, strlen(MaxTime));
         Mmsg(tmp, " MetaEmail.EmailTime <= '%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }
   } else {
      if (Id[0] != 0) {
         db_escape_string(jcr, jcr->db, esc.c_str(), Id, strlen(Id));
         Mmsg(tmp, " MetaAttachment.AttachmentEmailId = '%s'", esc.c_str());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }
      if (Name[0] != 0) {
         db->search_op(jcr, "MetaAttachment.AttachmentName", Name, esc.handle(), tmp.handle());
         append_AND_OR_filter(and_or, where, tmp.c_str());
      }

      if (isInline >= 0) {
         Mmsg(tmp, " MetaAttachment.AttachmentIsInline = %d", isInline);
         append_filter(where, tmp.c_str());
      }

      if (ContentType[0]) {
         db_escape_string(jcr, jcr->db, esc.c_str(), ContentType, strlen(ContentType));
         Mmsg(tmp, " MetaAttachment.AttachmentContentType = '%s'", esc.c_str());
         append_filter(where, tmp.c_str());
      }
   }

   if (Owner[0]) {
      db_escape_string(jcr, jcr->db, esc.c_str(), Owner, strlen(Owner));
      if (strchr(Owner, '%')) {
         Mmsg(tmp, " Meta%s.%sOwner ILIKE '%s'", Type, Type, esc.c_str());
      } else {
         Mmsg(tmp, " Meta%s.%sOwner = '%s'", Type, Type, esc.c_str());
      }
      append_filter(where, tmp.c_str());
   }

   if (Tenant[0]) {
      db_escape_string(jcr, jcr->db, esc.c_str(), Tenant, strlen(Tenant));
      Mmsg(tmp, " Meta%s.%sTenant = '%s'", Type, Type, esc.c_str());
      append_filter(where, tmp.c_str());
   }

   if (MinSize > 0) {
      Mmsg(tmp, " Meta%s.%sSize >= %llu", Type, Type, MinSize);
      append_filter(where, tmp.c_str());
   }

   if (MaxSize > 0) {
      Mmsg(tmp, " Meta%s.%sSize <= %llu", Type, Type, MaxSize);
      append_filter(where, tmp.c_str());
   }

   if (Plugin[0] != 0) {
      db_escape_string(jcr, jcr->db, esc.c_str(), Plugin, strlen(Plugin));
      Mmsg(tmp, " Meta%s.Plugin='%s'", Type, esc.c_str());
      append_filter(where, tmp.c_str());
   }

   if (is_a_number_list(JobIds)) {
      Mmsg(tmp, " Meta%s.JobId IN (%s)", Type, JobIds);
      append_filter(where, tmp.c_str());
   }
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL */ 
