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
/**
 * @file kubernetes-fd.c
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * Modified by: Francisco Manuel Garcia Botella (francisco.garcia@baculasystems.com)
 * @brief This is a Bacula Kubernetes Plugin with metaplugin interface.
 * @version 2.2.0
 * @date 2024-02-20
 *
 * @copyright Copyright (c) 2021 All rights reserved.
 *            IP transferred to Bacula Systems according to agreement.
 */

#include "kubernetes-fd.h"

/* Plugin Info definitions */
const char *PLUGIN_LICENSE       = "Bacula AGPLv3";
const char *PLUGIN_AUTHOR        = "Radoslaw Korzeniewski, Francisco Manuel Garcia Botella";
const char *PLUGIN_DATE          = "February 2024";
const char *PLUGIN_VERSION       = "2.2.0"; // TODO: should synchronize with kubernetes-fd.json
const char *PLUGIN_DESCRIPTION   = "Bacula Kubernetes Plugin";

/* Plugin compile time variables */
const char *PLUGINPREFIX         = "kubernetes:";
const char *PLUGINNAME           = "kubernetes";
const char *PLUGINNAMESPACE      = "@kubernetes";
const bool CUSTOMNAMESPACE       = true;
const bool CUSTOMPREVJOBNAME     = false;
const char *PLUGINAPI            = "3";
const char *BACKEND_CMD          = "k8s_backend";
const int32_t CUSTOMCANCELSLEEP  = 0;

checkFile_t checkFile = NULL;
const bool CORELOCALRESTORE = false;
const bool ACCURATEPLUGINPARAMETER = true;
const int ADDINCLUDESTRIPOPTION = 0;
const bool DONOTSAVE_FT_PLUGIN_CONFIG = false;
const uint32_t BACKEND_TIMEOUT = 0; // use default

#ifdef DEVELOPER
const metadataTypeMap plugin_metadata_map[] = {{"METADATA_STREAM", plugin_meta_blob}};
#else
const metadataTypeMap plugin_metadata_map[] = {{NULL, plugin_meta_invalid}};
#endif
