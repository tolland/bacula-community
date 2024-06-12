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

from baculak8s.entities.file_info import FileInfo
from baculak8s.io.default_io import DefaultIO
from baculak8s.io.packet_definitions import ESTIMATION_START_PACKET
from baculak8s.jobs.job_pod_bacula import (PVCDATA_GET_ERROR, JobPodBacula)
from baculak8s.plugins.k8sbackend.baculaannotations import BaculaAnnotationsClass
from baculak8s.util.respbody import parse_json_descr

PATTERN_NOT_FOUND = "No matches found for pattern {}"
NO_PV_FOUND = "No required Persistent Volumes found at the cluster"
NO_SC_FOUND = "No required Storage Classes found at the cluster"
NO_NS_FOUND = "No required Namespaces found at the cluster"
PV_LIST_ERROR = "Cannot list PV objects. Err={}"
PODS_LIST_ERROR = "Cannot list Pods objects. Err={}"
SC_LIST_ERROR = "Cannot list StorageClass objects. Err={}"
NS_LIST_ERROR = "Cannot list Namespace objects. Err={}"
RES_LIST_ERROR = "Cannot list resource objects. Err={}"
# PVCDATA_LIST_ERROR = "Cannot list PVC Data objects. Err={}"
PROCESSING_NAMESPACE_INFO = "Processing namespace: {namespace}"
PROCESSING_PVCDATA_START_INFO = "Start backup volume claim: {pvc}"
PROCESSING_PVCDATA_STOP_INFO = "Finish backup volume claim: {pvc}"
PROCESSING_PODBACKUP_PHASE_START_INFO = "Start backup phase of pvcs that they are in pod annotations"
PROCESSING_PODBACKUP_PHASE_FINISH_INFO = "Finish backup phase of pvcs that they are in pod annotations"
PROCESSING_PODBACKUP_START_INFO = "Start backup Pod: {namespace}/{podname}"
PROCESSING_PODBACKUP_FINISH_INFO = "Finish backup Pod: {namespace}/{podname}"
SKIP_PVCDATA_SECOND_BACKUP_INFO = "Skip backup of pvc `{}` because it did previously with a pod"
SECOND_TRY_PVCDATA_INFO = "Generic try to do backup of this pvc `{}`"
BA_PVC_NOT_FOUND_ERROR = "Requested volume claim: {pvc} on {namespace}/{podname} not found!"
BA_PVCNAME_ERROR = "Invalid annotations for Pod: {namespace}/{podname}. {bavol} Required!"
PROCESS_POD_PVCDATA_ERROR = "Cannot process Pod PVC Data backup: {namespace}/{podname}!"
LABEL_PARAM_INVALID_ERROR = "Label parameter ({}) is invalid!"


class EstimationJob(JobPodBacula):
    """
        Job that contains the business logic
        related to the estimation mode of the Backend.
        It depends upon a Plugin Class implementation
        that retrieves estimation data from the Plugins Data Source
    """

    def __init__(self, plugin, params, start_packet=None):
        if not start_packet:
            start_packet = ESTIMATION_START_PACKET
        self.__start_packet = start_packet
        super().__init__(plugin, DefaultIO(), params)
        self.include_matches = []
        self.regex_include_matches = []
        self.exclude_matches = []
        self.regex_exclude_matches = []
        _sc = params.get('storageclass', [])
        self.storageclassparam  = _sc if len(_sc) > 0 else None
        _pv = params.get('persistentvolume', [])
        self.persistentvolumeparam = _pv if len(_pv) > 0 else None

    def execute(self):
        self._start(self.__start_packet)
        self.execution_loop()
        self.__verify_all_matches()
        self._io.send_eod()

    def execution_loop(self):
        return self.processing_loop(estimate=True)

    def processing_loop(self, estimate=False):
        sc_list = self._plugin.list_all_storageclass(estimate=estimate)
        logging.debug("processing list_all_storageclass:{}:nrfound:{}".format(self.storageclassparam, len(sc_list)))
        if isinstance(sc_list, dict) and sc_list.get('exception'):
            self._handle_error(SC_LIST_ERROR.format(parse_json_descr(sc_list)))

        else:
            if self.storageclassparam is not None and len(sc_list) == 0:
                self._handle_error(NO_SC_FOUND)

            for sc in sc_list:
                logging.debug('processing sc:{}'.format(sc))
                self.process_file(sc_list.get(sc))

        pv_list = self._plugin.list_all_persistentvolumes(estimate=estimate)
        logging.debug("processing list_all_persistentvolumes:{}:nrfound:{}".format(self.persistentvolumeparam, len(pv_list)))
        if isinstance(pv_list, dict) and pv_list.get('exception'):
            self._handle_error(PV_LIST_ERROR.format(parse_json_descr(pv_list)))

        else:
            if self.persistentvolumeparam is not None and len(pv_list) == 0:
                self._handle_error(NO_PV_FOUND)

            for pv in pv_list:
                logging.debug("processing pv: {}".format(pv))
                self.process_file(pv_list.get(pv))

        ns_list = self._plugin.list_all_namespaces(estimate=estimate)
        logging.debug("processing list_all_namespaces:nrfound:{}".format(len(ns_list)))
        if isinstance(ns_list, dict) and ns_list.get('exception'):
            self._handle_error(NS_LIST_ERROR.format(parse_json_descr(ns_list)))

        else:
            if len(self._params.get('namespace')) != 0 and len(ns_list) == 0:
                self._handle_error(NO_NS_FOUND)

            for nsname in ns_list:
                ns = ns_list.get(nsname)
                logging.debug('processing ns: {}'.format(ns.get('name')))
                if not estimate:
                    self._io.send_info(PROCESSING_NAMESPACE_INFO.format(namespace=ns['name']))
                self.process_file(ns)
                nsdata = self._plugin.list_namespaced_objects(nsname, estimate=estimate)
                logging.debug('NSDATA:{}'.format([ns.keys() for ns in nsdata]))         # limit debug output
                for sub in nsdata:
                    # sub is a list of different resource types
                    if isinstance(sub, dict) and sub.get('exception'):
                        self._handle_error(RES_LIST_ERROR.format(parse_json_descr(sub)))
                    else:
                        for res in sub:
                            self.process_file(sub.get(res))

                podsannotated = self._plugin.get_annotated_namespaced_pods_data(nsname, estimate=estimate)
                logging.debug("processing get_annotated_namespaced_pods_data:{}:nrfound:{}".format(nsname,
                                                                                                   len(podsannotated)))
                # We can define what pvc backup in two places: fileset and pod annotation. So, we save the pod annotations to we don't backup two times the same.
                pvc_backed={}
                # here we have a list of pods which are anotated
                if podsannotated is not None and self._plugin.config.get('pvcdata') is not None:
                    if isinstance(podsannotated, dict) and podsannotated.get('exception'):
                        self._handle_error(PODS_LIST_ERROR.format(parse_json_descr(podsannotated)))
                    else:
                        if len(podsannotated) > 0 and not estimate:
                            self._io.send_info(PROCESSING_PODBACKUP_PHASE_START_INFO)
                        for pod in podsannotated:
                            logging.debug('PODDATA:{}'.format(pod))
                            # this is required parameter!
                            pvcnames = pod.get(BaculaAnnotationsClass.BackupVolume)
                            if pvcnames is None:
                                self._handle_error(BA_PVCNAME_ERROR.format(namespace=nsname,
                                                                           podname=pod.get('name'),
                                                                           bavol=BaculaAnnotationsClass.BaculaPrefix +
                                                                           BaculaAnnotationsClass.BackupVolume))
                                continue
                            else:
                                podname = pod.get('name')
                                if not estimate:
                                    self._io.send_info(PROCESSING_PODBACKUP_START_INFO.format(namespace=nsname,
                                                                                              podname=podname))
                                status = self.process_pod_pvcdata(nsname, pod, pvcnames)
                                logging.debug("Status in processing_loop: {}".format(status))
                                if status is None:
                                    for pvc_name in pvcnames.split(','):
                                        pvc_backed[pvc_name] = 'error' 
                                    logging.error("Some unknown error at {namespace}/{podname}!".format(namespace=nsname, podname=podname))
                                    self._handle_error(PROCESS_POD_PVCDATA_ERROR.format(namespace=nsname, podname=podname))
                                    break
                                if not estimate:
                                    for pvc_name in pvcnames.split(','):
                                        pvc_backed[pvc_name] = 'ok'
                                    self._io.send_info(PROCESSING_PODBACKUP_FINISH_INFO.format(namespace=nsname,
                                                                                               podname=podname))
                        if len(podsannotated) > 0 and not estimate:
                            self._io.send_info(PROCESSING_PODBACKUP_PHASE_FINISH_INFO)
                            logging.debug("PVCs backed through pod annotations: {}".format(pvc_backed))
                
                pvcdatalist = self._plugin.list_pvcdata_for_namespace(nsname, estimate=estimate)
                logging.debug("processing list_pvcdata_for_namespace:{}:nrfound:{}".format(nsname, len(pvcdatalist)))
                if pvcdatalist is not None:
                    if isinstance(pvcdatalist, dict) and pvcdatalist.get('exception'):
                        self._handle_error(PV_LIST_ERROR.format(parse_json_descr(pvcdatalist)))
                    else:
                        for pvc in pvcdatalist:
                            pvcdata = pvcdatalist.get(pvc)
                            logging.debug('PVCDATA:{}:{}'.format(pvc, pvcdata))
                            if not estimate:
                                logging.debug("Previous pvc_backed: {} /{}".format(pvc_backed, pvc))
                                # If a pod has annotations to backup a pvc, it did it previously. So, skip if it was ok, if not second try.
                                if pvc_backed.get(pvc) == 'ok':
                                    self._io.send_info(SKIP_PVCDATA_SECOND_BACKUP_INFO.format(pvc))
                                    continue
                                elif pvc_backed.get(pvc) == 'error':
                                    if self._plugin.pvc_is_terminating(nsname, pvcdata):
                                        logging.debug("Skip pvc. Cause Terminating status")
                                        self._io.send_warning("Skip pvc of second try `{}` because it is in Terminating status.".format(pvc))
                                        continue
                                    if self._plugin.pvc_is_pending(nsname, pvcdata):
                                        logging.debug("Skip pvc. Cause Pending status")
                                        self._io.send_warning("Skip pvc `{}` because it is in Pending status.".format(pvc))
                                        continue
                                    self._io.send_warning(SECOND_TRY_PVCDATA_INFO.format(pvc))
                                self._io.send_info(PROCESSING_PVCDATA_START_INFO.format(pvc=pvc))
                                if self._plugin.pvc_is_terminating(nsname, pvcdata):
                                    logging.debug("Skip pvc. Cause Terminating status")
                                    self._io.send_warning("Skip pvc `{}` because it is in Terminating status.".format(pvc))
                                    continue
                                if self._plugin.pvc_is_pending(nsname, pvcdata):
                                    logging.debug("Skip pvc. Cause Pending status")
                                    self._io.send_warning("Skip pvc `{}` because it is in Pending status.".format(pvc))
                                    continue
                            status = self.process_pvcdata(nsname, pvcdata)
                            if status is None:
                                # None means unable to prepare listening service during backup
                                break
                            if not estimate and status:
                                self._io.send_info(PROCESSING_PVCDATA_STOP_INFO.format(pvc=pvc))
                        logging.debug("Finish pvcdatalist")

    def _estimate_file(self, data):
        logging.debug('{}'.format(data))
        if isinstance(data, dict):
            file_info = data.get('fi')
        elif isinstance(data, FileInfo):
            file_info = data
        else:
            raise ValueError('Invalid data in estimate_file')
        logging.debug('file_info: {}'.format(file_info))
        self._io.send_file_info(file_info)

    def process_file(self, data):
        return self._estimate_file(data)

    def process_pvcdata(self, namespace, pvcdata):
        return self._estimate_file(pvcdata)

    def process_pod_pvcdata(self, namespace, pod, pvcnames):
        # iterate on requested pvc
        logging.debug("process_pod_pvcdata in Estimate mode")
        for pvc in pvcnames.split(','):
            if self._plugin.pvc_is_terminating(namespace, pvc):
                logging.debug("Skip pvc. Cause Terminating status")
                self._io.send_warning("Skip pvc `{}` because it is in Terminating status.".format(pvc))
                continue
            if self._plugin.pvc_is_pending(namespace, pvc):
                logging.debug("Skip pvc. Cause Pending status")
                self._io.send_warning("Skip pvc `{}` because it is in Pending status.".format(pvc))
                continue
            # get pvcdata for this volume
            pvcdata = self._plugin.get_pvcdata_namespaced(namespace, pvc)
            if isinstance(pvcdata, dict) and 'exception' in pvcdata:
                self._handle_error(PVCDATA_GET_ERROR.format(parse_json_descr(pvcdata)))
            else:
                logging.debug('PVCDATA:{}:{}'.format(pvc, pvcdata))
                if len(pvcdata) > 0:
                    self._estimate_file(pvcdata)
        return True

    def __match_includes(self, info):
        if len(self._params.get("includes", [])) <= 0:
            return True

        any_match = info.match_any_glob(
            self._params["includes"],
            self.include_matches
        )

        return any_match

    def __match_regex_includes(self, info):
        if len(self._params.get("regex_includes", [])) <= 0:
            return True

        any_match = info.match_any_regex(
            self._params["regex_includes"],
            self.regex_include_matches
        )

        return any_match

    def __match_excludes(self, info):
        if len(self._params.get("excludes", [])) <= 0:
            return False

        any_match = info.match_any_glob(
            self._params["excludes"],
            self.exclude_matches
        )

        return any_match

    def __match_regex_excludes(self, info):
        if len(self._params.get("regex_excludes", [])) <= 0:
            return False

        any_match = info.match_any_regex(
            self._params["regex_excludes"],
            self.regex_exclude_matches
        )

        return any_match

    def __verify_all_matches(self):
        self.__verify_matches("includes", self.include_matches)
        self.__verify_matches("regex_includes", self.regex_include_matches)
        self.__verify_matches("excludes", self.exclude_matches)
        self.__verify_matches("regex_excludes", self.regex_exclude_matches)

    def __verify_matches(self, pattern_type, matches):

        if len(self._params.get(pattern_type, [])) <= 0:
            return

        patterns = self._params[pattern_type]

        # Assures that all patterns got at least one match
        for pattern in patterns:
            if pattern not in matches:
                error_msg = PATTERN_NOT_FOUND.format(pattern)
                self._handle_error(error_msg)
