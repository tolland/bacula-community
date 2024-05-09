# -*- coding: UTF-8 -*-
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


class K8SObjType(object):
    K8SOBJ_CONFIGMAP = 'cm'
    K8SOBJ_DAEMONSET = 'ds'
    K8SOBJ_DEPLOYMENT = 'dp'
    K8SOBJ_ENDPOINT = 'ep'
    K8SOBJ_INGRESS = 'ing'
    K8SOBJ_LIMITRANGE = 'lr'
    K8SOBJ_NAMESPACE = 'ns'
    K8SOBJ_POD = 'pod'
    K8SOBJ_PVOLCLAIM = 'pvc'
    K8SOBJ_PVOLUME = 'pv'
    K8SOBJ_PODTEMPLATE = 'podt'
    K8SOBJ_REPLICASET = 'rs'
    K8SOBJ_REPLICACONTR = 'rc'
    K8SOBJ_RESOURCEQUOTA = 'rq'
    K8SOBJ_SECRET = 'sec'
    K8SOBJ_SERVICE = 'svc'
    K8SOBJ_SERVICEACCOUNT = 'sva'
    K8SOBJ_STATEFULSET = 'ss'
    K8SOBJ_PVCDATA = 'pvcdata'
    K8SOBJ_STORAGECLASS = 'sc'

    K8SOBJ_NAMESPACE_Path = 'namespaces'
    K8SOBJ_PVOLUME_Path = 'persistentvolumes'
    K8SOBJ_PVCS_Path = 'persistentvolumeclaims'
    K8SOBJ_PVCDATA_Path = 'pvcdata'
    K8SOBJ_STORAGECLASS_Path = 'storageclass'

    pathdict = {
        K8SOBJ_CONFIGMAP: 'configmaps',
        K8SOBJ_DAEMONSET: 'daemonsets',
        K8SOBJ_DEPLOYMENT: 'deployments',
        K8SOBJ_ENDPOINT: 'endpoints',
        K8SOBJ_INGRESS: 'ingress',
        K8SOBJ_LIMITRANGE: 'limitranges',
        K8SOBJ_NAMESPACE: K8SOBJ_NAMESPACE_Path,
        K8SOBJ_POD: 'pods',
        K8SOBJ_PVOLCLAIM: K8SOBJ_PVCS_Path,
        K8SOBJ_PVCDATA: K8SOBJ_PVCS_Path,
        K8SOBJ_PVOLUME: K8SOBJ_PVOLUME_Path,
        K8SOBJ_PODTEMPLATE: 'podtemplates',
        K8SOBJ_REPLICASET: 'replicasets',
        K8SOBJ_REPLICACONTR: 'replicationcontroller',
        K8SOBJ_RESOURCEQUOTA: 'resourcequota',
        K8SOBJ_SECRET: 'secrets',
        K8SOBJ_SERVICE: 'services',
        K8SOBJ_SERVICEACCOUNT: 'serviceaccounts',
        K8SOBJ_STATEFULSET: 'statefulsets',
        K8SOBJ_STORAGECLASS: K8SOBJ_STORAGECLASS_Path,
    }

    methoddict = {
        K8SOBJ_CONFIGMAP: 'config_map',
        K8SOBJ_DAEMONSET: 'daemon_set',
        K8SOBJ_DEPLOYMENT: 'deployment',
        K8SOBJ_ENDPOINT: 'endpoint',
        K8SOBJ_INGRESS: 'ingress',
        K8SOBJ_LIMITRANGE: 'limitrange',
        K8SOBJ_NAMESPACE: 'namespace',
        K8SOBJ_POD: 'pod',
        K8SOBJ_PVOLCLAIM: 'persistentvolume_claim',
        K8SOBJ_PVCDATA: 'persistentvolume_data',
        K8SOBJ_PVOLUME: 'persistentvolume',
        K8SOBJ_PODTEMPLATE: 'pod_template',
        K8SOBJ_REPLICASET: 'replica_set',
        K8SOBJ_REPLICACONTR: 'replication_controller',
        K8SOBJ_RESOURCEQUOTA: 'resource_quota',
        K8SOBJ_SECRET: 'secret',
        K8SOBJ_SERVICE: 'service',
        K8SOBJ_SERVICEACCOUNT: 'service_account',
        K8SOBJ_STATEFULSET: 'stateful_set',
        K8SOBJ_STORAGECLASS: 'storageclass',
    }
