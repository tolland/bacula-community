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
#     Copyright (c) 2020 by Inteos sp. z o.o.
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

import logging

import kubernetes
from baculak8s.entities.file_info import (DEFAULT_FILE_MODE, NOT_EMPTY_FILE,
                                          FileInfo)
from baculak8s.entities.k8sobjtype import K8SObjType
from baculak8s.plugins.k8sbackend.k8sfileinfo import NOW_TIMESTAMP, k8sfileinfo
from baculak8s.plugins.k8sbackend.persistentvolumeclaims import (
    persistentvolumeclaims_namespaced_names,
    persistentvolumeclaims_read_namespaced)
from baculak8s.plugins.k8sbackend.pods import pods_namespaced_specs
from baculak8s.util.size_util import k8s_size_to_int


def pvcdata_list_update_node_names(corev1api, namespace, pvcdatalist):
    """ Updates node_name values for pvcdatalist.

    Args:
        corev1api (corev1api): kubernetes corev1api instance
        namespace (str): kubernetes namespace
        pvcdatalist (dict): pvc data list as dictionary

    Returns:
        dict: updated pvc data list as dictionary
    """
    # here we collect node_names for proper backup pod deployment
    pods = pods_namespaced_specs(corev1api, namespace=namespace)
    logging.debug('[CUSTOM] Get pods:{}'.format(pods))
    for pod in pods:
        logging.debug('[CUSTOM] Pod: {}'.format(pod))
        logging.debug('[CUSTOM] Pod Specs: {}'.format(pod.spec))
        if pod.spec is not None:
            logging.debug('[CUSTOM] POD SPEC is None')
        for vol in pod.spec.volumes:
            if vol.persistent_volume_claim is not None:
                pvcname = vol.persistent_volume_claim.claim_name
                for pvcf in pvcdatalist:
                    if pvcname == pvcdatalist[pvcf].get('name') and pvcdatalist[pvcf].get('node_name') is None:
                        pvcdatalist[pvcf]['node_name'] = pod.spec.node_name
    return pvcdatalist


def pvcdata_get_namespaced(corev1api, namespace, pvcname, pvcalias=None):
    """ Return a single pvcdata dict for requested pvcname.

    Args:
        corev1api (corev1api): kubernetes corev1api instance
        namespace (str): kubernetes namespace
        pvcname (str): requested pvc name
        pvcalias (str, optional): when not None then File_Info object wil use it as file name. Defaults to None.

    Returns:
        dict: pvc data dict
    """
    pvc = persistentvolumeclaims_read_namespaced(corev1api, namespace, pvcname)
    logging.debug('[CUSTOM] Read PVC: ')
    logging.debug(pvc)

    pvcspec = pvc.spec
    storageclassname = pvcspec.storage_class_name
    logging.debug('[CUSTOM] Get size:' + str(pvcspec.resources))
    pvcsize = pvcspec.resources.requests.get('storage', '-1')
    pvcdata = {
        'name': pvcname,
        'node_name': None,
        'storage_class_name': storageclassname,
        'capacity': pvcsize,
        'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_PVCDATA,
                          nsname=namespace,
                          name=pvcname if pvcalias is None else pvcalias,
                          ftype=NOT_EMPTY_FILE,
                          size=pvcsize),
    }
    pvcdatalist = pvcdata_list_update_node_names(corev1api, namespace, {pvcname: pvcdata})
    logging.debug('[CUSTOM] Read persistent volume claim completed')

    return pvcdatalist.get(pvcname)


def pvcdata_list_namespaced(corev1api, namespace, estimate=False, pvcfilter=True, labels=""):
    """ Return a list of pvcdata dicts for selected namespace.

    Args:
        corev1api (corev1api): kubernetes corev1api instance
        namespace (str): kubernetes namespace
        estimate (bool, optional): select if we do estimate (True) or backup job (False). Defaults to False.
        pvcfilter (bool, optional): when not None and not empty list then select pvc which name is in this list only. Defaults to True.
        labels (str, optional): selector labels

    Returns:
        dict: pvc data list as dictionary
    """
    pvcdata = {}
    if pvcfilter:
        logging.debug("pvcfilter: {}".format(pvcfilter))
        pvcnamelist = persistentvolumeclaims_namespaced_names(corev1api, namespace, labels)
        for pvcn in pvcnamelist:
            pvcname = pvcn[1]
            logging.debug('found:{}'.format(pvcname))
            if isinstance(pvcfilter, list) and pvcname not in pvcfilter:
                continue
            pvcsize = pvcn[3]
            pvcdata[pvcname] = {
                'name': pvcname,
                'node_name': None,
                'storage_class_name': pvcn[2],
                'capacity': pvcsize,
                'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_PVCDATA,
                                  nsname=namespace,
                                  name=pvcname,
                                  ftype=NOT_EMPTY_FILE,
                                  size=pvcsize),
            }
            logging.debug("add pvc: {}".format(pvcdata[pvcname]))
    # here we collect node_names for proper backup pod deployment
    pods = pods_namespaced_specs(corev1api, namespace=namespace)
    for pod in pods:
        if pod.spec.volumes is not None:
            for vol in pod.spec.volumes:
                if vol.persistent_volume_claim is not None:
                    pvcname = vol.persistent_volume_claim.claim_name
                    for pvcf in pvcdata:
                        if pvcname == pvcdata[pvcf].get('name') and pvcdata[pvcf].get('node_name') is None:
                            pvcdata[pvcf]['node_name'] = pod.spec.node_name
    return pvcdata


def list_pvcs_namespaced(corev1api, namespace):
    """ Return pvclist for selected namespace with FileInfo object only.
       This function is useful in listing mode only.

    Args:
        corev1api (corev1api): kubernetes corev1api instance
        namespace (str): kubernetes namespace

    Returns:
        dict: pvc data list as dictionary
    """
    pvcslist = {}
    pvcnamelist = persistentvolumeclaims_namespaced_names(corev1api, namespace)
    for pvcn in pvcnamelist:
        pvcname = pvcn[1]
        pvcsize = pvcn[3]
        logging.debug('found:{} : {}'.format(pvcname, pvcsize))
        name = "/{}/{}/{}/{}".format(K8SObjType.K8SOBJ_NAMESPACE_Path, namespace,
                                        K8SObjType.K8SOBJ_PVCDATA_Path, pvcname)
        pvcslist[pvcname] = {
            'fi': FileInfo(name=name,
                            ftype=NOT_EMPTY_FILE,
                            size=k8s_size_to_int(pvcsize),
                            uid=0, gid=0,
                            mode=DEFAULT_FILE_MODE,
                            nlink=1,
                            modified_at=NOW_TIMESTAMP,
                            accessed_at=NOW_TIMESTAMP,
                            created_at=NOW_TIMESTAMP),
        }
    return pvcslist
