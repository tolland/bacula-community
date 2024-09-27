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
 * @file kubernetes-fd.h
 * @author Radosław Korzeniewski (radoslaw@korzeniewski.net)
 * @brief This is a Bacula Kubernetes Plugin with metaplugin interface.
 * @version 1.3.0
 * @date 2021-01-05
 *
 * @copyright Copyright (c) 2021 All rights reserved.
 *            IP transferred to Bacula Systems according to agreement.
 */

#ifndef KUBERNETES_PLUGIN_FD_H
#define KUBERNETES_PLUGIN_FD_H

#include "pluginlib/pluginlib.h"
#include "pluginlib/metaplugin.h"

/*
 * The list of restore options saved to the RestoreObject.
 */
struct ini_items plugin_items_dump[] =
{
//   name                       handler             comment                                        required default
   {"config",                   ini_store_str,      "K8S config file",                                      0, "*None*"},
   {"host",                     ini_store_str,      "K8S API server URL/Host",                              0, "*None*"},
   {"token",                    ini_store_str,      "K8S Bearertoken",                                      0, "*None*"},
//   {"username",                 ini_store_str,      "HTTP Auth username for API",                     0, "*None*"},
//   {"password",                 ini_store_str,      "HTTP Auth password for API",                     0, "*None*"},
   {"verify_ssl",               ini_store_bool,     "K8S API server cert verification",                     0, "True"},
   {"ssl_ca_cert",              ini_store_str,      "Custom CA Certs file to use",                          0, "*None*"},
   {"outputformat",             ini_store_str,      "Output format when saving to file (JSON, YAML)",       0, "RAW"},
   {"fdaddress",                ini_store_str,      "The address for listen to incoming backup pod data",   0, "*FDAddress*"},
   {"fdport",                   ini_store_int32,    "The port for opening socket for listen",               0, "9104"},
   {"pluginhost",               ini_store_str,      "The endpoint address for backup pod to connect",       0, "*FDAddress*"},
   {"pluginport",               ini_store_int32,    "The endpoint port to connect",                         0, "9104"},
   {NULL, NULL, NULL, 0, NULL}
};

// the list of valid plugin options
const char * valid_params[] =
{
   "listing",
   "query",
   "abort_on_error",
   "config",
   "incluster",
   "host",
   "token",
   "verify_ssl",
   "ssl_ca_cert",
   "timeout",
   "debug",
   "backup_mode",
   "namespace",
   "ns",
   "persistentvolume",
   "pv",
   "pvconfig",
   "scconfig",
   "pvcdata",
   "fdaddress",
   "fdport",
   "pluginhost",
   "pluginport",
   "fdcertfile",
   "fdkeyfile",
   "baculaimage",
   "imagepullpolicy",
   "imagepullsecret",
   "outputformat",
   "labels",
   NULL,
};

#endif   // KUBERNETES_PLUGIN_FD_H
