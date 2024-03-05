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
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

import logging
import kubernetes
from baculak8s.plugins.k8sbackend.pods import pods_namespaced_specs


"""
* bacula/backup.mode: [snapshot|standard] - it will be the cloud way to select what pvcdata you want to backup and extend the current plugin parameter: pvcdata[=<pvcname>], default is snapshot if not defined.
* bacula/backup.volumes: <pvcname[,pvcname2...]> - required, multiple pvc names as comma separated list.
* bacula/run.before.job.container.command: [<container>/<command>|*/<command>] - a star (*) means all containers.
* bacula/run.before.job.failjobonerror: [yes|no] - default is yes.
* bacula/run.after.job.container.command: [<container>/<command>|*/<command>] - a star (*) means all containers.
* bacula/run.after.job.failjobonerror: [yes|no] - default is no.
"""


class BaculaBackupMode(object):
    """
    This is a class to manage snapshot mode.
    """
    Snapshot = 'snapshot'
    Clone = 'clone'
    Standard = 'standard'
    params = (Snapshot, Clone, Standard)

    @staticmethod
    def process_param(mode):
        """The static method validates backup mode

        Args:
            mode (str): a backup mode parameter from k8s annotation

        Returns:
            str: backup mode normalized to consts, `None` when error
        """
        if mode is None:
            return None
        mode = mode.lower()
        for p in BaculaBackupMode.params:
            if p == mode:
                return p
        raise Exception('This backup mode `{}` is not supported. Only supported: snapshot, clone or standard.'.format(mode))


class BaculaAnnotationsClass(object):
    """
    This is a class to manage Bacula annotations parameters
    """
    BaculaPrefix = 'bacula/'
    BackupMode = 'backup.mode'
    BackupVolume = 'backup.volumes'
    RunBeforeJob = 'run.before.job.container.command'
    RunBeforeJobonError = 'run.before.job.failjobonerror'
    RunAfterJob = 'run.after.job.container.command'
    RunAfterJobonError = 'run.after.job.failjobonerror'
    RunAfterSnapshot = 'run.after.snapshot.container.command'
    RunAfterSnapshotonError = 'run.after.snapshot.failjobonerror'
    params = (BackupMode, BackupVolume, RunBeforeJob, RunBeforeJobonError, RunAfterJob, RunAfterJobonError, RunAfterSnapshot, RunAfterSnapshotonError)

    @staticmethod
    def process_param(param):
        """The static method validates Bacula annotations

        Args:
            param (str): a Bacula annotation from k8s

        Returns:
            str: Bacula annotation normalized to consts, `None` when error
        """
        if param is not None:
            for p in BaculaAnnotationsClass.params:
                if param == BaculaAnnotationsClass.BaculaPrefix + p:
                    return p
        return None

    @staticmethod
    def handle_run_job_container_command(param):
        """The static method handles container/command annotation parameters

        Args:
            param (str): a container command parameter from k8s annotation

        Returns:
            tuple(2): container / command split
        """
        container, command = (None, None)
        if param is not None:
            try:
                container, command = param.split('/', 1)
            except ValueError as e:
                logging.error(e)
        return container, command


def annotated_namespaced_pods_data(corev1api, namespace, estimate=False, labels=""):
    """Reads Pods annotations to search for Bacula annotations

    Args:
        corev1api (coreviapi): kubernetes corev1api instance
        namespace (str): namespace for the pod
        estimate (bool, optional): inform if we does backup or estimate job. Defaults to False.
        labels (list , optional): k8s label filter. Defaults to None.

    Returns:
        list: a list of pods and its annotations for selected namespace
    """
    podsdata = []
    pods = pods_namespaced_specs(corev1api, namespace, labels)
    for pod in pods:
        metadata = pod.metadata
        if metadata.annotations is None:
            continue
        bacula_annotations = [k for k, v in metadata.annotations.items() if k.startswith(BaculaAnnotationsClass.BaculaPrefix)]
        if len(bacula_annotations) > 0:
            containers = [c.name for c in pod.spec.containers]
            podobj = {
                'name': metadata.name,
                'containers': containers
            }
            for ba in bacula_annotations:
                param = metadata.annotations.get(ba)
                baname = BaculaAnnotationsClass.process_param(ba)   # we will ignore all annotations we cannot handle
                if baname is not None:
                    podobj[baname] = param
            podsdata.append(podobj)

    return podsdata
