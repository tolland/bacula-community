# Bacula(R) - The Network Backup Solution
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

import logging
import time

from baculak8s.entities.file_info import DIRECTORY
from baculak8s.entities.plugin_object import PluginObject
from baculak8s.io.packet_definitions import FILE_DATA_START
from baculak8s.jobs.estimation_job import PVCDATA_GET_ERROR, EstimationJob
from baculak8s.jobs.job_pod_bacula import DEFAULTRECVBUFFERSIZE
from baculak8s.plugins.k8sbackend.baculaannotations import (
    BaculaAnnotationsClass, BaculaBackupMode, annotated_pvc_backup_mode)
from baculak8s.plugins.k8sbackend.baculabackup import BACULABACKUPPODNAME
from baculak8s.plugins.k8sbackend.podexec import ExecStatus, exec_commands
from baculak8s.util.respbody import parse_json_descr
from baculak8s.util.boolparam import BoolParam
from baculak8s.plugins.k8sbackend.k8sfileinfo import defaultk8spath

BACKUP_START_PACKET = "BackupStart"
BACKUP_PARAM_LABELS = "Resource Selector: {}"
FILE_BACKUP_ERROR = "Error while reading file contents from the chosen Data Source: {}"
POD_DATA_RECV_ERR = "Error in receiving data from bacula-backup Pod!"
BA_MODE_ERROR = "Invalid annotations for Pod: {namespace}/{podname}. Backup Mode '{mode}' not supported!"
BA_EXEC_STDOUT = "{}:{}"
BA_EXEC_STDERR = "{} Error:{}"
BA_EXEC_ERROR = "Pod Container execution: {}"
POD_BACKUP_SELECTED = "The selected backup mode to do pvc backup of the pvc `{}` is `{}`"
CHANGE_BACKUP_MODE_FOR_INCOMPATIBLITY_PVC = "The pvc `{}` is not compatible with snapshot backup, changing mode to clone. Only pvc with storage that they use CSI driver are compatible."
PVC_BACKUP_MODE_APPLIED_INFO = "The pvc `{}` will be backup with {} mode."
RETRY_BACKUP_WITH_STANDARD_MODE = "If the clone backup is empty. It will try again to do a backup using standard mode."

class BackupJob(EstimationJob):
    """
        Job that contains the business logic
        related to the backup mode of the Backend.
        It depends upon a Plugin Class implementation
        that retrieves backup data from the Plugins Data Source
    """

    def __init__(self, plugin, params):
        super().__init__(plugin, params, BACKUP_START_PACKET)
        _label = params.get('labels', None)
        self.fs_backup_mode = BaculaBackupMode.process_param(params.get("backup_mode", BaculaBackupMode.Snapshot)) # Fileset backup mode defined.
        if _label is not None:
            self._io.send_info(BACKUP_PARAM_LABELS.format(_label))
        self._io.send_info("The selected default backup mode to do pvc backup in all job is `{}`.".format(self.fs_backup_mode))

    def execution_loop(self):
        super().processing_loop(estimate=False)
        self.process_plugin_objects()

    def process_file(self, data):
        return self._backup_file(data)

    def process_plugin_objects(self):
        # logging.debug("SELF: {}".format(dir(self)))
        """
            name: plugin command line
            category: Container
            type: Kubernetes/Openshift POD
            source: (name on the network?)
            uuid: (I don't know if available)
            size: total size POD would be nice
            status: T/W/e
            count: Number of POD
        """
        logging.debug("PO_PODS: {}".format(self._plugin.pods_counter))
        if self._plugin.pods_counter > 0:
            po_pods = PluginObject("/{}/".format(defaultk8spath),
                                "Kubernetes PODs",
                                cat="Container",
                                potype="POD",
                                src=self._plugin.po_source_data,
                                uuid=self._plugin.po_source_data,
                                count=self._plugin.pods_counter)
            self._io.send_plugin_object(po_pods)
        logging.debug("PO_PVCS: {} {}".format(self._plugin.pvcs_counter, self._plugin.pvcs_totalsize))
        if self._plugin.pvcs_counter > 0:
            po_pvcs = PluginObject("/{}/".format(defaultk8spath),
                                "Kubernetes Persistent Volume Claims",
                                cat="Container",
                                potype="PVC",
                                src=self._plugin.po_source_data,
                                uuid=self._plugin.po_source_data,
                                count=self._plugin.pvcs_counter,
                                size=self._plugin.pvcs_totalsize)
            self._io.send_plugin_object(po_pvcs)

    def _backup_file(self, data):
        file_info = data.get('fi')
        super()._estimate_file(file_info)
        if file_info.type != DIRECTORY:
            self.__backup_data(file_info, data.get('spec'))
        self._io.send_eod()

    def __backup_data(self, info, spec_data):
        self._io.send_command(FILE_DATA_START)
        if spec_data is None:
            self._handle_error(FILE_BACKUP_ERROR.format(info.name))
        else:
            for file_chunk in [spec_data[i:i+DEFAULTRECVBUFFERSIZE] for i in range(0, len(spec_data), DEFAULTRECVBUFFERSIZE)]:
                self._io.send_data(str.encode(file_chunk))

    def __backup_pvcdata(self, namespace):
        logging.debug('backup_pvcdata:data recv')
        self._io.send_command(FILE_DATA_START)
        response = self.connsrv.handle_connection(self.handle_pod_data_recv)
        if 'error' in response:
            self._handle_error(response['error'])
            if 'should_remove_pod' in response:
                self.delete_pod(namespace=namespace, force=True)
            return False
        logging.debug('backup_pvcdata:logs recv')
        response = self.connsrv.handle_connection(self.handle_pod_logs)
        if 'error' in response:
            self._handle_error(response['error'])
            return False
        return True
    
    def process_pvcdata(self, namespace, pvcdata, backup_with_pod = False, retry_backup = False):
        status = None
        vsnapshot = None
        is_cloned = False
        cloned_pvc_name = None
        # For retry if clone backup is incompatible.
        orig_pvcdata = pvcdata
        # Detect if pvcdata is compatible with snapshots
        if not backup_with_pod and not retry_backup:
            logging.debug('Backup mode {} of pvc {} without pod:'.format(self.fs_backup_mode, pvcdata.get('name')))
            if self.fs_backup_mode == BaculaBackupMode.Snapshot:
                logging.debug('Snapshot is activated')
                vsnapshot, pvcdata = self.handle_create_vsnapshot_backup(namespace, pvcdata.get('name'))
                self._io.send_info(PVC_BACKUP_MODE_APPLIED_INFO.format(pvcdata.get('name'), BaculaBackupMode.Snapshot))
                self.current_backup_mode = BaculaBackupMode.Snapshot

            if (vsnapshot is None and self.fs_backup_mode != BaculaBackupMode.Standard) or self.fs_backup_mode == BaculaBackupMode.Clone:
                if self.fs_backup_mode != BaculaBackupMode.Clone:
                    self._io.send_info(CHANGE_BACKUP_MODE_FOR_INCOMPATIBLITY_PVC.format(pvcdata.get('name')))
                self._io.send_info(PVC_BACKUP_MODE_APPLIED_INFO.format(pvcdata.get('name'), BaculaBackupMode.Clone))
                cloned_pvc_name = self.create_pvcclone(namespace, pvcdata.get('name'))
                cloned_pvc = self._plugin.get_pvcdata_namespaced(namespace, cloned_pvc_name)
                logging.debug('Cloned pvc fi:{}'.format(cloned_pvc.get('fi')))
                cloned_pvc.get('fi').set_name(pvcdata.get('fi').name)
                pvcdata = cloned_pvc
                is_cloned = True
                self.current_backup_mode = BaculaBackupMode.Clone

            if self.fs_backup_mode == BaculaBackupMode.Standard:
                self._io.send_info(PVC_BACKUP_MODE_APPLIED_INFO.format(pvcdata.get('name'), BaculaBackupMode.Standard))
                self.current_backup_mode = BaculaBackupMode.Standard
        logging.debug('Process_pvcdata (Backup_job): {} --- {}'.format(vsnapshot, pvcdata))

        if self.prepare_bacula_pod(pvcdata, namespace=namespace, mode='backup'):
            super()._estimate_file(pvcdata)     # here to send info about pvcdata to plugin
            status = self.__backup_pvcdata(namespace=namespace)
            if status:
                self._io.send_eod()
                self.handle_tarstderr()
            self.handle_delete_pod(namespace=namespace)
        # Both prepare_bacula_pod fails or not, we must remove snapshot and pvc
        if not backup_with_pod:
            self.handle_delete_vsnapshot_backup(namespace, vsnapshot, pvcdata)
        if is_cloned:
            self.delete_pvcclone(namespace, cloned_pvc_name)
        # It retries when the backup is not with pod annotation (it is controlled in their function)
        if not backup_with_pod and not retry_backup and not self.backup_clone_compatibility:
            # It is important set to True before recall the process.
            self.backup_clone_compatibility = True # We only try once

            self._io.send_info(RETRY_BACKUP_WITH_STANDARD_MODE)

            status = self.process_pvcdata(namespace, orig_pvcdata, backup_with_pod, True)
        return status

    def handle_pod_container_exec_command(self, corev1api, namespace, pod, runjobparam, failonerror=False):
        podname = pod.get('name')
        containers = pod.get('containers')
        logging.debug("[{}] pod {} containers: {}".format(runjobparam, podname, containers))
        # now check if run before job
        container, command = BaculaAnnotationsClass.handle_run_job_container_command(pod.get(runjobparam))
        if container is not None:
            logging.info("container: {}".format(container))
            logging.info("command: {}".format(command))
            if container != '*':
                # check if container exist
                if container not in containers:
                    # error
                    logging.error("container {} not found".format(container))
                    return False
                containers = [container]
            # here execute command
            for cname in containers:
                logging.info("executing command: {} on {}".format(command, cname))
                outch, errch, infoch = exec_commands(corev1api, namespace, podname, cname, command)
                logging.info("stdout:\n{}".format(outch))
                if len(outch) > 0:
                    outch = outch.rstrip('\n')
                    self._io.send_info(BA_EXEC_STDOUT.format(runjobparam, outch))
                logging.info("stderr:\n{}".format(errch))
                if len(errch) > 0:
                    errch = errch.rstrip('\n')
                    self._io.send_warning(BA_EXEC_STDERR.format(runjobparam, errch))
                execstatus = ExecStatus.check_status(infoch)
                logging.info("Exec status: {}".format(execstatus))
                if not execstatus:
                    self._io.send_warning(BA_EXEC_ERROR.format(infoch.get('message')))
                    if failonerror:
                        self._handle_error("Failing job on request...")
                        return False

        return True

    def process_pod_pvcdata(self, namespace, pod, pvcnames):
        logging.debug("process_pod_pvcdata:{}/{} {}".format(namespace, pod, pvcnames))
        status = None
        corev1api = self._plugin.corev1api
        pod_backup_mode = BaculaBackupMode.process_param(pod.get(BaculaAnnotationsClass.BackupMode, BaculaBackupMode.Snapshot))
        if pod_backup_mode is None:
            self._handle_error(BA_MODE_ERROR.format(namespace=namespace,
                                                    podname=pod.get('name'),
                                                    mode=pod.get(BaculaAnnotationsClass.BackupMode)))
            return False

        self._io.send_info("The selected default backup mode to do pvc backup of the pod `{}` is `{}`".format(pod.get('name'), pod_backup_mode))

        failonerror = BoolParam.handleParam(pod.get(BaculaAnnotationsClass.RunBeforeJobonError), True)      # the default is to fail job on error
        # here we execute remote command before Pod backup
        if not self.handle_pod_container_exec_command(corev1api, namespace, pod, BaculaAnnotationsClass.RunBeforeJob, failonerror):
            logging.error("handle_pod_container_exec_command execution error!")
            return False

        requestedvolumes = [v.lstrip().rstrip() for v in pvcnames.split(',')]
        handledvolumes = []

        # iterate on requested volumes for shapshot
        logging.debug("iterate over requested vols for backup: {}".format(requestedvolumes))
        for pvc in requestedvolumes:
            pvcname = pvc
            original_pvc = self._plugin.get_pvcdata_namespaced(namespace, pvcname)
            vsnapshot = None
            logging.debug("handling vol before backup: {}".format(pvcname))
            pvc_raw = self._plugin.get_persistentvolumeclaim_read_namespaced(namespace, pvcname)
            pvc_backup_mode = annotated_pvc_backup_mode(pvc_raw, pod_backup_mode)
            self._io.send_info(POD_BACKUP_SELECTED.format(pvcname, pvc_backup_mode))

            # Check if pvc has status: 'Terminating'. Because in this state, the backup raise error.
            if self._plugin.pvc_is_terminating(namespace, original_pvc):
                logging.debug("Skip pvc. Cause Terminating status")
                self._io.send_warning("Skip pvc `{}` because it is in Terminating status.".format(pvcname))
                continue
            if self._plugin.pvc_is_pending(namespace, original_pvc):
                logging.debug("Skip pvc. Cause Pending status")
                self._io.send_warning("Skip pvc `{}` because it is in Pending status.".format(pvcname))
                continue

            if pvc_backup_mode == BaculaBackupMode.Snapshot:
                logging.debug('Snapshot mode chosen')
                vsnapshot, pvc_from_vsnap = self.handle_create_vsnapshot_backup(namespace, pvcname)
                logging.debug("The vsnapshot created from pvc {} is: {}".format(pvcname, vsnapshot))
                logging.debug("The pvc create from vsnapshot {} is: {}. FI: {}".format(vsnapshot, pvc_from_vsnap, pvc_from_vsnap.get('fi')))
                if vsnapshot is None:
                    logging.debug(CHANGE_BACKUP_MODE_FOR_INCOMPATIBLITY_PVC.format(pvcname))
                    # backupmode = BaculaBackupMode.Clone
                    self._io.send_info(CHANGE_BACKUP_MODE_FOR_INCOMPATIBLITY_PVC.format(pvcname))
                    pvc_backup_mode = BaculaBackupMode.Clone
                else:
                    pvc = pvc_from_vsnap
                    pvcname = pvc_from_vsnap.get("name")

            if pvc_backup_mode == BaculaBackupMode.Clone:
                pvcname = self.create_pvcclone(namespace, pvcname)
                cloned_pvc = self._plugin.get_pvcdata_namespaced(namespace, pvcname)
                if pvcname is None:
                    # error
                    logging.error("create_pvcclone failed!")
                    return False
                else:
                    logging.debug('Original_pvc: {}'.format(original_pvc.get('fi')))
                    logging.debug('Cloned_pvc->fi: {}'.format(cloned_pvc.get('fi')))
                    cloned_pvc.get('fi').set_name(original_pvc.get('fi').name)
                    pvc = cloned_pvc

            logging.debug("handling vol after snapshot/clone: {}".format(pvcname))
            handledvolumes.append({
                'pvcname': pvcname,
                'pvc': pvc,
                'vsnapshot': vsnapshot,
                'backupmode': pvc_backup_mode,
                'original_pvc': original_pvc
                })

        failonerror = BoolParam.handleParam(pod.get(BaculaAnnotationsClass.RunAfterSnapshotonError), False)     # the default is ignore errors
        # here we execute remote command after vol snapshot
        if not self.handle_pod_container_exec_command(corev1api, namespace, pod, BaculaAnnotationsClass.RunAfterSnapshot, failonerror):
            return False

        # iterate on requested volumes for backup
        logging.debug("iterate over requested vols for backup: {}".format(handledvolumes))
        for volumes in handledvolumes:
            logging.debug('Volume in handlevolumes:\n{}'.format(volumes))
            pvc = volumes['pvc']
            pvcname = volumes['pvcname']
            # get pvcdata for this volume
            """
            PVCDATA:plugintest-pvc-alone:{'name': 'plugintest-pvc-alone-baculaclone-lfxrra', 'node_name': None, 'storage_class_name': 'ocs-storagecluster-cephfs', 'capacity': '1Gi', 'fi': <baculak8s.entities.file_info.FileInfo object at 0x7fc3c08bc668>}
            """
            pvcdata = self._plugin.get_pvcdata_namespaced(namespace, pvcname, pvcname)
            if isinstance(pvcdata, dict) and 'error' in pvcdata:
                self._handle_error(PVCDATA_GET_ERROR.format(parse_json_descr(pvcdata)))

            else:
                # Modify the name in FileInfo because we need save the file like original name 
                # and not the new pvc (from vsnapshot) name.
                if volumes.get('backupmode') == BaculaBackupMode.Snapshot or volumes.get('backupmode') == BaculaBackupMode.Clone:
                    logging.debug('We change the name of FileInfo to adapt the original pvc name with the new pvc name')
                    pvcdata.get('fi').set_name(pvc.get('fi').name)

                if len(pvcdata) > 0:
                    status = self.process_pvcdata(namespace, pvcdata, True)
                    # Control the compatibility of snapshot or clone. If it is not compatible, retry again with standard backup mode.
                    logging.debug('Call again process_pvcdata from process_pod_pvcdata? {}'.format(self.backup_clone_compatibility))
                    if not self.backup_clone_compatibility:
                        status = self.process_pvcdata(namespace, volumes['original_pvc'], True)

        # iterate on requested volumes for delete snap
        logging.debug("iterate over requested vols for delete snap: {}".format(handledvolumes))
        for volumes in handledvolumes:
            pvcname = volumes['pvcname']
            logging.debug("Should remove this pvc: {}".format(pvcname))
            if volumes.get('backupmode') == BaculaBackupMode.Clone:
                # snapshot delete if snapshot requested
                status = self.delete_pvcclone(namespace, pvcname)
            if volumes.get('backupmode') == BaculaBackupMode.Snapshot and volumes['vsnapshot'] is not None:
                status = self.handle_delete_vsnapshot_backup(namespace, volumes['vsnapshot'], volumes['pvc'])
        logging.debug("Finish removing pvc clones and vsnapshots. Status {}".format(status))

        failonerror = BoolParam.handleParam(pod.get(BaculaAnnotationsClass.RunAfterJobonError), False)     # the default is ignore errors
        # here we execute remote command after Pod backup
        if not self.handle_pod_container_exec_command(corev1api, namespace, pod, BaculaAnnotationsClass.RunAfterJob, failonerror):
            return False

        return status
