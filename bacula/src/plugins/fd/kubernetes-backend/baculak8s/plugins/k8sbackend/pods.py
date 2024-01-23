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
from baculak8s.entities.file_info import NOT_EMPTY_FILE
from baculak8s.entities.k8sobjtype import K8SObjType
from baculak8s.plugins.k8sbackend.k8sfileinfo import (NOW_TIMESTAMP,
                                                      encoder_dump,
                                                      encoder_load,
                                                      k8sfile2objname,
                                                      k8sfileinfo)
from baculak8s.plugins.k8sbackend.k8sutils import prepare_metadata


def pods_read_namespaced(corev1api, namespace, name):
    return corev1api.read_namespaced_pod(name, namespace)


def pods_namespaced_specs(corev1api, namespace, labels=""):
    podslist = []
    pods = corev1api.list_namespaced_pod(namespace=namespace, watch=False, label_selector=labels)
    logging.debug('[CUSTOM] Get items of pods: {}'.format(pods.items))
    for pod in pods.items:
        podslist.append(pod)
    # logging.debug("pods_namespaced_specs:{}".format(podslist))
    return podslist


def pods_list_namespaced(corev1api, namespace, estimate=False, labels=""):
    podslist = {}
    pods = corev1api.list_namespaced_pod(namespace=namespace, watch=False, label_selector=labels)
    for pod in pods.items:
        poddata = pods_read_namespaced(corev1api, namespace, pod.metadata.name)
        spec = encoder_dump(poddata)
        podslist['pod-' + pod.metadata.name] = {
            'spec': spec if not estimate else None,
            'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_POD, nsname=namespace,
                              name=pod.metadata.name,
                              ftype=NOT_EMPTY_FILE,
                              size=len(spec),
                              creation_timestamp=poddata.metadata.creation_timestamp),
        }
    return podslist


def pods_restore_namespaced(corev1api, file_info, file_content):
    pod = encoder_load(file_content, file_info.name)
    metadata = prepare_metadata(pod.metadata)
    # Instantiate the pods object
    pods = kubernetes.client.V1Pod(
        api_version=pod.api_version,
        kind="Pod",
        spec=pod.spec,
        metadata=metadata
    )
    # clean some data
    pods.spec.node_name = None
    secvol = []
    volumes = []
    # this removes dynamic secret token from volumes
    for v in pods.spec.volumes:
        if v.secret is not None and v.name.startswith('default-token-'):
            # TODO: we should check if the secret exist and is not type=='kubernetes.io/*'
            logging.debug('detectedSecVolume:'+str(v))
            secvol.append(v)
        else:
            logging.debug('standardVolume:'+str(v.name))
            volumes.append(v)
    pods.spec.volumes = volumes
    volume_mounts = []
    # this removes above volumes from volume mounts
    for c in pods.spec.containers:
        for v in c.volume_mounts:
            found = False
            logging.debug('volMountCheck:'+str(v.name))
            for s in secvol:
                if s.name == v.name:
                    found = True
                    logging.debug('secVolMountFound')
                    break
            logging.debug('findResult:'+str(found))
            if not found:
                volume_mounts.append(v)
        c.volume_mounts = volume_mounts
        logging.debug('volumeMounts after cleanup:' + str(c.volume_mounts))
    if file_info.objcache is not None:
        # object exist so we replace it
        response = corev1api.replace_namespaced_pod(k8sfile2objname(file_info.name),
                                                    file_info.namespace, pods, pretty='true')
    else:
        # object does not exist, so create one as required
        response = corev1api.create_namespaced_pod(file_info.namespace, pods, pretty='true')
    return {'response': response}
