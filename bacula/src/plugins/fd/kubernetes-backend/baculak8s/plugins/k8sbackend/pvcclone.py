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
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

import os
import json
import logging

from baculak8s.util.token import generate_token

DEFAULTCLONEYAML = os.getenv('DEFAULTCLONEYAML', "/opt/bacula/scripts/bacula-backup-clone.yaml")
PREFIX_CLONE_PVC_BACKUP_NAME = "bacula-pvcclone-{job_name}-"
CLONE_PVC_BACKUP_NAME=PREFIX_CLONE_PVC_BACKUP_NAME + "{job_id}"
JOB_NAME_MAX_CHARS=25
JOB_ID_MAX_DIGITS=12
CLONETEMPLATE = """
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: {clonename}
  namespace: {namespace}
  labels:
    app: baculabackup
spec:
  storageClassName: {storageclassname}
  dataSource:
    name: {pvcname}
    kind: PersistentVolumeClaim
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: {pvcsize}
"""

def find_bacula_pvc_clones_from_old_job(pvc_list, job):
    name_for_search = PREFIX_CLONE_PVC_BACKUP_NAME.format(job_name=job.split('.')[0][:JOB_NAME_MAX_CHARS].lower())
    num_hyphen=name_for_search.count('-')
    old_pvcs = []
    for pvc_name in pvc_list:
        if pvc_name.startswith(name_for_search) and num_hyphen == pvc_name.count('-'):
            old_pvcs.append(pvc_name)
    logging.debug(f"Found old cloned pvcs: {old_pvcs}")
    return old_pvcs

def get_clone_pvc_name(job):
    # Get job name and id, and limit to not exceed 63 characters in pvc name
    job_name = job.split('.')[0][:JOB_NAME_MAX_CHARS].lower()
    job_id = job.split(':')[1][:JOB_ID_MAX_DIGITS]
    return CLONE_PVC_BACKUP_NAME.format(job_name=job_name, job_id=job_id)

def prepare_backup_clone_yaml(namespace, pvcname, pvcsize, scname, jobname, clonename=None):
    """ Handles PVC clone yaml preparation based on available templates

    Args:
        namespace (str): k8s namespace for pvc clone
        pvcname (str): source pvc name to clone from
        pvcsize (str): k8s capacity of the original pvc
        scname (str): storage class of the original pvc
        job (str): Job name to add to cloned pvc name
        clonename (str, optional): the cloned - destination - pvcname; if `None` then name will be assigned automatically. Defaults to None.

    Returns:
        tuple(2): return a prepared pvc clone yaml string and assigned pvc clone name, especially useful when this name was created automatically.
    """
    cloneyaml = CLONETEMPLATE
    if os.path.exists(DEFAULTCLONEYAML):
        with open(DEFAULTCLONEYAML, 'r') as file:
            cloneyaml = file.read()
    if clonename is None:
        clonename = get_clone_pvc_name(jobname)
    return cloneyaml.format(namespace=namespace, pvcname=pvcname, pvcsize=pvcsize, clonename=clonename, storageclassname=scname), clonename
    
def delete_pvcclone(corev1api, namespace, pvc_name, grace_period_seconds=0, propagation_policy='Foreground'):
    return corev1api.delete_namespaced_persistent_volume_claim(
        pvc_name, namespace, grace_period_seconds=grace_period_seconds,
        propagation_policy=propagation_policy)
