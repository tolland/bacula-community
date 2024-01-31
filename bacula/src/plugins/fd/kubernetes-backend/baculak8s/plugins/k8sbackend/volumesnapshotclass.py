# -*- coding: UTF-8 -*-
#
#  Bacula(R) - The Network Backup Solution
#
#   Copyright (C) 2000-2023 Kern Sibbald
#
#   The original author of Bacula is Kern Sibbald, with contributions
#   from many others, a complete list can be found in the file AUTHORS.
#
#   You may use this file and others of this release according to the
#   license defined in the LICENSE file, which includes the Affero General
#   Public License, v3.0 ("AGPLv3") and some additional permissions and
#   terms pursuant to its AGPLv3 Section 7.
#
#   This notice must be preserved when any source code is
#   conveyed and/or propagated.
#
#   Bacula(R) is a registered trademark of Kern Sibbald.
#
#   Author: Francisco Manuel Garcia Botella, francisco.garcia@baculasystems.com.
#

import logging
import pathlib

K8SOBJ_SNAPSHOT_GROUP = 'snapshot.storage.k8s.io'
K8SOBJ_SNAPSHOT_VERSION = 'v1'
K8SOBJ_SNAPSHOT_PLURAL = 'volumesnapshotclasses'

def volumesnapshotclass_list_all(custom_api, filter_names=None):
    vol_snap_list = {}
    volume_snapshot_classes = custom_api.list_cluster_custom_object(K8SOBJ_SNAPSHOT_GROUP, K8SOBJ_SNAPSHOT_VERSION, K8SOBJ_SNAPSHOT_PLURAL, watch=False)
    
    logging.debug("Volume snapshot classes:{}".format(volume_snapshot_classes))
    for vol_snap in volume_snapshot_classes.get('items'):
        if filter_names is not None and len(filter_names) > 0:
            logging.debug("filter_names-glob-for: {}".format(vol_snap.get('metadata').get('name')))
            found = False
            for vol_glob in filter_names:
                logging.debug("checking vol_glob: {}".format(vol_glob))
                if pathlib.Path(vol_snap.get('metadata').get('name')).match(vol_glob):
                    found = True
                    logging.debug('Found volSnap.')
                    break
            if not found:
                continue
        vol_snap_list[vol_snap.get('metadata').get('name')] = vol_snap
    return vol_snap_list

def get_snapshot_drivers_compatible(custom_api):
    compatible_drivers = []
    try:
        volume_snapshot_classes = volumesnapshotclass_list_all(custom_api)
    except Exception as e:
        logging.error('Controlled exception. This exception is because the volumesnapshotclasses is nos available. We can confirm this fact with `kubectl api-resources`')
        logging.exception(e)
        pass
    
    for vol_snap_name in volume_snapshot_classes:
        compatible_drivers.append(volume_snapshot_classes[vol_snap_name].get('driver'))
    return compatible_drivers
