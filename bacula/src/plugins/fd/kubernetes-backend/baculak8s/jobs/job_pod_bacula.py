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
import time
from abc import ABCMeta

import yaml
from baculak8s.jobs.job import Job
from baculak8s.plugins.k8sbackend.baculabackup import (BACULABACKUPIMAGE,
                                                       ImagePullPolicy,
                                                       prepare_backup_pod_yaml,
                                                       get_backup_pod_name)
from baculak8s.plugins.k8sbackend.pvcclone import prepare_backup_clone_yaml, find_bacula_pvc_clones_from_old_job
from baculak8s.plugins.k8sbackend.baculaannotations import BaculaBackupMode
from baculak8s.util.respbody import parse_json_descr
from baculak8s.util.sslserver import DEFAULTTIMEOUT, ConnectionServer
from baculak8s.util.token import generate_token

DEFAULTRECVBUFFERSIZE = 64 * 1048
PLUGINHOST_NONE_ERR = "PLUGINHOST parameter is missing and cannot be autodetected. " \
                      "Cannot continue with pvcdata backup!"
POD_EXECUTION_ERR = "Cannot successfully start bacula-backup pod in expected time!"
POD_REMOVE_ERR = "Unable to remove proxy Pod {podname}! Other operations with proxy Pod will fail!"
POD_EXIST_ERR = "Job already running in '{namespace}' namespace. Check logs or delete {podname} Pod manually."
TAR_STDERR_UNKNOWN = "Unknown error. You should check Pod logs for possible explanation."
PLUGINPORT_VALUE_ERR = "Cannot use provided pluginport={port} option. Used default!"
FDPORT_VALUE_ERR = "Cannot use provided fdport={port} option. Used default!"
POD_YAML_PREPARED_INFO = "Prepare bacula-backup Pod with: {image} <{pullpolicy}> {pluginhost}:{pluginport}"
POD_YAML_PREPARED_INFO_NODE = "Prepare Bacula Pod on: {nodename} with: {image} <{pullpolicy}> {pluginhost}:{pluginport}"
CANNOT_CREATE_BACKUP_POD_ERR = "Cannot create backup pod. Err={}"
CANNOT_REMOVE_BACKUP_POD_ERR = "Cannot remove backup pod. Err={}"
PVCDATA_GET_ERROR = "Cannot get PVC Data object Err={}"
PVCCLONE_YAML_PREPARED_INFO = "Prepare clone: {namespace}/{snapname} storage: {storage} capacity: {capacity}"
CANNOT_CREATE_PVC_CLONE_ERR = "Cannot create PVC snapshot. Err={}"
CANNOT_CREATE_VSNAPSHOT_ERR = "Cannot create Volume Snapshot. Err={}"
CANNOT_CREATE_PVC_SNAPSHOT_ERR = "Cannot create PVC from Volume Snapshot. Err={}"
CANNOT_REMOVE_PVC_CLONE_ERR = "Cannot remove PVC snapshot. Err={}"
CANNOT_REMOVE_VSNAPSHOT_ERR = "Unable to remove volume snapshot {vsnapshot}! Please you must remove it manually."
CANNOT_START_CONNECTIONSERVER = "Cannot start ConnectionServer. Err={}"

WARNING_CLONED_PVC_WAS_NOT_WORKED = "As clone backup is empty. It will retry again to do a backup with standard mode."
VSNAPSHOT_BACKUP_COMPATIBLE_INFO = "The pvc `{}` is compatible with volume snapshot backup. Doing backup with this technology."
PVC_FROM_SNAPSHOT_CREATED = "The pvc `{}` was created from volume snapshot from pvc `{}`."
CREATING_PVC_FROM_VSNAPSHOT = "Creating pvc from volume snapshot of pvc `{}`."
FINISHED_PVC_FROM_VSNAPSHOT = "Finished pvc `{}` volume snapshot from pvc `{}`."
class JobPodBacula(Job, metaclass=ABCMeta):
    """

    This is a common class for all job which handles pvcdata backups using
    bacula-backup Pod.

    """

    def __init__(self, plugin, io, params):
        super().__init__(plugin, io, params)
        self.connsrv = None
        self.fdaddr = params.get("fdaddress")
        self.fdport = params.get("fdport", 9104)
        self.pluginhost = params.get("pluginhost", self.fdaddr)
        self.pluginport = params.get("pluginport", self.fdport)
        self.certfile = params.get('fdcertfile')
        _keyfile = params.get('fdkeyfile')
        self.keyfile = _keyfile if _keyfile is not None else self.certfile
        self._prepare_err = False
        self.token = None
        self.jobname = '{name}:{jobid}'.format(name=params.get('name', 'undefined'), jobid=params.get('jobid', '0'))
        self.timeout = params.get('timeout', DEFAULTTIMEOUT)
        try:
            self.timeout = int(self.timeout)
        except ValueError:
            self.timeout = DEFAULTTIMEOUT
        self.timeout = max(1, self.timeout)
        self.tarstderr = ''
        self.tarexitcode = None
        self.backupimage = params.get('baculaimage', BACULABACKUPIMAGE)
        self.imagepullpolicy = ImagePullPolicy.process_param(params.get('imagepullpolicy'))
        self.imagepullsecret = params.get('imagepullsecret', None)
        self.backup_clone_compatibility = True
        self.debug = params.get('debug', 0)
        self.current_backup_mode = BaculaBackupMode.Standard

    def handle_pod_logs(self, connstream):
        logmode = ''
        self.tarstderr = ''
        file_count = 0
        bytes_count = 0
        with connstream.makefile(mode='r') as fd:
            self.tarexitcode = fd.readline().strip()
            logging.debug('handle_pod_logs:tarexitcode:{}'.format(self.tarexitcode))
            while True:
                data = fd.readline()
                if not data:
                    break
                logging.debug('LOGS:{}'.format(data.strip()))
                if data.startswith('---- stderr ----'):
                    logmode = 'stderr'
                    continue
                elif data.startswith('---- list ----'):
                    logmode = 'list'
                    continue
                elif data.startswith('---- end ----'):
                    break
                if logmode == 'stderr':
                    self.tarstderr += data
                    continue
                elif logmode == 'list':
                    # no listing feature yet
                    file_count += 1
                    try:
                        file_props = data.split()
                        logging.debug('Data Split:{}'.format(','.join(file_props)))
                        bytes_count += int(file_props[2]) 
                    except Exception as e:
                        logging.exception(e)
                    continue
        logging.debug('Bytes/files in backup: {}/{}'.format(bytes_count, file_count))
        logging.debug('Type of job:' + str(self._params.get('type')))
        # If the backupMode is standard, we ignore if the backup contains 0 bytes.
        if self._params.get('type') == 'b' and self.current_backup_mode != BaculaBackupMode.Standard and bytes_count == 0 and file_count < 3:
            self._io.send_non_fatal_error(WARNING_CLONED_PVC_WAS_NOT_WORKED)
            self.backup_clone_compatibility = False


    def handle_pod_data_recv(self, connstream):
        while True:
            data = self.connsrv.streamrecv(DEFAULTRECVBUFFERSIZE)
            if not data:
                logging.debug('handle_pod_data_recv:EOT')
                break
            if self.debug == '3':
                logging.debug('handle_pod_data_recv:D' + str(len(data)))
            self._io.send_data(data)

    def handle_pod_data_send(self, connstream):
        while True:
            data = self._io.read_data()
            if not data:
                logging.debug('handle_pod_data_send:EOT')
                break
            self.connsrv.streamsend(data)
            if self.debug == '3':
                logging.debug('handle_pod_data_send:D{}'.format(len(data)))

    def prepare_pod_yaml(self, namespace, pvcdata, mode='backup'):
        logging.debug('pvcdata: {}'.format(pvcdata))
        if self.pluginhost is None:
            self._handle_error(PLUGINHOST_NONE_ERR)
            self._prepare_err = True
            return None
        pport = self.pluginport
        try:
            self.pluginport = int(self.pluginport)
        except ValueError:
            self.pluginport = 9104
            logging.warning(PLUGINPORT_VALUE_ERR.format(port=pport))
            self._io.send_warning(PLUGINPORT_VALUE_ERR.format(port=pport))
        pvcname = pvcdata.get('name')
        node_name = pvcdata.get('node_name')

        podyaml = prepare_backup_pod_yaml(mode=mode, nodename=node_name, host=self.pluginhost, port=self.pluginport,
                                          token=self.token, namespace=namespace, pvcname=pvcname, image=self.backupimage,
                                          imagepullpolicy=self.imagepullpolicy, imagepullsecret=self.imagepullsecret, job=self.jobname)
        if node_name is None:
            self._io.send_info(POD_YAML_PREPARED_INFO.format(
                image=self.backupimage,
                pullpolicy=self.imagepullpolicy,
                pluginhost=self.pluginhost,
                pluginport=self.pluginport
            ))
        else:
            self._io.send_info(POD_YAML_PREPARED_INFO_NODE.format(
                nodename=node_name,
                image=self.backupimage,
                pullpolicy=self.imagepullpolicy,
                pluginhost=self.pluginhost,
                pluginport=self.pluginport
            ))
        return podyaml

    def prepare_clone_yaml(self, namespace, pvcname, capacity, storage_class):
        logging.debug('prepare_clone_yaml: {} {} {} {}'.format(namespace, pvcname, capacity, storage_class))
        if namespace is None or pvcname is None or capacity is None or storage_class is None:
            logging.error("Invalid params to pvc clone!")
            return None, None
        pvcyaml, snapname = prepare_backup_clone_yaml(namespace, pvcname, capacity, storage_class, self.jobname)
        self._io.send_info(PVCCLONE_YAML_PREPARED_INFO.format(
            namespace=namespace,
            snapname=snapname,
            storage=storage_class,
            capacity=capacity
        ))
        return pvcyaml, snapname

    def prepare_connection_server(self):
        if self.connsrv is None:
            if self.fdaddr is None:
                self.fdaddr = '0.0.0.0'
            fport = self.fdport
            try:
                self.fdport = int(self.fdport)
            except ValueError:
                self.fdport = 9104
                logging.warning(FDPORT_VALUE_ERR.format(port=fport))
                self._handle_error(FDPORT_VALUE_ERR.format(port=fport))
            logging.debug("prepare_connection_server:New ConnectionServer: {}:{}".format(
                str(self.fdaddr),
                str(self.fdport)))
            self.connsrv = ConnectionServer(self.fdaddr, self.fdport,
                                            token=self.token,
                                            certfile=self.certfile,
                                            keyfile=self.keyfile,
                                            timeout=self.timeout)
            response = self.connsrv.listen()
            logging.debug("response:{}".format(response))
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_START_CONNECTIONSERVER.format(parse_json_descr(response)))
                return False
        else:
            logging.debug("prepare_connection_server:Reusing ConnectionServer!")
            self.connsrv.token = self.token
        return True

    def execute_pod(self, namespace, podyaml):
        prev_bacula_pod_name = self._plugin.check_bacula_pod(namespace, self.jobname)
        if prev_bacula_pod_name:
            logging.debug('Exist previous bacula-backup pod! Name: {}'.format(prev_bacula_pod_name))
            self._io.send_info('Exist a previous bacula-backup pod: {}'.format(prev_bacula_pod_name))
            response = self._plugin.remove_backup_pod(namespace, prev_bacula_pod_name)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_REMOVE_BACKUP_POD_ERR.format(parse_json_descr(response)))
                return False
            self._io.send_info('Removed previous bacula-backup pod: {}'.format(prev_bacula_pod_name))
        poddata = yaml.safe_load(podyaml)
        response = self._plugin.create_backup_pod(namespace, poddata)
        if isinstance(response, dict) and 'error' in response:
            self._handle_error(CANNOT_CREATE_BACKUP_POD_ERR.format(parse_json_descr(response)))
        else:
            for seq in range(self.timeout):
                time.sleep(1)
                isready = self._plugin.backup_pod_isready(namespace, podname=get_backup_pod_name(self.jobname), seq=seq)
                if isinstance(isready, dict) and 'error' in isready:
                    self._handle_error(CANNOT_CREATE_BACKUP_POD_ERR.format(parse_json_descr(isready)))
                    break
                elif isready:
                    return True
        return False

    def execute_pvcclone(self, namespace, clonename, cloneyaml):
        pass

    def delete_pod(self, namespace, force=False):
        for a in range(self.timeout):
            time.sleep(1)
            response = self._plugin.check_gone_backup_pod(namespace, get_backup_pod_name(self.jobname), force=force)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_REMOVE_BACKUP_POD_ERR.format(parse_json_descr(response)))
            else:
                logging.debug('delete_pod:isgone:{}'.format(response))
                if response:
                    return True
        return False

    def delete_pvcclone(self, namespace, clonename, force=False):
        for a in range(self.timeout):
            time.sleep(1)
            response = self._plugin.check_gone_pvcclone(namespace, clonename, force=force)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_REMOVE_PVC_CLONE_ERR.format(parse_json_descr(response)))
            else:
                logging.debug('delete_pvcclone:isgone:{}'.format(response))
                if response:
                    return True
        return False

    def handle_delete_pod(self, namespace):
        if not self.delete_pod(namespace=namespace):
            self._handle_error(POD_REMOVE_ERR.format(podname=get_backup_pod_name(self.jobname)))

    def handle_tarstderr(self):
        if self.tarexitcode != '0' or len(self.tarstderr) > 0:
            # format or prepare error message
            if not len(self.tarstderr):
                self.tarstderr = TAR_STDERR_UNKNOWN
            else:
                self.tarstderr = self.tarstderr.rstrip('\n')
            # classify it as error or warning
            if self.tarexitcode != '0':
                self._handle_non_fatal_error(self.tarstderr)
            else:
                self._io.send_warning(self.tarstderr)

    def prepare_bacula_pod(self, pvcdata, namespace=None, mode='backup'):
        if self._prepare_err:
            # first prepare yaml was unsuccessful, we can't recover from this error
            return False
        self.token = generate_token()
        if namespace is None:
            namespace = pvcdata.get('fi').namespace
        logging.debug('prepare_bacula_pod:token={} namespace={}'.format(self.token, namespace))
        podyaml = self.prepare_pod_yaml(namespace, pvcdata, mode=mode)
        if podyaml is None:
            # error preparing yaml
            self._prepare_err = True
            return False
        if not self.prepare_connection_server():
            self._prepare_err = True
            return False
        logging.debug('prepare_bacula_pod:start pod')
        if not self.execute_pod(namespace, podyaml):
            self._handle_error(POD_EXECUTION_ERR)
            return False
        return True

    def cleanup_old_cloned_pvc(self, namespace):
        logging.debug("Starting cleanup the old pvc from other previous job")
        # List all pvc names from namespace
        pvc_names = self._plugin.get_pvc_names(namespace)
        # Search old cloned pvcs
        old_clone_pvcs = find_bacula_pvc_clones_from_old_job(pvc_names, self.jobname)
        # Remove old pvcs
        for old_clone_pvc in old_clone_pvcs:
            self._io.send_info(f"Exist a previous bacula-backup pvc. Name: {old_clone_pvc}")
            response = self._plugin.delete_persistent_volume_claim(namespace, old_clone_pvc)
            logging.debug(f"Response from pvc `{old_clone_pvc}` delete request: {response}")
            if isinstance(response, dict) and "error" in response:
                return response
            self._io.send_info(f"Removed the previous bacula-backup pvc: {old_clone_pvc}")
            logging.debug("Deleted request launched successfully")
        logging.debug("Ended cleanup the old pvcs from other previous job")

    def create_pvcclone(self, namespace, pvcname):
        clonename = None
        logging.debug("pvcclone for:{}/{}".format(namespace, pvcname))
        self.cleanup_old_cloned_pvc(namespace)
        pvcdata = self._plugin.get_pvcdata_namespaced(namespace, pvcname)
        if isinstance(pvcdata, dict) and 'exception' in pvcdata:
            self._handle_error(PVCDATA_GET_ERROR.format(parse_json_descr(pvcdata)))
        else:
            logging.debug('PVCDATA_ORIG:{}:{}'.format(pvcname, pvcdata))
            cloneyaml, clonename = self.prepare_clone_yaml(namespace, pvcname, pvcdata.get('capacity'), pvcdata.get('storage_class_name'))
            if cloneyaml is None or clonename is None:
                # error preparing yaml
                self._prepare_err = True
                return None
            clonedata = yaml.safe_load(cloneyaml)
            response = self._plugin.create_pvc_clone(namespace, clonedata)
            if isinstance(response, dict) and 'error' in response:
                self._handle_error(CANNOT_CREATE_PVC_CLONE_ERR.format(parse_json_descr(response)))
                return None

        return clonename

    def handle_create_vsnapshot_backup(self, namespace, pvcname):
        """
        Manage operations to create snapshot and new pvc from this snapshot to do backup. 
        """
        pvc = self._plugin.get_pvcdata_namespaced(namespace, pvcname)
        
        # Check if pvc is compatible with vsnapshot
        if not self._plugin.check_pvc_compatiblity_with_vsnapshot(namespace, pvc.get('name')):
            logging.debug('The pvc is not compatible with vsnapshots. Name:{}'.format(pvc.get('name')))
            return None, pvc
        logging.debug('Origin pvcdata FileInfo:\n{}'.format(pvc.get('fi')))
        self._io.send_info(VSNAPSHOT_BACKUP_COMPATIBLE_INFO.format(pvc.get('name')))
        vsnapshot = self._plugin.create_vsnapshot(namespace, pvc)
        if isinstance(vsnapshot, dict) and 'error' in vsnapshot:
            self._handle_error(CANNOT_CREATE_VSNAPSHOT_ERR.format(parse_json_descr(vsnapshot)))
            return None, None
        # Create pvc from volume snapshot
        self._io.send_info(CREATING_PVC_FROM_VSNAPSHOT.format(pvc.get('name')))
        new_pvc = self._plugin.create_pvc_from_vsnapshot(namespace, pvc)
        if isinstance(new_pvc, dict) and 'error' in new_pvc:
            self._handle_error(CANNOT_CREATE_PVC_SNAPSHOT_ERR.format(parse_json_descr(new_pvc)))
            return None, None
        self._io.send_info(FINISHED_PVC_FROM_VSNAPSHOT.format(new_pvc.get('name'), pvc.get('name')))
        return vsnapshot, new_pvc

    def handle_delete_vsnapshot_backup(self, namespace, vsnapshot, pvcdata):
        logging.debug('handle_delete_vsnapshot: {}/{}/{}'.format(namespace, vsnapshot, pvcdata))
        if vsnapshot is None:
            return None
        pvc_name = pvcdata
        if type(pvcdata) == dict:
            pvc_name = pvcdata.get('name')
        response = self._plugin.remove_pvcclone(namespace, pvc_name)
        if isinstance(response, dict) and "error" in response:
            return self._handle_error(CANNOT_REMOVE_PVC_CLONE_ERR.format(pvc_name))
        response = self._plugin.remove_vsnapshot(namespace, vsnapshot.get('name'))
        if isinstance(response, dict) and "error" in response:
            return self._handle_error(CANNOT_REMOVE_VSNAPSHOT_ERR.format(vsnapshot=vsnapshot.get('name')))
        return True
