/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2023 Kern Sibbald

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
 * Storage manager classes.
 * All of this code is intented to make managing
 * (accessing, setting, incrementing counters, applying storage policy...) storage easier
 * from the code perspective.
 */

#ifndef STORE_MNGR_H
#define STORE_MNGR_H 1

/* Forward delcaration */
class STORE;

/*
 * Helper class to make managing each storage type (write/read) easier.
 * It contains storage resource ('store' member) which is currently used as well as list of all
 * possible storage resource choices.
 */
class storage {
    private:
      bool write;            /* Write or read storage */
      STORE *store;          /* Selected storage to be used */
      alist *origin_list;    /* Configured Storage (raw list without any policy applied) */
      alist *list;           /* Storage possibilities */
      POOLMEM *source;       /* Where the storage came from */
      POOLMEM *list_str;     /* List of storage names in the list */
      pthread_mutex_t mutex; /* Mutex for accessing items */
      bool unused_stores_decremented; /* Set if only currently used storage has NumConcurrentJobs incremented */

      /* Only when we are a read storage - increment concurrent read counters for all storages on the list */
      bool inc_rstores(JCR *jcr);

      /* Only when we are a write storage - increment concurrent write counters for all storages on the list */
      bool inc_wstores(JCR *jcr);

      const char *print_list(alist *list);

   public:
      storage();

      ~storage();

      /* Determine if we are write or read storage */
      void set_rw(bool write);

      /* Get storage which will be used next */
      STORE *get_store() {
         return store;
      }

      /* Get list of all possible storages.
       * This metod can possibly return list with less storages than the original group.
       * It's because some of it's elements can ba unavailable at that time (e.g. reached maxConcJobs limit). */
      alist *get_list();

      /* Get original list of all storages as assigned by the set() method. */
      alist *get_origin_list();

      /* Get source of the storage (pool, job, commandline, unknown, ...) */
      const char *get_source() const;

      /* Get media type of current storage */
      const char *get_media_type() const;

      /* Set storage override. Remove all previous storage.
       * Can be called for single storage - list consists only one, specified storage then.
       * Can be called for setting a list - internal list consists of same elemets as the list passed
       * as an arg. First item from the list becames storage currently used.
       */
      void set(STORE *storage, const char *source);
      void set(alist *storage, const char *source);

      /* Reset class, remove all items from list, unset storage currently used, clean source */
      void reset();

      /* Set custom storage for next usage (it needs to be an item from the current store list) */
      bool set_current_storage(STORE *storage);

      /* Increment concurrent read/write counters for all storages on the list */
      bool inc_stores(JCR *jcr);

      /* Decrement concurrent read/write counters for all storages on the list */
      void dec_stores();

      void dec_unused_stores();

      void dec_curr_store();

      /* Print all elements of the list (sample result of print_list() -> "File1, File2, File3" */
      const char *print_origin_list();
      const char *print_possible_list();
};


/*
 * Storage Manager class responsible for managing all of the storage used by the JCR.
 * It's holds read as well as write storage resources assigned to the JCR.
 * It is a base class for Storage Policy (hence virtual 'apply_policy' method).
 * Most of member functions are just wrappers around the storage class to make accessing
 * and managin read/write storage in a bit more friendly way.
 *
 */
class StorageManager : public SMARTALLOC {

   protected:
      storage rstore;               /* Read storage */
      storage wstore;               /* Write storage */
      const char *policy;           /* Storage Group Policy used */
      virtual void apply_policy(bool write_store) = 0;

   public:
      virtual void apply_write_policy(JCR*) = 0;
      virtual void apply_read_policy(JCR*) = 0;

      virtual ~StorageManager() {
         reset_rwstorage();
         free(policy);
      };

      StorageManager(const char *policy);

      static const char *get_default_policy() {
         return "ListedOrder";
      }

      /************ READ STORAGE HELPERS ************/
      STORE *get_rstore();

      alist *get_rstore_list();

      alist *get_origin_rstore_list();

      const char *get_rsource() const;

      const char *get_rmedia_type() const;

      void set_rstore(STORE *storage, const char *source);

      void set_rstore(alist *storage, const char *source);

      void reset_rstorage();

      bool inc_read_stores(JCR *jcr);

      void dec_read_stores();

      const char *print_possible_rlist();
      const char *print_origin_rlist();

      /************ WRITE STORAGE HELPERS ************/
      STORE *get_wstore() {
         return wstore.get_store();
      }

      alist *get_wstore_list();

      alist *get_origin_wstore_list();

      const char *get_wsource() const;

      const char *get_wmedia_type() const;

      bool set_current_wstorage(STORE *storage);

      void set_wstorage(STORE *storage, const char *source);

      void set_wstorage(alist *storage, const char *source);

      void reset_wstorage();

      const char *print_possible_wlist();
      const char *print_origin_wlist();

      bool inc_write_stores(JCR *jcr);

      void dec_write_stores();

      void dec_curr_wstore();

      void dec_unused_wstores();

      /************ GENERIC STORAGE HELPERS ************/
      void reset_rwstorage();

      const char *get_policy_name() {
         return policy;
      }
};

/*
 * Least used policy chooses storage from the list which has the least concurrent jobs number.
 */
class LeastUsedStore : public StorageManager {
   private:
      void apply_policy(bool write_store);
   public:
      void apply_write_policy(JCR*);
      void apply_read_policy(JCR*);

   LeastUsedStore() : StorageManager("LeastUsed") {
   }

   ~LeastUsedStore() {
   }
};

/*
 * Default policy for the storage group. It uses first available storage from the list.
 */
class ListedOrderStore : public StorageManager {
   private:
      void apply_policy(bool write_store) {
         /* Do nothing for now */
      }
   public:
      void apply_write_policy(JCR*) {
         return apply_policy(true);
      }
      void apply_read_policy(JCR*) {
         return apply_policy(false);
      }

   ListedOrderStore(): StorageManager("ListedOrder")  {
   }

   ~ListedOrderStore() {
   }
};

class LastBackedUpToStore : public StorageManager {
   private:
      void apply_policy(bool write_store);
   public:
      void apply_write_policy(JCR* jcr);
      void apply_read_policy(JCR* jcr);

   LastBackedUpToStore() : StorageManager("LastBackedUpTo") {
   }

   ~LastBackedUpToStore() {
   }
};

/* Context with per-policy specific data (as of now there's only single uint64_t value available for each policy)*/
class sm_ctx : public SMARTALLOC {
   public:
      STORE *store;
      dlink link;
      uint64_t number; /* Per-policy specific number (e.g. free space, cpu usage...) */

      sm_ctx(STORE *s) {
         store = s;
         number = 0;
      }
};

/*
 * Abstract class for policies which require DIR<->SD querying.
 * Each querying policy has to implement query() and reorder() methods - theese act
 * as a callbacks for QueryStore's generic methods.
 */
class QueryStore : public StorageManager {
   protected:
      /* Policy-specific query method */
      virtual bool query(BSOCK *sd, dlist *d_list, sm_ctx *context) = 0;

      /* Reorder storage list after querying all storages in the list for needed information */
      virtual void reorder_list(alist *list, dlist *d_list) = 0;

   public:
      virtual void apply_policy(bool write_store);

   QueryStore (const char *policy="VirtualPolicy_QueryStore"): StorageManager(policy)  {
   }

   ~QueryStore() {
   }
};

/*
 * Policy which reorders storage list based on FreeSpace available on each Storage
 */
class FreeSpaceStore : public QueryStore {
   private:
      /* Private helper struct */
      struct store_free_size {
         STORE *store;
         uint64_t free_size;
         dlink link;
      };

      /* Comparator for easy list ordering */
      static int cmp(void *item1, void *item2) {
         sm_ctx *ctx1 = (sm_ctx *) item1;
         sm_ctx *ctx2 = (sm_ctx *) item2;
         uint64_t s1 = ctx1->number;
         uint64_t s2 = ctx2->number;

         if (s1 < s2) {
            return 1;
         } else if (s1 > s2) {
            return -1;
         } else {
            return 0;
         }
      }

   protected:
      bool query(BSOCK *sd, dlist *d_list, sm_ctx *context);

      virtual void reorder_list(alist *list, dlist *d_list);

   public:
      void apply_write_policy(JCR*) {
         return apply_policy(true);
      }
      void apply_read_policy(JCR*) {
         return apply_policy(false);
      }

   FreeSpaceStore(): QueryStore("FreeSpace") {
   }

   FreeSpaceStore(const char *policy): QueryStore("policy") {
   }

   virtual ~FreeSpaceStore() {
   }
};

class FreeSpaceLeastUsedStore : public FreeSpaceStore {
   private:
      uint64_t threshold;

   protected:
      virtual void reorder_list(alist *list, dlist *d_list);

   public:
      FreeSpaceLeastUsedStore(uint64_t thres=0): FreeSpaceStore("FreeSpaceLeastUsed") {
         threshold = thres;
      }

      virtual ~FreeSpaceLeastUsedStore() {
      }
};


#endif // STORE_MNGR_H