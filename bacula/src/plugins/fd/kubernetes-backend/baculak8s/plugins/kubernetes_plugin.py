# -*- coding: UTF-8 -*-
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

import requests
import requests.packages.urllib3
import urllib3
from kubernetes import client, config
from kubernetes.client.rest import ApiException
from urllib3.exceptions import MaxRetryError, SSLError, TimeoutError
from baculak8s.entities.file_info import (DEFAULT_DIR_MODE, DIRECTORY,
                                          EMPTY_FILE, FileInfo)
from baculak8s.entities.k8sobjtype import K8SObjType
from baculak8s.io.log import Log
from baculak8s.plugins import k8sbackend
from baculak8s.plugins.k8sbackend.baculabackup import BACULABACKUPPODNAME
from baculak8s.plugins.k8sbackend.baculaannotations import annotated_namespaced_pods_data
from baculak8s.plugins.k8sbackend.configmaps import *
from baculak8s.plugins.k8sbackend.csi_snapshot import *
from baculak8s.plugins.k8sbackend.daemonset import *
from baculak8s.plugins.k8sbackend.deployment import *
from baculak8s.plugins.k8sbackend.endpoints import *
from baculak8s.plugins.k8sbackend.k8sfileinfo import NOW_TIMESTAMP
from baculak8s.plugins.k8sbackend.limitrange import *
from baculak8s.plugins.k8sbackend.namespaces import *
from baculak8s.plugins.k8sbackend.persistentvolumeclaims import *
from baculak8s.plugins.k8sbackend.persistentvolumes import *
from baculak8s.plugins.k8sbackend.pods import *
from baculak8s.plugins.k8sbackend.podtemplates import *
from baculak8s.plugins.k8sbackend.pvcdata import *
from baculak8s.plugins.k8sbackend.replicaset import *
from baculak8s.plugins.k8sbackend.replicationcontroller import *
from baculak8s.plugins.k8sbackend.resourcequota import *
from baculak8s.plugins.k8sbackend.secret import *
from baculak8s.plugins.k8sbackend.service import *
from baculak8s.plugins.k8sbackend.serviceaccounts import *
from baculak8s.plugins.k8sbackend.statefulset import *
from baculak8s.plugins.k8sbackend.storageclass import *
from baculak8s.plugins.k8sbackend.volumesnapshotclass import get_snapshot_drivers_compatible
from baculak8s.plugins.plugin import *
from baculak8s.util.date_util import gmt_to_unix_timestamp
from baculak8s.util.lambda_util import wait_until_resource_is_ready

HTTP_NOT_FOUND = 404
K8S_POD_CONTAINER_STATUS_ERROR = "Pod status error! Reason: {}, Message: {}"
K8S_VOLUME_SNAPSHOT_STATUS_ERROR = "Volume status error! Reason: {}, Message: {}"
class KubernetesPlugin(Plugin):
    """
        Plugin that communicates with Kubernetes API
    """

    def __init__(self, params):
        logging.debug("params:" + str(params))
        self._params = params
        _ns = params.get("namespace", [])
        _pv = params.get("persistentvolume", [])
        _sc = params.get("storageclass", [])
        _vssl = params.get("verify_ssl", None)
        _pvconfig = params.get("pvconfig", True)
        _scconfig = params.get("scconfig", True)
        _pvcdata = params.get("pvcdata", None)
        if isinstance(_pvcdata, str):
            # single param
            if _pvcdata == '1':
                # pvcdata without value
                _pvcdata = True
            else:
                _pvcdata = _pvcdata.split(',')

        self.config = {
            'config_file': params.get("config", None),
            'host': params.get("host", None),
            'token': params.get("token", None),
            'username': params.get("username", None),   # for future usage
            'password': params.get("password", None),   # for future usage
            'incluster': params.get("incluster", None),
            'verify_ssl': True if _vssl is None or _vssl == "1" or _vssl == '' else False,
            'ssl_ca_cert': params.get("ssl_ca_cert", None),
            'namespace': _ns if len(_ns) > 0 else None,
            'nsconfig': True,
            'pv': _pv if len(_pv) > 0 else None,
            'pvconfig': False if _pvconfig == '0' else True,
            'storageclass': _sc if len(_sc) > 0 else None,
            'scconfig': False if _scconfig == '0' else True,
            'pvcdata': _pvcdata,
            'labels': params.get("labels", ""),
        }

        # disable namespace backup when pv param selected
        if self.config.get('pv') is not None and self.config.get('namespace') is None:
            self.config['nsconfig'] = False

        # as pvcs are namespaced objects then it is required to add namespace=... parameter
        if self.config.get('namespace') is None and self.config.get('pvcdata') is not None:
            self.config['pvcdata'] = None

        logging.debug("pluginconfig:{}".format(self.config))

        self.coreapi = None
        self.corev1api = None
        self.appsv1api = None
        self.storagev1api = None
        self.clientConfiguration = None
        self.clientAPI = None
        self.k8s = {
            K8SObjType.K8SOBJ_PVOLUME: {},
            K8SObjType.K8SOBJ_NAMESPACE: {},
            K8SObjType.K8SOBJ_STORAGECLASS: {},
            K8SObjType.K8SOBJ_PVCDATA: {},
        }
        self._connected = False
        self.pods_counter = 0
        self.pvcs_counter = 0
        self.pvcs_totalsize = 0
        self.po_source_data = "<notset>"

    def connect(self):
        """
            Implementation of Plugin.connect(self)
        """
        if self.config.get("incluster") is not None:
            try:
                config.load_incluster_config()
            except config.config_exception.ConfigException as e:
                strerror = ' '.join(e.args)
                logging.debug("ERROR at load_incluster_config: " + strerror)
                return {
                    'error': "incluster error: " + strerror,
                    'exception': True,
                }
            except Exception as e:
                logging.debug("ERROR at load_incluster_config: " + str(e))
                return {
                    'error': str(e),
                    'exception': True,
                }

        elif self.config.get("token") is not None and self.config.get('host') is not None:
            # handle connection with bearertoken and apihost params
            self.clientConfiguration = client.Configuration()
            self.clientConfiguration.host = self.config.get('host')
            self.clientConfiguration.verify_ssl = self.config.get('verify_ssl')
            ssl_ca_cert = self.config.get("ssl_ca_cert")
            if ssl_ca_cert is not None:
                self.clientConfiguration.ssl_ca_cert = ssl_ca_cert
            self.clientConfiguration.api_key = {"authorization": "Bearer " + self.config.get("token")}
            self.clientAPI = client.ApiClient(configuration=self.clientConfiguration)
        else:
            configfile = self.config.get("config_file", None)
            if configfile is None:
                configfile = config.kube_config.KUBE_CONFIG_DEFAULT_LOCATION
            logging.debug("load_kube_config(config_file={})".format(configfile))
            try:
                config.load_kube_config(config_file=configfile)
            except OSError as e:
                logging.debug("ERROR OSError at load_kube_config: " + str(e.strerror))
                return {
                    'error': e.strerror + " config=" + str(configfile),
                    'exception': True,
                }
            except Exception as e:
                logging.debug("ERROR Exception at load_kube_config: " + str(e))
                return {
                    'error': str(e) + " config=" + str(configfile),
                    'exception': True,
                }

        # Tests the connection with K8S
        self.coreapi = client.CoreApi(api_client=self.clientAPI)
        self.corev1api = client.CoreV1Api(api_client=self.clientAPI)
        self.appsv1api = client.AppsV1Api(api_client=self.clientAPI)
        self.storagev1api = client.StorageV1Api(api_client=self.clientAPI)
        self.crd_api = client.CustomObjectsApi(api_client=self.clientAPI) # To manage csi snapshots

        logging.getLogger(requests.packages.urllib3.__package__).setLevel(logging.ERROR)
        logging.getLogger(client.rest.__package__).setLevel(logging.ERROR)
        urllib3.disable_warnings()
        logging.captureWarnings(True)
        response = self.__execute(lambda: self.coreapi.get_api_versions(), check_connection=False)
        if isinstance(response, dict) and "error" in response:
            logging.debug("ERROR response:{}".format(response))
            self.coreapi = None
            self.corev1api = None
            self.appsv1api = None
            self.storagev1api = None
            return response

        else:
            self._connected = True
            # grab some info about a cluster and forward to job
            vapi = client.VersionApi()
            response = self.__execute(lambda: vapi.get_code(), check_connection=False)
            if isinstance(response, dict) and "error" in response:
                data = {}
            else:
                data = {'response': response}
                response = self.__execute(lambda: self.coreapi.get_api_versions(), check_connection=False)
                if not (isinstance(response, dict) and "error" in response):
                    self.po_source_data = response.server_address_by_client_cid_rs[0].server_address
            return data

    def disconnect(self):
        """
            Implementation of Plugin.disconnect(self)
            No need to disconnect from K8S
        """
        pass

    def list_in_path(self, path):
        """
            Implementation of Plugin.list_in_path(self, path)
            returns FileInfo objects associated with the $path$
        """
        top_level_dirs = [
            K8SObjType.K8SOBJ_NAMESPACE_Path,
            K8SObjType.K8SOBJ_PVOLUME_Path,
            K8SObjType.K8SOBJ_STORAGECLASS_Path,
        ]
        path = path.lstrip("/")
        if path.startswith(K8SObjType.K8SOBJ_NAMESPACE_Path):
            lpath = path.split("/")
            if len(lpath) > 1 and len(lpath[1]) > 0:
                if len(lpath) == 2 or len(lpath[2]) == 0:
                    data = {
                        'pvcdata': {
                            'fi': FileInfo(name='/namespaces/' + lpath[1] + '/' + K8SObjType.K8SOBJ_PVCDATA_Path,
                                           ftype=DIRECTORY,
                                           size=0,
                                           uid=0, gid=0,
                                           nlink=1,
                                           mode=DEFAULT_DIR_MODE,
                                           modified_at=NOW_TIMESTAMP,
                                           accessed_at=NOW_TIMESTAMP,
                                           created_at=NOW_TIMESTAMP),
                            }
                    }
                    return data
                elif lpath[2] == K8SObjType.K8SOBJ_PVCDATA_Path:
                    return list_pvcs_namespaced(self.corev1api, lpath[1])
            else:
                return self.list_all_namespaces_names()

        if path == K8SObjType.K8SOBJ_STORAGECLASS_Path:
            return self.list_all_storageclass_names()

        if path == K8SObjType.K8SOBJ_PVOLUME_Path:
            return self.list_all_persistentvolumes_names()

        self.k8s = {}
        for dirname in top_level_dirs:
            self.k8s[dirname] = {
                'fi': FileInfo(name='/' + dirname,
                               ftype=DIRECTORY,
                               size=0,
                               uid=0, gid=0,
                               nlink=1,
                               mode=DEFAULT_DIR_MODE,
                               modified_at=NOW_TIMESTAMP,
                               accessed_at=NOW_TIMESTAMP,
                               created_at=NOW_TIMESTAMP),
            }
        return self.k8s

    def query_parameter(self, parameter):
        """
            Implementation of Plugin.query_parameter(self, parameter)
            returns array of available parameters
        """
        if parameter == 'namespace':
            return self.__execute(lambda: namespace_names(self.corev1api))
        if parameter == 'persistentvolume':
            return self.__execute(lambda: persistentvolumes_names(self.corev1api))
        if parameter == 'storageclass':
            return self.__execute(lambda: storageclass_names(self.storagev1api))
        ns = self.config.get("namespace")
        if ns is not None:
            ns = ns[0]
            if parameter == 'pvcdata':
                return self.__execute(lambda: persistentvolumeclaims_namespaced_names(self.corev1api, ns))
        return []

    def list_all_persistentvolumes(self, estimate=False):
        if self.config.get('pvconfig'):
            self.k8s[K8SObjType.K8SOBJ_PVOLUME] = \
                self.__execute(lambda: persistentvolumes_list_all(self.corev1api,
                                                                  pvfilter=self.config['pv'],
                                                                  estimate=estimate))
        return self.k8s[K8SObjType.K8SOBJ_PVOLUME]

    def list_all_namespaces(self, estimate=False):
        if self.config.get('nsconfig'):
            self.k8s[K8SObjType.K8SOBJ_NAMESPACE] = \
                self.__execute(lambda: namespaces_list_all(self.corev1api,
                                                           nsfilter=self.config['namespace'],
                                                           estimate=estimate))
        return self.k8s[K8SObjType.K8SOBJ_NAMESPACE]

    def list_all_storageclass(self, estimate=False):
        if self.config.get('scconfig'):
            self.k8s[K8SObjType.K8SOBJ_STORAGECLASS] = \
                self.__execute(lambda: storageclass_list_all(self.storagev1api,
                                                             scfilter=self.config['storageclass'],
                                                             estimate=estimate))
        return self.k8s[K8SObjType.K8SOBJ_STORAGECLASS]

    def get_pvcdata_namespaced(self, namespace, pvcname, pvcalias=None, estimate=False):
        logging.debug("pvcdata namespaced: {}/{} pvcalias={}".format(namespace, pvcname, pvcalias))
        return self.__execute(lambda: pvcdata_get_namespaced(self.corev1api, namespace, pvcname, pvcalias))

    def list_pvcdata_for_namespace(self, namespace, estimate=False, allpvcs=False):
        pvcfilter = self.config.get('pvcdata', allpvcs) if not allpvcs else allpvcs
        logging.debug("list pvcdata for namespace:{} pvcfilter={} estimate={}".format(namespace, pvcfilter, estimate))
        return self.__execute(lambda: pvcdata_list_namespaced(self.corev1api, namespace, estimate, pvcfilter=pvcfilter))

    def get_config_maps(self, namespace, estimate=False):
        return self.__execute(lambda: config_maps_list_namespaced(self.corev1api, namespace, estimate,
                                                                  self.config['labels']))

    def get_endpoints(self, namespace, estimate=False):
        return self.__execute(lambda: endpoints_list_namespaced(self.corev1api, namespace, estimate,
                                                                self.config['labels']))

    def get_pods(self, namespace, estimate=False):
        # the __execute() wrapper intercept exceptions and return and error dict()
        # instead of the expected response
        response = self.__execute(lambda: pods_list_namespaced(self.corev1api, namespace, estimate, self.config['labels']))
        if not isinstance(response, dict) or not "error" in response:
            pods = response
            nrpods = len(pods)
            logging.debug("get_pods[{}]:pods:{}".format(namespace, nrpods))
            self.pods_counter += nrpods
            return pods
        else:
            return response # it is a dictionary with an error

    def get_pvcs(self, namespace, estimate=False):
        # the __execute() wrapper intercept exceptions and return and error dict()
        # instead of the expected response
        response = self.__execute(lambda: persistentvolumeclaims_list_namespaced(self.corev1api, namespace, estimate,
                                                                             self.config['labels']))
        if isinstance(response, tuple) and len(response) == 2:
            pvcs, totalsize = response
            nrpvcs = len(pvcs)
            self.pvcs_counter += nrpvcs
            self.pvcs_totalsize += totalsize
            self.k8s['pvcs'] = pvcs
            logging.debug("get_pvcs[{}]:pvcs:{}".format(namespace, nrpvcs))
            return pvcs
        else:
            return response # it should be a dictionary with an error

    def get_podtemplates(self, namespace, estimate=False):
        return self.__execute(lambda: podtemplates_list_namespaced(self.corev1api, namespace, estimate,
                                                                   self.config['labels']))

    def get_limit_ranges(self, namespace, estimate=False):
        return self.__execute(lambda: limit_range_list_namespaced(self.corev1api, namespace, estimate,
                                                                  self.config['labels']))

    def get_replication_controller(self, namespace, estimate=False):
        return self.__execute(lambda: replication_controller_list_namespaced(self.corev1api, namespace, estimate,
                                                                             self.config['labels']))

    def get_resource_quota(self, namespace, estimate=False):
        return self.__execute(lambda: resource_quota_list_namespaced(self.corev1api, namespace, estimate,
                                                                     self.config['labels']))

    def get_secrets(self, namespace, estimate=False):
        return self.__execute(lambda: secrets_list_namespaced(self.corev1api, namespace, estimate,
                                                              self.config['labels']))

    def get_services(self, namespace, estimate=False):
        return self.__execute(lambda: services_list_namespaced(self.corev1api, namespace, estimate,
                                                               self.config['labels']))

    def get_service_accounts(self, namespace, estimate=False):
        return self.__execute(lambda: service_accounts_list_namespaced(self.corev1api, namespace, estimate,
                                                                       self.config['labels']))

    def get_daemon_sets(self, namespace, estimate=False):
        return self.__execute(lambda: daemon_sets_list_namespaced(self.appsv1api, namespace, estimate,
                                                                  self.config['labels']))

    def get_deployments(self, namespace, estimate=False):
        return self.__execute(lambda: deployments_list_namespaced(self.appsv1api, namespace, estimate,
                                                                  self.config['labels']))

    def get_replica_sets(self, namespace, estimate=False):
        return self.__execute(lambda: replica_sets_list_namespaced(self.appsv1api, namespace, estimate,
                                                                   self.config['labels']))

    def get_stateful_sets(self, namespace, estimate=False):
        return self.__execute(lambda: stateful_sets_list_namespaced(self.appsv1api, namespace, estimate,
                                                                    self.config['labels']))

    def list_all_persistentvolumes_names(self):
        self.k8s[K8SObjType.K8SOBJ_PVOLUME] = self.__execute(lambda: persistentvolumes_list_all_names(self.corev1api))
        return self.k8s[K8SObjType.K8SOBJ_PVOLUME]

    def get_annotated_namespaced_pods_data(self, namespace, estimate=False):
        return self.__execute(lambda: annotated_namespaced_pods_data(self.corev1api, namespace, estimate,
                                                                     self.config['labels']))

    def list_all_storageclass_names(self):
        self.k8s[K8SObjType.K8SOBJ_STORAGECLASS] = self.__execute(lambda: storageclass_list_all_names(self.storagev1api))
        return self.k8s[K8SObjType.K8SOBJ_STORAGECLASS]

    def list_all_namespaces_names(self):
        self.k8s[K8SObjType.K8SOBJ_NAMESPACE] = self.__execute(lambda: namespaces_list_all_names(self.corev1api))
        return self.k8s[K8SObjType.K8SOBJ_NAMESPACE]

    def list_namespaced_objects(self, namespace, estimate=False):
        # We should maintain the following resources backup order
        logging.debug("list_namespaced_objects_label:[{}]".format(self.config['labels']))
        self.k8s[K8SObjType.K8SOBJ_PVCDATA] = {}
        data = [
            self.get_config_maps(namespace, estimate),
            self.get_service_accounts(namespace, estimate),
            self.get_secrets(namespace, estimate),
            # self.get_endpoints(namespace, estimate),
            self.get_pvcs(namespace, estimate),
            self.get_limit_ranges(namespace, estimate),
            self.get_resource_quota(namespace, estimate),
            self.get_services(namespace, estimate),
            self.get_pods(namespace, estimate),
            self.get_daemon_sets(namespace, estimate),
            self.get_replica_sets(namespace, estimate),
            self.get_stateful_sets(namespace, estimate),
            self.get_deployments(namespace, estimate),
            self.get_replication_controller(namespace, estimate),
        ]
        return data

    def upload_config_map(self, file_info, file_content):
        return self.__execute(lambda: config_map_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_daemon_set(self, file_info, file_content):
        return self.__execute(lambda: daemon_sets_restore_namespaced(self.appsv1api, file_info, file_content))

    def upload_deployment(self, file_info, file_content):
        return self.__execute(lambda: deployments_restore_namespaced(self.appsv1api, file_info, file_content))

    def upload_endpoint(self, file_info, file_content):
        return self.__execute(lambda: endpoints_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_limitrange(self, file_info, file_content):
        return self.__execute(lambda: limit_range_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_namespace(self, file_info, file_content):
        return self.__execute(lambda: namespaces_restore(self.corev1api, file_info, file_content))

    def upload_pod(self, file_info, file_content):
        return self.__execute(lambda: pods_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_persistentvolume_claim(self, file_info, file_content):
        return self.__execute(lambda: persistentvolumeclaims_restore_namespaced(self.corev1api, file_info,
                                                                                file_content))

    def upload_persistentvolume(self, file_info, file_content):
        return self.__execute(lambda: persistentvolumes_restore(self.corev1api, file_info, file_content))

    def upload_storageclass(self, file_info, file_content):
        return self.__execute(lambda: storageclass_restore(self.storagev1api, file_info, file_content))

    def upload_pod_template(self, file_info, file_content):
        return self.__execute(lambda: podtemplates_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_replica_set(self, file_info, file_content):
        return self.__execute(lambda: replica_sets_restore_namespaced(self.appsv1api, file_info, file_content))

    def upload_replication_controller(self, file_info, file_content):
        return self.__execute(lambda: replication_controller_restore_namespaced(self.corev1api, file_info,
                                                                                file_content))

    def upload_resource_quota(self, file_info, file_content):
        return self.__execute(lambda: resource_quota_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_secret(self, file_info, file_content):
        return self.__execute(lambda: secrets_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_service(self, file_info, file_content):
        return self.__execute(lambda: services_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_service_account(self, file_info, file_content):
        return self.__execute(lambda: service_accounts_restore_namespaced(self.corev1api, file_info, file_content))

    def upload_stateful_set(self, file_info, file_content):
        return self.__execute(lambda: stateful_sets_restore_namespaced(self.appsv1api, file_info, file_content))

    def __restore_k8s_object(self, file_info, file_content_source=None):
        file_content = b''
        if file_info.size != 0 and file_content_source is not None:
            while True:
                data = file_content_source.read()
                if data is None:
                    break
                else:
                    file_content += data
        method_name = 'upload_' + str(K8SObjType.methoddict.get(file_info.objtype))
        method = getattr(self, method_name, None)
        if method is None:
            return {'error': 'Invalid object type: {}'.format(file_info.objtype)}
        if file_info.objcache is None:
            current_file = self.check_file(file_info)
            if isinstance(current_file, dict) and 'error' in current_file:
                file_info.objcache = None
                logging.error("check_file: {}".format(current_file['error']))
            else:
                file_info.objcache = current_file
        return method(file_info, file_content)

    def restore_file(self, file_info, file_content_source=None):
        """
            Implementation of Plugin.restore_file(self, file_info, file_content_source=None)
        """
        if file_info.objtype != K8SObjType.K8SOBJ_PVCDATA:
            return self.__restore_k8s_object(file_info, file_content_source)

    # TODO: export/move all checks into k8sbackend
    def check_storage_compatibility_with_vsnapshot(self, storage_class_name):
        logging.debug("Check Storage class compatibility.{}".format(storage_class_name))
        storage_provisioner = get_provisioner(self.storagev1api, storage_class_name)
        logging.debug("Provisioner {}".format(storage_provisioner))
        logging.debug('Compatible Drivers: {} '.format(get_snapshot_drivers_compatible(self.crd_api)))
        if storage_provisioner in get_snapshot_drivers_compatible(self.crd_api):
            return True
        return False

    def check_pvc_compatiblity_with_vsnapshot(self, namespace, pvc_name):
        pvc = self.get_pvcdata_namespaced(namespace, pvc_name)
        logging.debug('Check Compatibility with Snapshots. Name: {}'.format(pvc_name))
        logging.debug('PVC: {}'.format(pvc))
        return self.check_storage_compatibility_with_vsnapshot(pvc.get('storage_class_name'))

    def _check_config_map(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_config_map(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_daemon_set(self, file_info):
        return self.__exec_check_object(
            lambda: self.appsv1api.read_namespaced_daemon_set(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_deployment(self, file_info):
        return self.__exec_check_object(
            lambda: self.appsv1api.read_namespaced_deployment(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_endpoint(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_endpoints(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_limitrange(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_limit_range(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_namespace(self, file_info):
        return self.__exec_check_object(lambda: self.corev1api.read_namespace(k8sfile2objname(file_info.name)))

    def check_namespace(self, name):
        return self.__exec_check_object(lambda: self.corev1api.read_namespace(name))

    def _check_pod(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_pod(k8sfile2objname(file_info.name), file_info.namespace))

    def check_pod(self, namespace, name):
        return self.__exec_check_object(lambda: self.corev1api.read_namespaced_pod(name, namespace))

    def _check_persistentvolume_claim(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_persistent_volume_claim(k8sfile2objname(file_info.name),
                                                                           file_info.namespace))

    def _check_persistentvolume(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_persistent_volume(k8sfile2objname(file_info.name)))

    def _check_storageclass(self, file_info):
        return self.__exec_check_object(
            lambda: self.storagev1api.read_storage_class(k8sfile2objname(file_info.name)))

    def _check_pod_template(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_pod_template(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_replica_set(self, file_info):
        return self.__exec_check_object(
            lambda: self.appsv1api.read_namespaced_replica_set(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_stateful_set(self, file_info):
        return self.__exec_check_object(
            lambda: self.appsv1api.read_namespaced_stateful_set(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_replication_controller(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_replication_controller(k8sfile2objname(file_info.name),
                                                                          file_info.namespace))

    def _check_resource_quota(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_resource_quota(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_secret(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_secret(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_service(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_service(k8sfile2objname(file_info.name), file_info.namespace))

    def _check_service_account(self, file_info):
        return self.__exec_check_object(
            lambda: self.corev1api.read_namespaced_service_account(k8sfile2objname(file_info.name),
                                                                   file_info.namespace))

    def check_file(self, file_info):
        """

        :param file_info:
        :return:
        """
        method_name = '_check_' + str(K8SObjType.methoddict.get(file_info.objtype))
        method = getattr(self, method_name, None)
        if method is None:
            return {'error': 'Invalid object type: {}'.format(file_info.objtype)}
        return method(file_info)

    def __exec_check_object(self, action, check_connection=True):
        """
        Executes an action and verifies if it return HTTP404
        :param action:
        :param check_connection:
        :return:
        """

        if check_connection:
            # Verifies if connect() was called
            if not self._connected:
                return None

        try:
            return action()
        except ApiException as e:
            if e.status == HTTP_NOT_FOUND:
                return None
            return False

    def __execute(self, action, check_connection=True):
        """
            Executes an action that uses one of the python-swiftclient APIs
            (Connection API or Service API)

            :param action: Action to be executed

            :return: The action result in case of success, or an error dictionary
                    in case of failure

        """

        if check_connection:
            # Verifies if connect() was called
            if not self._connected:
                return {
                    'exception': True,
                    'error': "To use this, a connection must be established first",
                    'error_code': 0
                }

        try:
            return action()
        except ApiException as e:
            Log.save_exception(e)
            return {
                'exception': True,
                'error': e.reason,
                'error_code': e.status,
                'descr': e.body,
            }
        except MaxRetryError as e:
            Log.save_exception(e)
            return {
                'exception': True,
                'error': e.reason,
                'error_code': ERROR_CONNECTION_REFUSED,
            }
        except TimeoutError as e:
            Log.save_exception(e)
            return {
                'exception': True,
                'error': "A socket timeout error occurs.",
                'error_code': ERROR_HOST_TIMEOUT
            }
        except SSLError as e:
            Log.save_exception(e)
            return {
                'exception': True,
                'error': "SSL certificate fails in an HTTPS connection.",
                'error_code': ERROR_SSL_FAILED
            }
        except OSError as e:
            Log.save_exception(e)
            if e.__class__.__name__ == "ConnectionError":
                error_code = ERROR_HOST_NOT_FOUND
                error_message = "Host is Down"
            elif e.__class__.__name__ == "ConnectTimeout":
                error_code = ERROR_HOST_TIMEOUT
                error_message = "Host Timeout"
            else:
                error_code = UNRECOGNIZED_CONNECTION_ERROR
                error_message = "Unrecognized Error"

            return {
                'exception': True,
                'error': error_message,
                'error_code': error_code
            }
        except Exception as e:
            Log.save_exception(e)
            return {
                'exception': True,
                'error': "Unrecognized Error",
                'error_code': UNRECOGNIZED_CONNECTION_ERROR
            }

    def __extract_file_info(self, bucket, file_name, headers):
        """
            Extracts the file information from the swift object Stat data structure

            :param bucket: The objects bucket
            :param file_name: The objects name
            :param headers: The objects headers

            :return: the file information data structure

        """
        # TODO: unused?
        accessed_at_gmt = headers["date"]
        accessed_at_ts = gmt_to_unix_timestamp(accessed_at_gmt)
        modified_at_gmt = headers["last-modified"]
        modified_at_ts = gmt_to_unix_timestamp(modified_at_gmt)
        created_at_ts = int(float(headers["x-timestamp"]))
        file_size = int(headers["content-length"])
        file_type = NOT_EMPTY_FILE if file_size > 0 else EMPTY_FILE

        return {
            "name": file_name,
            "type": file_type,
            "size": file_size,
            "uid": 0,
            "gid": 0,
            "mode": DEFAULT_FILE_MODE,
            "nlink": 1,
            "modified-at": modified_at_ts,
            "accessed-at": accessed_at_ts,
            "created-at": created_at_ts,
        }

    def create_backup_pod(self, namespace, poddata):
        pod = client.V1Pod(
            api_version=poddata.get('apiVersion'),
            kind=poddata.get('kind'),
            metadata=poddata.get('metadata'),
            spec=poddata.get('spec')
        )
        response = self.__execute(lambda: self.corev1api.create_namespaced_pod(namespace=namespace, body=pod, pretty=True))
        if isinstance(response, dict) and "error" in response:
            return response
        return {}

    def create_pvc_clone(self, namespace, pvcclonedata):
        pvcclone = client.V1PersistentVolumeClaim(
            api_version=pvcclonedata.get('apiVersion'),
            kind=pvcclonedata.get('kind'),
            metadata=pvcclonedata.get('metadata'),
            spec=pvcclonedata.get('spec')
        )
        response = self.__execute(lambda: self.corev1api.create_namespaced_persistent_volume_claim(namespace=namespace, body=pvcclone, pretty=True))
        if isinstance(response, dict) and "error" in response:
            return response
        return {}

    def vsnapshot_isready(self, namespace, snapshot_name):
        response = self._vsnapshot_status(namespace, snapshot_name)
        if isinstance(response, dict) and "error" in response:
            return response
        status = response.get('status')
        logging.debug("vsnapshot_isready:ReadyToUse:{}".format(status))
        if status is None:
            return False
        return status.get('readyToUse')

    def create_vsnapshot(self, namespace, pvc):
        pvcname = pvc.get('name')
        logging.info("Creating vsnapshot of pvc `{}`".format(pvc))
        storage_provisioner =  get_provisioner(self.storagev1api, pvc.get('storage_class_name'))
        snapshot = prepare_create_snapshot_body(self.crd_api, namespace, pvcname, self._params.get('jobid'), storage_provisioner)
        logging.debug("Body to create vsnapshot:\n`{}`\n".format(snapshot))
        response = self.__execute(lambda: self.crd_api.create_namespaced_custom_object(**snapshot, pretty=True))
        if isinstance(response, dict) and "error" in response:
            return response
        snapshot_name = snapshot.get("body").get("metadata").get("name")
        logging.debug('Request create snapshot `{}` sent correctly'.format(snapshot_name))
        wait_until_resource_is_ready(
            lambda: self.vsnapshot_isready(namespace, snapshot_name),
            "Waiting create volume snapshot `{}` to be ready.".format(snapshot_name)
        )
        logging.info("Created successfully vsnapshot of pvc `{}`".format(pvcname))
        return {
            'name': snapshot_name,
            'kind': snapshot.get('body').get('kind'),
            'source': pvcname,
            'namespace': namespace,
            'volume_snapshot_class_name': snapshot.get('body').get('spec').get('volumeSnapshotClassName')
        }

    def create_pvc_from_vsnapshot(self, namespace, pvcdata):
        new_pvc = prepare_pvc_from_vsnapshot_body(namespace, pvcdata, self._params.get('jobid'))
        logging.debug("Body to create pvc from vsnapshot: `{}`".format(new_pvc))
        response = self.create_pvc_clone(namespace, new_pvc)
        if isinstance(response, dict) and 'error' in response:
            return response
        response = self.get_pvcdata_namespaced(namespace, new_pvc.get('metadata').get('name'))
        if isinstance(response, dict) and 'error' in response:
            return response
        logging.debug('PVC from vsnapshot Response: {}'.format(response))
        new_pvc_name = response.get('name')
        wait_until_resource_is_ready(
            lambda: self.pvc_status(namespace, new_pvc_name),
            'Waiting create pvc from snapshot `{}` to be ready'.format(new_pvc_name)
        )
        return {
            'name': new_pvc_name,
            'node_name': response.get('node_name'),
            'storage_class_name': response.get('storage_class_name'),
            'capacity': response.get('capacity'),
            'fi': pvcdata.get('fi')
        }

    def backup_pod_status(self, namespace):
        return self.corev1api.read_namespaced_pod_status(name=BACULABACKUPPODNAME, namespace=namespace)

    def pvc_status(self, namespace, pvcname):
        return self.__execute(lambda: self.corev1api.read_namespaced_persistent_volume_claim_status(name=pvcname, namespace=namespace))

    def _vsnapshot_status(self, namespace, snapshot_name):
        return self.__execute(lambda: self.crd_api.get_namespaced_custom_object_status(**prepare_snapshot_action(namespace, snapshot_name)))

    def backup_pod_isready(self, namespace, seq=None, podname=BACULABACKUPPODNAME):
        pod = self.backup_pod_status(namespace)
        status = pod.status
        # logging.debug("backup_pod_isready:status:{} {}".format(type(status), status))
        if status.container_statuses is None:
            if status.reason is None:
                # the Pod constainer status is not available yet
                return False
            err = K8S_POD_CONTAINER_STATUS_ERROR.format(status.reason, status.message)
            logging.error(err)
            return {'error': err}
        isready = status.container_statuses[0].ready
        logging.info("backup_pod_status:isReady: {} / {}".format(isready, seq))
        return isready

    def pvc_isready(self, namespace, pvcname):
        response = self.pvc_status(namespace, pvcname)
        if isinstance(response, dict) and "error" in response:
            return response
        status = response.status
        logging.debug("pvc_isready:status:{}".format(status))
        return status.phase == 'Bound'

    def remove_backup_pod(self, namespace, podname=BACULABACKUPPODNAME):
        logging.debug('remove_backup_pod')
        response = self.__execute(lambda: self.corev1api.delete_namespaced_pod(
            podname, namespace, grace_period_seconds=0,
            propagation_policy='Foreground'))
        if isinstance(response, dict) and "error" in response:
            return response
        return {}

    def remove_pvcclone(self, namespace, clonename):
        logging.debug('remove_pvcclone `{}`'.format(clonename))
        response = self.__execute(lambda: self.corev1api.delete_namespaced_persistent_volume_claim(
            clonename, namespace, grace_period_seconds=0,
            propagation_policy='Foreground'))
        logging.debug("Response: {}".format(response))
        if isinstance(response, dict) and "error" in response:
            return response
        r1 = self.get_pvcdata_namespaced(namespace, clonename)
        wait_until_resource_is_ready(lambda: 'error' in self.get_pvcdata_namespaced(namespace, clonename))
        r2 = self.get_pvcdata_namespaced(namespace, clonename)
        logging.debug('PVC removed `{}`'.format(clonename))
        return {}

    def remove_vsnapshot(self, namespace, vsnapshot_name):
        logging.debug('remove_vsnapshot `{}`'.format(vsnapshot_name))
        response = self.__execute(lambda: self.crd_api.delete_namespaced_custom_object(**prepare_snapshot_action(namespace, vsnapshot_name)))
        logging.debug('Response remove_vsnapshot {}'.format(response))
        if isinstance(response, dict) and "error" in response:
            return response
        logging.debug('Volume Snapshot removed `{}`'.format(vsnapshot_name))
        return {}

    def check_gone_backup_pod(self, namespace, force=False):
        """ Checks if $BACULABACKUPPODNAME at selected namespace is already running.
            If not then we can proceed with Job. If it terminated but not removed then we will safely remove it.
        Args:
            namespace (str): namespace for Pod
            force (bool, optional): when we want to remove pod in any state. Defaults to False.

        Returns:
            bool: True if Pod not exist else False
        """
        # TODO: Refactor this method as it does more then described!
        status = None
        gone = False
        try:
            status = self.backup_pod_status(namespace)
        except ApiException as e:
            if e.status == HTTP_NOT_FOUND:
                gone = True
                return True
        finally:
            logging.info("check_gone_backup_pod:gone:" + str(gone))
        if status is not None and (force or status.status.phase not in ['Pending', 'Running']):
            response = self.remove_backup_pod(namespace)
            if isinstance(response, dict) and 'error' in response:
                # propagate error up
                return response
        return False

    def check_gone_pvcclone(self, namespace, clonename, force=False):
        response = self.pvc_status(namespace, clonename)
        if isinstance(response, dict) and "error" in response:
            logging.info("pvc status: {}".format(response))
            if response.get('error_code') != HTTP_NOT_FOUND:
                return response
            else:
                return True
        status = response.status
        logging.info("status: {}".format(status))
        if status is not None and status.phase != 'Pending':
            response = self.remove_pvcclone(namespace, clonename)
            if isinstance(response, dict) and 'error' in response:
                # propagate error up
                return response
        return False


class FileContentAdapter:
    """
        This Iterator Class is used to upload Object Segments.
        It is used by the Python-swiftclient connection API,
        and dependes upon a File Stream. It should read from
        the File Stream until segment_size is reached.

    """

    def __init__(self, stream, segment_size):
        self.stream = stream
        self.segment_size = segment_size
        self.should_stop = False
        self.resend_segment = False
        self.current_segment = None

    def tell(self):
        """
            Called by Python-swiftclient to resend data in case of error
        """
        self.resend_segment = True
        pass

    def seek(self, pos):
        """
            Called by Python-swiftclient to resend data in case of error
        """
        self.resend_segment = True
        pass

    def __iter__(self):
        return self

    def __next__(self):
        """
            Called by Python-swiftclient to retrieve the Object Content
        """

        if self.should_stop:
            raise StopIteration

        if self.resend_segment and self.current_segment is not None:
            self.resend_segment = False
            return self.current_segment

        next_segment = b''
        next_segment_size = 0

        while True:
            chunk = self.stream.read()

            if not chunk:
                self.should_stop = True
                break

            next_segment += chunk
            next_segment_size += len(chunk)

            if next_segment_size >= self.segment_size:
                self.should_stop = True
                break

        if next_segment_size == 0:
            raise StopIteration

        self.current_segment = next_segment
        return next_segment
