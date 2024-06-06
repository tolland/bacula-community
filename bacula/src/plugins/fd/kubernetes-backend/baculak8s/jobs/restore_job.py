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
import re
import time

from baculak8s.entities.file_info import DIRECTORY, EMPTY_FILE, NOT_EMPTY_FILE
from baculak8s.entities.k8sobjtype import K8SObjType
from baculak8s.io.jobs.restore_io import (COMMA_SEPARATOR_NOT_SUPPORTED,
                                          FILE_ERROR_TEMPLATE,
                                          FILE_TRANSFER_START,
                                          RESTORE_LOOP_ERROR, RESTORE_START,
                                          SKIP_PACKET, SUCCESS_PACKET,
                                          FileContentReader, RestoreIO,
                                          RestorePacket)
from baculak8s.jobs.job_pod_bacula import JobPodBacula
from baculak8s.plugins.k8sbackend.k8sfileinfo import k8sfile2objname
from baculak8s.services.job_info_service import (REPLACE_ALWAYS,
                                                 REPLACE_NEVER)
from baculak8s.util.respbody import parse_json_descr
from baculak8s.util.sslserver import DEFAULTTIMEOUT

NS_DOESNOT_EXIST_ERR = "Namespace '{namespace}' does not exist! You should restore namespace too!"
PVC_DOESNOT_EXIST_ERR = "PVC: '{pvcname}' does not exist or is not mounted! " \
                        "You should restore PVC config or mount it in any Pod!"
PVC_ISNOTREADY_ERR = "PVC: '{pvcname}' is not ready in expected time! " \
                     "You should check PVC config or logs!"
SERVICE_ACCOUNT_TOKEN_RESTORE_INFO = "Unable to restore service-account-token {token}. Regenerated new one."
RESTORE_RES_ERR = "Cannot restore resource object. Err={}"
PVC_STATUS_ERR = "PVC: '{}' invalid status! Err={}"


class RestoreJob(JobPodBacula):
    """
        Job that contains the business logic
        related to the restore mode of the Backend.
        It depends upon a Plugin Class implementation
        that sends data to the Plugins Data Source.
    """

    def __init__(self, plugin, params):
        super().__init__(plugin, RestoreIO(), params)
        self.ns = {}
        self.localrestore = "where" in self._params and self._params["where"] != ""

    def execution_loop(self):
        return self.__restore_loop()

    def execute(self):
        self._start(RESTORE_START),
        self.execution_loop()
        self._io.send_eod()

    def __restore_loop(self):
        file_info = None
        while True:
            packet_type, packet = self._io.next_loop_packet(onError=self._abort)
            if packet_type == RestorePacket.RESTORE_END:
                break
            elif packet_type == RestorePacket.FILE_INFO:
                file_info = packet
                self.restore_object(file_info)
            else:
                self._io.send_abort(RESTORE_LOOP_ERROR)
                self._abort()

    def restore_object(self, file_info):
        dorestore = self.__handle_file_info(file_info)
        logging.debug('restore_object:dorestore:'+str(dorestore))
        if dorestore:
            # everything is ok, so do a restore
            reader = FileContentReader()
            self._io.send_command(SUCCESS_PACKET)
            self._read_start(FILE_TRANSFER_START, onError=self._abort)

            if self.localrestore or file_info.objtype != K8SObjType.K8SOBJ_PVCDATA:
                self.__restore_file(file_info, reader)
            else:
                namespace = file_info.namespace
                self.__restore_pvcdata(namespace)

        elif dorestore is not None:
            self._io.send_command(SKIP_PACKET)

    def __handle_file_info(self, file_info):
        logging.debug('handle_file_info:fileinfo: {}'.format(file_info))
        dorestore = True
        if file_info.type in [EMPTY_FILE, NOT_EMPTY_FILE]:
            if "replace" in self._params and not self.__should_replace(file_info):
                dorestore = False

            else:
                if self.localrestore:
                    file_info = self._plugin.apply_where_parameter(file_info, self._params["where"])
                    if "regexwhere" in self._params and self._params["regexwhere"] != '':
                        self.__apply_regexwhere_param(file_info)

                else:
                    if file_info.objtype == K8SObjType.K8SOBJ_PVCDATA:
                        dorestore = self.__prepare_pvcdata_restore(file_info)

        elif file_info.type == DIRECTORY:
            # We just ignore it for the current implementation
            dorestore = False

        logging.debug('handle_file_info:dorestore: {}'.format(dorestore))
        return dorestore

    def __prepare_pvcdata_restore(self, file_info):
        """
            Successful pvcdata restore require the following prerequisites:
            - namespace has to exist so at least restored with this job
            - pvcs has to exist so at least restored with this job
        :param file_info: pvcdata FileInfo for restore
        :return:
        """

        namespace = file_info.namespace
        nsdata = self.ns.get(namespace)
        if nsdata is None:
            # no information about namespace, get it
            logging.debug('prepare_pvcdata_restore:no previous ns check. do it!')
            nsexist = self._plugin.check_namespace(namespace)
            nsdata = {
                'exist': nsexist,
            }
            self.ns[namespace] = nsdata
            if nsexist is None:
                logging.debug('prepare_pvcdata_restore ns:{} not exist!'.format(namespace))
                self._handle_error(NS_DOESNOT_EXIST_ERR.format(namespace=namespace))
                return None
        else:
            if nsdata.get('exist', None) is None:
                logging.debug('prepare_pvcdata_restore ns:cached:{} not exist!'.format(namespace))
                return False
        logging.debug('prepare_pvcdata_restore ns: {} found.'.format(namespace))

        pvcdatalist = nsdata.get('pvcs')
        if pvcdatalist is None:
            # grab pvcs
            pvcdatalist = self._plugin.list_pvcdata_for_namespace(namespace, allpvcs=True)
            logging.debug('prepare_pvcdata_restore pvcdatalist:{}'.format(pvcdatalist))
            self.ns[namespace].update({
                'pvcs': pvcdatalist,
            })

        pvcname = k8sfile2objname(file_info.name)
        pvcdata = pvcdatalist.get(pvcname)
        if pvcdata is None:
            logging.debug('prepare_pvcdata_restore pvc:{} not exist!'.format(pvcname))
            self._handle_error(PVC_DOESNOT_EXIST_ERR.format(pvcname=pvcname))
            return None

        pvcisready = False
        for _ in range(DEFAULTTIMEOUT):
            time.sleep(1)
            isready = self._plugin.pvc_isready(namespace, pvcname)
            iswaiting = self._plugin.is_pvc_waiting_first_consumer(namespace,pvcname)
            if isinstance(isready, dict) and 'error' in isready:
                # cannot check pvc status
                self._handle_error(PVC_STATUS_ERR.format(pvcname, parse_json_descr(isready)))
            elif isready or iswaiting:
                pvcisready = True
                break
            # well, we have to wait for pvc to be ready, so restart procedure
            logging.debug('Waiting for pvc to become ready...')

        if not pvcisready:
            logging.debug('prepare_pvcdata_restore pvc:{} is not ready in expected time!'.format(pvcname))
            self._handle_error(PVC_ISNOTREADY_ERR.format(pvcname=pvcname))
            return None

        self.ns[namespace].update({
            'pvcdata': pvcdata,
        })
        logging.debug('prepare_pvcdata_restore PVCDATA:{}'.format(pvcdata))

        if self.prepare_bacula_pod(pvcdata, namespace=namespace, mode='restore'):
            return True

        return False

    def __should_replace(self, file_info):
        """
        Checks if restored object is already available as we have to distinguish between create and patch.
        :param file_info:
        :return:
        """
        if file_info.objtype == K8SObjType.K8SOBJ_PVCDATA:
            # always replace file_info
            return True
        curent_file = self._plugin.check_file(file_info)
        # cache get object for future use
        if isinstance(curent_file, dict) and 'error' in curent_file:
            file_info.objcache = None
            logging.error("check_file: {}".format(curent_file['error']))
        else:
            file_info.objcache = curent_file
        if self._params["replace"] == REPLACE_ALWAYS:
            return True
        if self._params["replace"] == REPLACE_NEVER:
            if curent_file is not None:
                return False
        # XXX: we cannot support ifnewer or ifolder because k8s does not provide modification time
        return True

    def __apply_regexwhere_param(self, file_info):
        if re.match(r",(.+?),(.+?),", self._params["regexwhere"]):
            self._io.send_abort(COMMA_SEPARATOR_NOT_SUPPORTED)
            self._abort()

        file_info.apply_regexwhere_param(self._params["regexwhere"])

    def __handle_pvcdata_connections(self):
        logging.debug('restore_pvcdata:data send')
        response = self.connsrv.handle_connection(self.handle_pod_data_send)
        if 'error' in response:
            self._handle_error(response['error'])
            return False
        logging.debug('backup_pvcdata:logs recv')
        response = self.connsrv.handle_connection(self.handle_pod_logs)
        if 'error' in response:
            self._handle_error(response['error'])
            return False
        return True

    def __restore_pvcdata(self, namespace):
        self.__handle_pvcdata_connections()
        self.handle_tarstderr()
        self.handle_delete_pod(namespace=namespace)
        self._io.send_command(SUCCESS_PACKET)

    def __restore_file(self, file_info, reader):
        if file_info.size == 0 and file_info.objtype != K8SObjType.K8SOBJ_PVCDATA:
            logging.debug('file_info.size == 0')
            response = self._plugin.restore_file(file_info)
        else:
            reader.finished = False
            response = self._plugin.restore_file(file_info, reader)
        if isinstance(response, dict) and 'error' in response:
            if file_info.objtype == K8SObjType.K8SOBJ_SECRET:
                self._io.send_warning(SERVICE_ACCOUNT_TOKEN_RESTORE_INFO.format(
                    token=file_info.name.split('/')[-1][:-5]))
            else:
                if 'exception' in response:
                    error_msg = RESTORE_RES_ERR.format(parse_json_descr(response))
                    self._handle_error(error_msg)
                else:
                    error_msg = FILE_ERROR_TEMPLATE.format(file_info.name, file_info.namespace, response['error'])
                    self._handle_error(error_msg)
        else:
            if file_info.size != 0 or file_info.objtype == K8SObjType.K8SOBJ_PVCDATA:
                self._io.send_command(SUCCESS_PACKET)
