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

import kubernetes
from baculak8s.util.size_util import k8s_size_to_int
from baculak8s.entities.file_info import NOT_EMPTY_FILE
from baculak8s.plugins.k8sbackend.k8sfileinfo import (K8SObjType, encoder_dump,
                                                      encoder_load,
                                                      k8sfile2objname,
                                                      k8sfileinfo)
from baculak8s.plugins.k8sbackend.k8sutils import prepare_metadata


def persistentvolumeclaims_read_namespaced(corev1api, namespace, name):
    return corev1api.read_namespaced_persistent_volume_claim(name, namespace)

def persistentvolumeclaims_namespaced_only_names(corev1api, namespace, labels=""):
    pvcslist = []
    pvcs = corev1api.list_namespaced_persistent_volume_claim(namespace=namespace, watch=False, label_selector=labels)
    for pvc in pvcs.items:
        pvcslist.append(pvc.metadata.name)
    return pvcslist

def persistentvolumeclaims_namespaced_names(corev1api, namespace, labels=""):
    pvcslist = []
    pvcs = corev1api.list_namespaced_persistent_volume_claim(namespace=namespace, watch=False, label_selector=labels)
    for pvc in pvcs.items:
        pvcspec = pvc.spec
        pvcslist.append([
            "pvcdata",
            pvc.metadata.name,
            pvcspec.storage_class_name,
            pvcspec.resources.requests.get('storage', '-1')
        ])
    return pvcslist


def persistentvolumeclaims_list_namespaced(corev1api, namespace, estimate=False, labels=""):
    pvcslist = {}
    pvcstotalsize = 0
    pvcs = corev1api.list_namespaced_persistent_volume_claim(namespace=namespace, watch=False, label_selector=labels)
    for pvc in pvcs.items:
        pvcdata = persistentvolumeclaims_read_namespaced(corev1api, namespace, pvc.metadata.name)
        spec = encoder_dump(pvcdata)
        logging.debug("PVCDATA-OBJ:{}".format(pvcdata))
        pvcsize = 0
        if (pvcdata.status.capacity is not None):
            pvcsize = k8s_size_to_int(pvcdata.status.capacity['storage'])
        pvcstotalsize += pvcsize

        pvcslist['pvc-' + pvc.metadata.name] = {
            'spec': spec if not estimate else None,
            'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_PVOLCLAIM, nsname=namespace,
                              name=pvc.metadata.name,
                              ftype=NOT_EMPTY_FILE,
                              size=len(spec),
                              creation_timestamp=pvcdata.metadata.creation_timestamp),
        }
    return pvcslist, pvcstotalsize


def persistentvolumeclaims_restore_namespaced(corev1api, file_info, file_content):
    pvc = encoder_load(file_content, file_info.name)
    metadata = prepare_metadata(pvc.metadata)
    # Instantiate the persistentvolumeclaims object
    persistentvolumeclaims = kubernetes.client.V1PersistentVolumeClaim(
        api_version=pvc.api_version,
        kind="PersistentVolumeClaim",
        spec=pvc.spec,
        metadata=metadata
    )
    # clean some data
    persistentvolumeclaims.spec.volume_mode = None
    persistentvolumeclaims.spec.volume_name = None
    logging.debug('PVC: ' + str(persistentvolumeclaims))
    if file_info.objcache is not None:
        # object exist so we replace it
        response = corev1api.replace_namespaced_persistent_volume_claim(k8sfile2objname(file_info.name),
                                                                        file_info.namespace, persistentvolumeclaims,
                                                                        pretty='true')
    else:
        # object does not exist, so create one as required
        response = corev1api.create_namespaced_persistent_volume_claim(file_info.namespace, persistentvolumeclaims,
                                                                       pretty='true')
    return {'response': response}
