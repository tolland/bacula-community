# -*- coding: UTF-8 -*-
#
#  Bacula(R) - The Network Backup Solution
#
#   Copyright (C) 2000-2022 Kern Sibbald
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
#     Copyright (c) 2019 by Inteos sp. z o.o.
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

import logging
import pathlib

from baculak8s.entities.file_info import NOT_EMPTY_FILE
from baculak8s.plugins.k8sbackend.k8sfileinfo import *
from baculak8s.plugins.k8sbackend.k8sutils import *
from baculak8s.util.size_util import k8s_size_to_int


def storageclass_read(storagev1api, name):
    return storagev1api.read_storage_class(name)


def storageclass_list_all(storagev1api, scfilter=None, estimate=False):
    sclist = {}
    storageclass = storagev1api.list_storage_class(watch=False)
    for sc in storageclass.items:
        if scfilter is not None and len(scfilter) > 0:
            logging.debug("scfilter-glob-for: {}".format(sc.metadata.name))
            found = False
            for scglob in scfilter:
                logging.debug("checking scglob: {}".format(scglob))
                if pathlib.Path(sc.metadata.name).match(scglob):
                    found = True
                    logging.debug('Found.')
                    break
            if not found:
                continue
        scdata = storageclass_read(storagev1api, sc.metadata.name)
        spec = encoder_dump(scdata)
        sclist[sc.metadata.name] = {
            'spec': spec if not estimate else None,
            'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_STORAGECLASS,
                              name=sc.metadata.name,
                              ftype=NOT_EMPTY_FILE,
                              size=len(spec),
                              creation_timestamp=scdata.metadata.creation_timestamp),
        }
    return sclist


def storageclass_names(storagev1api):
    sclist = []
    storageclass = storagev1api.list_storage_class(watch=False)
    for sc in storageclass.items:
        sclist.append(["storageclass", sc.metadata.name])
    return sclist


def storageclass_list_all_names(storagev1api):
    sclist = {}
    storageclass = storagev1api.list_storage_class(watch=False)
    # logging.debug(storageclass)
    for sc in storageclass.items:
        sclist[sc.metadata.name] = {
            'fi': FileInfo(name="/{}/{}".format(K8SObjType.K8SOBJ_STORAGECLASS_Path, sc.metadata.name),
                           ftype=NOT_EMPTY_FILE,
                           size=1024,   # arbitrary file size
                           uid=0, gid=0,
                           mode=DEFAULT_FILE_MODE,
                           nlink=1,
                           modified_at=NOW_TIMESTAMP,
                           accessed_at=NOW_TIMESTAMP,
                           created_at=k8stimestamp_to_unix_timestamp(sc.metadata.creation_timestamp)),
        }
    return sclist


def get_provisioner(storagev1api, storage_name: str):
    storageclass = storagev1api.patch_storage_class(storage_name, {})
    if storageclass is not None:
        return storageclass.provisioner
    return None

"""
{'allow_volume_expansion': True,
 'allowed_topologies': None,
 'api_version': None,
 'kind': None,
 'metadata': {'annotations': {'storageclass.kubernetes.io/is-default-class': 'true'},
              'cluster_name': None,
              'creation_timestamp': datetime.datetime(2020, 7, 23, 12, 0, 2, tzinfo=tzlocal()),
              'deletion_grace_period_seconds': None,
              'deletion_timestamp': None,
              'finalizers': None,
              'generate_name': None,
              'generation': None,
              'initializers': None,
              'labels': {'addonmanager.kubernetes.io/mode': 'EnsureExists'},
              'managed_fields': None,
              'name': 'standard',
              'namespace': None,
              'owner_references': None,
              'resource_version': '265',
              'self_link': '/apis/storage.k8s.io/v1/storageclasses/standard',
              'uid': 'b859eec1-8055-4cad-b9a0-fc13ef01a21a'},
 'mount_options': None,
 'parameters': {'type': 'pd-standard'},
 'provisioner': 'kubernetes.io/gce-pd',
 'reclaim_policy': 'Delete',
 'volume_binding_mode': 'Immediate'}
"""


def storageclass_restore(storagev1api, file_info, file_content):
    logging.debug("storageclass_restore:fileinfo: {}".format(file_info))
    sc = encoder_load(file_content, file_info.name)
    metadata = prepare_metadata(sc.metadata)
    # Instantiate the storageclass object
    storageclass = client.V1StorageClass(
        api_version=sc.api_version,
        kind="StorageClass",
        metadata=metadata,
        allow_volume_expansion=sc.allow_volume_expansion,
        allowed_topologies=sc.allowed_topologies,
        mount_options=sc.mount_options,
        parameters=sc.parameters,
        provisioner=sc.provisioner,
        reclaim_policy=sc.reclaim_policy,
        volume_binding_mode=sc.volume_binding_mode,
    )
    if file_info.objcache is not None:
        # object exist so we replace it
        response = storagev1api.replace_storage_class(k8sfile2objname(file_info.name), body=storageclass, pretty='true')
    else:
        # object does not exist, so create one as required
        response = storagev1api.create_storage_class(body=storageclass, pretty='true')
    return {'response': response}
