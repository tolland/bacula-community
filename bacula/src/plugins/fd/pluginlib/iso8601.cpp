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
 * @file iso8601.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula plugin ISO8601 parsing library.
 * @version 1.2.0
 * @date 2020-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "iso8601.h"
#include "lib/btime.h"


#ifdef ISO8601_USE_REGEX

// Adapted from http://delete.me.uk/2005/03/iso8601.html
#define ISO8601_REGEX_RAW              "(?P<year>[0-9]{4})-(?P<month>[0-9]{1,2})-(?P<day>[0-9]{1,2})" \
                                       "T(?P<hour>[0-9]{2}):(?P<minute>[0-9]{2})(:(?P<second>[0-9]{2})(\.(?P<fraction>[0-9]+))?)?" \
                                       "(?P<timezone>Z|[-+][0-9]{2}(:?[0-9]{2})?)?"

#define ISO8601_TIMEZONE_REGEX_RAW     "(?P<prefix>[+-])(?P<hours>[0-9]{2}):?(?P<minutes>[0-9]{2})?"



/**
 * @brief Construct a new ISO8601DateTime::ISO8601DateTime object
 *
 */
ISO8601DateTime::ISO8601DateTime() : status(false)
{
   const int options = REG_EXTENDED | REG_ICASE;
   int rc;
   char prbuf[500];

   status = true;

   rc = regcomp(&ISO8601_REGEX, ISO8601_REGEX_RAW, options);
   if (rc != 0){
      regerror(rc, &ISO8601_REGEX, prbuf, sizeof(prbuf));
      DMsg0(DERROR, "Cannot initialize Bacula regression library for ISO8601_REGEX!\n");
      DMsg1(DERROR, "regex Err=%s\n", prbuf);
      status = false;
   }

   rc = regcomp(&TIMEZONE_REGEX, ISO8601_TIMEZONE_REGEX_RAW, options);
   if (rc != 0){
      regerror(rc, &ISO8601_REGEX, prbuf, sizeof(prbuf));
      DMsg0(DERROR, "Cannot initialize Bacula regression library for TIMEZONE_REGEX!");
      DMsg1(DERROR, "regex Err=%s\n", prbuf);
      status = false;
   }
};
#endif

// #define ISO8601STRSCAN1
// #define ISO8601STRSCAN2
// #define ISO8601STRSCAN3
static const char *scan_formats[] =
{
   "%FT%T%z",
   "%FT%T%t%z",
   "%FT%T%Z",
   "%Y%m%dT%TZ",
   "%Y%m%dT%T%z",
   "%Y%m%dT%T%t%z",
   "%Y-%m-%dT%TZ",
   "%Y-%m-%dT%T%z",
   "%Y-%m-%dT%T%t%z",
   NULL,
};

/**
 * @brief Parses ISO 8601 dates into utime_t values
 *    The timezone is parsed from the date string. However it is quite common to
 *    have dates without a timezone (not strictly correct). In this case the
 *    default timezone specified in default_timezone is used. This is UTC by
 *    default.
 *
 * @param datestring
 * @return utime_t
 */
utime_t ISO8601DateTime::parse_data(const char *datestring)
{
   utime_t time = 0;

   tzset();

   if (strlen(datestring) > 0){
      const char *fmt = scan_formats[0];
      for (int a = 1; fmt != NULL; a++)
      {
#if __cplusplus > 201103L
         struct tm tm {0};
#else
         struct tm tm;
         memset(&tm, 0, sizeof(tm));
#endif
         char *rc = strptime(datestring, fmt, &tm);
         if (rc != NULL && *rc == '\0'){
            // no error scanning time
            time = mktime(&tm) - timezone;
            break;
         }
         fmt = scan_formats[a];
      }
   }

   return time;

#ifdef ISO8601_USE_REGEX
   int rc;

   rc = regexec(&ISO8601_REGEX, datestring.c_str(), 0, NULL, 0);
   if (rc == 0){
      /* found */


   }
   //  m = ISO8601_REGEX.match(datestring)
   //  if not m:
   //      raise ParseError("Unable to parse date string %r" % datestring)
   //  groups = m.groupdict()
   //  tz = parse_timezone(groups["timezone"])
   //  if groups["fraction"] is None:
   //      groups["fraction"] = 0
   //  else:
   //      groups["fraction"] = int(float("0.%s" % groups["fraction"]) * 1e6)

   //  try:
   //      return datetime(int(groups["year"]), int(groups["month"]), int(groups["day"]),
   //                      int(groups["hour"]), int(groups["minute"]), int(groups["second"]),
   //                      int(groups["fraction"]), tz)
   //  except Exception as e:
   //      raise ParseError("Failed to create a valid datetime record due to: %s"
   //                       % e)
#endif
};
