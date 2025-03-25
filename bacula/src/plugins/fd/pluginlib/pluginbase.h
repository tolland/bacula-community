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
 * @file pluginbase.cpp
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula File Daemon generic plugin interface.
 * @version 1.0.0
 * @date 2021-04-08
 *
 * @copyright Copyright (c) 2021 All rights reserved. IP transferred to Bacula Systems according to agreement.
 */

#include "pluginclass.h"
#define USE_CMD_PARSER
#include "fd_common.h"


#ifndef PLUGINLIB_PLUGINBASE_H
#define PLUGINLIB_PLUGINBASE_H

// Plugin Info definitions
extern const char *PLUGIN_LICENSE;
extern const char *PLUGIN_AUTHOR;
extern const char *PLUGIN_DATE;
extern const char *PLUGIN_VERSION;
extern const char *PLUGIN_DESCRIPTION;

// Plugin linking time variables
extern const char *PLUGINPREFIX;
extern const char *PLUGINNAME;
extern const char *PLUGINNAMESPACE;
extern const bool CUSTOMNAMESPACE;
extern const char *BACKEND_CMD;

// custom checkFile() callback
typedef bRC (*checkFile_t)(bpContext *ctx, char *fname);
extern checkFile_t checkFile;

// the list of valid plugin options
extern const char *valid_params[];

struct metadataTypeMap
{
   const char *command;
   metadata_type type;
};

pluginlib::PLUGINBCLASS *new_plugin_factory(bpContext *ctx);

#endif // PLUGINLIB_PLUGINBASE_H
