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
/**
 * @file commctx.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a simple smart array list (alist) resource guard conceptually based on C++11 - RAII.
 * @version 1.1.0
 * @date 2020-12-23
 *
 * @copyright Copyright (c) 2020 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#ifndef _SMARTALIST_H_
#define _SMARTALIST_H_

// This is for custom alist iterator
//    which is not working as Bacula can't use any of the modern C++ headers (yet?)
// #if __cplusplus >= 201104
// #include <iterator> // For std::forward_iterator_tag
// #include <cstddef>  // For std::ptrdiff_t
// #endif

/**
 * @brief This is a simple smart array list (alist) resource guard conceptually based on C++11 - RAII.
 *
 * @tparam T is a type for a pointer to allocate on smart_alist.
 */
template <typename T>
class smart_alist : public alist
{
private:
   void _destroy(bool _remove_items = true)
   {
      if (items)
      {
         for (int i = 0; i < max_items; i++)
         {
            if (items[i])
            {
               T *item = (T*)items[i];
               items[i] = NULL;
               delete item;
            }
         }
         if (_remove_items)
         {
            free(items);
            items = NULL;
         }
      }
      num_items = 0;
      last_item = 0;
      max_items = 0;
   }

public:
   // This is a custom alist iteration for modern C++
   // #if __cplusplus >= 201104
   //    struct Iterator
   //    {
   //       using iterator_category = std::forward_iterator_tag;
   //       using difference_type   = std::ptrdiff_t;
   //       using value_type        = T;
   //       using pointer           = T*;  // or also value_type*
   //       using reference         = T&;  // or also value_type&

   //    private:
   //       pointer m_ptr;

   //    public:
   //       Iterator(pointer ptr) : m_ptr(ptr) {}
   //       reference operator*() const { return *m_ptr; }
   //       pointer operator->() { return m_ptr; }

   //       // Prefix increment
   //       Iterator& operator++() { m_ptr++; return *this; }

   //       // Postfix increment
   //       Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

   //       friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
   //       friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };
   //    };

   //    // our iterator interface
   //    inline Iterator begin() { return Iterator(&items[0]); }
   //    inline Iterator end() { return Iterator(NULL); }
   // #endif

   smart_alist(int num = 10) : alist(num, not_owned_by_alist) {}
   ~smart_alist() { _destroy(); }

   inline void smart_destroy() { _destroy(); }

   // This is a simple copy operator
   // it requires a T(T*) constructor to work correctly
   inline smart_alist<T> &operator=(const smart_alist<T> &other)
   {
      if (items != other.items)
      {
         _destroy(false);
         if (other.items)
         {
            for (int i = 0; i < other.max_items; i++)
            {
               if (other.items[i])
               {
                  T *item = New(T((const T*)other.items[i]));
                  this->append(item);
               }
            }
         }
      }
      return *this;
   }
#if __cplusplus >= 201104
   // This is a simple move operator
   inline smart_alist<T> &operator=(smart_alist<T> &&other)
   {
      if (items != other.items)
      {
         _destroy();
         items = other.items;
         other.init(other.num_grow, own_items);
      }
      return *this;
   }
#endif
};

#endif   /* _SMARTALIST_H_ */
