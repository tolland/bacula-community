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
import logging

SNAPSHOT_DRIVER_COMPATIBLE='csi'

K8SOBJ_SNAPSHOT_GROUP = 'snapshot.storage.k8s.io'
K8SOBJ_SNAPSHOT_VERSION = 'v1'
K8SOBJ_SNAPSHOTCLASS_PLURAL = 'volumesnapshotclasses'
K8SOBJ_SNAPSHOT_PLURAL = 'volumesnapshots'
K8SOBJ_SNAPSHOT_KIND = 'VolumeSnapshot'
K8SOBJ_SNAPSHOT_NAME_TEMPLATE = 'bacula-vsnap-{pvc}-{jobid}'
BACKUP_PVC_FROM_SNAPSHOT_TEMPLATE = 'bacula-pvcfs-{pvc}-{jobid}'

def csi_snapshots_read_namespaced(crd_api, namespace, name):
    return crd_api.get_namespaced_custom_object(K8SOBJ_SNAPSHOT_GROUP, K8SOBJ_SNAPSHOT_VERSION, namespace, K8SOBJ_SNAPSHOT_PLURAL, name)
    

def csi_snapshots_namespaced_names(crd_api, namespace, labels=""):
    snapdict = {}
    snaps = crd_api.list_namespaced_custom_object(K8SOBJ_SNAPSHOT_GROUP, K8SOBJ_SNAPSHOT_VERSION, namespace, K8SOBJ_SNAPSHOT_PLURAL, watch=False, label_selector=labels)
    for snap in snaps.items:
        snapdict[snap.metadata.name] = {
            'name': snap.metadata.name,
            'api_version': snap.apiVersion,
            'kind': snap.kind,
            'namespace': snap.metadata.namespace,
            'resourceVersion': snap.metadata.resourceVersion,
            'uid': snap.metadata.uid,
            'pvc_source': snap.spec.source.persistentVolumeClaimName,
            'class_name': snap.spec.volumeSnapshotClassName,
            'creation_time': snap.status.creationTime,
            'ready_to_use': snap.status.readyToUse,
            'restore_size': snap.status.restoreSize
        }
    return snapdict

def get_volume_snapshot_class_name(crd_api, pvc_storageclass_provisioner):
    volume_snapshot_classes = crd_api.list_cluster_custom_object(K8SOBJ_SNAPSHOT_GROUP, K8SOBJ_SNAPSHOT_VERSION, K8SOBJ_SNAPSHOTCLASS_PLURAL, watch=False)
    logging.debug("Provisioner of PVC Storage class:\n{}".format(pvc_storageclass_provisioner))
    logging.debug("VolumeSnapshotClasses available:\n {}".format(volume_snapshot_classes))
    for snap in volume_snapshot_classes.get('items'):
        if pvc_storageclass_provisioner == snap.get('driver'):
            logging.debug("VolumesnapshotClass selected:\n{}".format(snap.get('metadata').get('name')))
            return snap.get('metadata').get('name')
    raise ValueError("VolumeSnapshotClass not found for this provisioner: {}. Please contact with Bacula support".format(pvc_storageclass_provisioner))



def prepare_create_snapshot_body(crd_api, namespace, pvc_name, jobid, pvc_storageclass_provisioner):
    volumeSnapshotClassName = get_volume_snapshot_class_name(crd_api, pvc_storageclass_provisioner)
    return {
        "group": K8SOBJ_SNAPSHOT_GROUP,
        "version": K8SOBJ_SNAPSHOT_VERSION,
        "namespace": namespace,
        "plural": K8SOBJ_SNAPSHOT_PLURAL,
        "body": {
            "apiVersion": "{}/{}".format(K8SOBJ_SNAPSHOT_GROUP, K8SOBJ_SNAPSHOT_VERSION),
            "kind": K8SOBJ_SNAPSHOT_KIND,
            "metadata": {
                "name": K8SOBJ_SNAPSHOT_NAME_TEMPLATE.format(pvc=pvc_name, jobid=jobid),
                "namespace": namespace
            },
            "spec": {
                "volumeSnapshotClassName": volumeSnapshotClassName,
                "source": {
                    "persistentVolumeClaimName": pvc_name
                }
            }
        }
    }

def prepare_snapshot_action(namespace, vsnapshot_name):
    return {
        "group": K8SOBJ_SNAPSHOT_GROUP,
        "version": K8SOBJ_SNAPSHOT_VERSION,
        "namespace": namespace,
        "plural": K8SOBJ_SNAPSHOT_PLURAL,
        "name": vsnapshot_name 
    }

def prepare_pvc_from_vsnapshot_body(namespace, pvcdata, jobid):
    vsnapshot_name = K8SOBJ_SNAPSHOT_NAME_TEMPLATE.format(pvc=pvcdata.get('name'), jobid=jobid)
    return {
        'api_version': 'v1',
        'kind': 'PersistentVolumeClaim',
        'metadata': {
            'name': BACKUP_PVC_FROM_SNAPSHOT_TEMPLATE.format(pvc=pvcdata.get('name'), jobid=jobid),
            'namespace': namespace
        },
        'spec': {
            'storageClassName': pvcdata.get('storage_class_name'),
            'dataSource': {
                'name': vsnapshot_name,
                'kind': K8SOBJ_SNAPSHOT_KIND,
                'apiGroup': K8SOBJ_SNAPSHOT_GROUP
            },
            'accessModes': ['ReadWriteOnce'],
            'resources': { 'requests': {'storage': pvcdata.get('capacity')}}
        }
    }
    
