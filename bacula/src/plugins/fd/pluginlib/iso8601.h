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
 * @file iso8601.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin ISO8601 parsing library.
 * @version 1.2.0
 * @date 2020-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#ifndef PLUGINLIB_ISO8601_H
#define PLUGINLIB_ISO8601_H

#include "pluginlib.h"
#include "lib/bregex.h"


// do not use regex for parsing
#undef ISO8601_USE_REGEX

class ISO8601DateTime : public SMARTALLOC
{
private:
#ifdef ISO8601_USE_REGEX
   regex_t ISO8601_REGEX;
   regex_t TIMEZONE_REGEX;
   bool status;
#endif

public:
#ifdef ISO8601_USE_REGEX
   ISO8601DateTime();
#else
#if __cplusplus > 201103L
   ISO8601DateTime() = default;
   ~ISO8601DateTime() = default;
#else
   ISO8601DateTime() {};
   ~ISO8601DateTime() {};
#endif
#endif

   utime_t parse_data(const char * datestring);
   inline utime_t parse_data(POOL_MEM &datestring) { return parse_data(datestring.c_str()); }
#ifdef ISO8601_USE_REGEX
   inline bool isready() { return status; };
#endif
};

#endif   // PLUGINLIB_ISO8601_H
