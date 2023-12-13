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
#     Copyright (c) 2019 by Inteos sp. z o.o.
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

from baculak8s.entities.file_info import NOT_EMPTY_FILE
from baculak8s.plugins.k8sbackend.k8sfileinfo import *
from baculak8s.plugins.k8sbackend.k8sutils import *


def namespace_read(corev1api, name):
    return corev1api.read_namespace(name)


def namespaces_list_all(corev1api, nsfilter=None, estimate=False):
    nslist = {}
    namespaces = corev1api.list_namespace(watch=False)
    for ns in namespaces.items:
        if nsfilter is not None and len(nsfilter) > 0:
            if ns.metadata.name not in nsfilter:
                continue
        nsdata = namespace_read(corev1api, ns.metadata.name)
        spec = encoder_dump(nsdata)
        nslist[ns.metadata.name] = {
            'name': ns.metadata.name,
            'spec': spec if not estimate else None,
            'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_NAMESPACE, nsname=ns.metadata.name,
                              name=ns.metadata.name,
                              ftype=NOT_EMPTY_FILE,
                              size=len(spec),
                              creation_timestamp=nsdata.metadata.creation_timestamp),
        }
    return nslist


def namespace_names(corev1api):
    nslist = []
    namespaces = corev1api.list_namespace(watch=False)
    for ns in namespaces.items:
        nslist.append(["namespace", ns.metadata.name])
    return nslist


def namespaces_list_all_names(corev1api):
    nslist = {}
    namespaces = corev1api.list_namespace(watch=False)
    for ns in namespaces.items:
        nslist[ns.metadata.name] = {
            'fi': FileInfo(name="/{}/{}".format(K8SObjType.K8SOBJ_NAMESPACE_Path, ns.metadata.name),
                           ftype=DIRECTORY,
                           size=0,
                           uid=0, gid=0,
                           mode=DEFAULT_DIR_MODE,
                           nlink=1,
                           modified_at=NOW_TIMESTAMP,
                           accessed_at=NOW_TIMESTAMP,
                           created_at=k8stimestamp_to_unix_timestamp(ns.metadata.creation_timestamp)),
        }
    return nslist


def namespaces_restore(corev1api, file_info, file_content):
    ns = encoder_load(file_content, file_info.name)
    metadata = prepare_metadata(ns.metadata)
    projectid = None
    # Populate annotations about projectId
    if ns.metadata.annotations is not None:
        projectid = ns.metadata.annotations.get('field.cattle.io/projectId', None)
    if projectid is not None:
        ann = {'field.cattle.io/projectId': projectid}
        if metadata.annotations is not None:
            metadata.annotations.update(ann)
        else:
            metadata.annotations = ann
    # Instantiate the namespace object
    namespace = client.V1Namespace(
        api_version=ns.api_version,
        kind="Namespace",
        spec=ns.spec,
        metadata=metadata
    )
    if file_info.objcache is not None:
        # TODO: Do we really need to replace existing namespace?
        response = corev1api.replace_namespace(k8sfile2objname(file_info.name), namespace, pretty='true')
    else:
        # object does not exist, so create one as required
        response = corev1api.create_namespace(namespace, pretty='true')
    return {'response': response}
