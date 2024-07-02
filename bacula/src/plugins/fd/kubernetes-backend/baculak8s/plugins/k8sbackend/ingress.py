# -*- coding: UTF-8 -*-
#
#  Bacula® - The Network Backup Solution
#
#     Copyright (C) 2000-2020 Bacula Systems SA
#     All rights reserved.
#
#     The main author of Bacula is Kern Sibbald, with contributions from many
#     others, a complete list can be found in the file AUTHORS.
#
#     Licensees holding a valid Bacula Systems SA license may use this file
#     and others of this release in accordance with the proprietary license
#     agreement provided in the LICENSE file.  Redistribution of any part of
#     this release is not permitted.
#
#     Bacula® is a registered trademark of Kern Sibbald.
#
#     Copyright (c) 2019 by Inteos sp. z o.o.
#     All rights reserved. IP transfered to Bacula Systems according to agreement.
#     Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
#

from baculak8s.entities.file_info import NOT_EMPTY_FILE
from baculak8s.plugins.k8sbackend.k8sfileinfo import *
from baculak8s.plugins.k8sbackend.k8sutils import *

def ingress_check(networkv1api, file_info):
    return networkv1api.read_namespaced_ingress(k8sfile2objname(file_info.name), file_info.namespace)


def ingress_read_namespaced(networkv1api, namespace, name):
    return networkv1api.read_namespaced_ingress(name, namespace)


def ingress_list_namespaced(networkv1api, namespace, estimate=False, labels=""):
    ingresslist = {}
    ingresses = networkv1api.list_namespaced_ingress(namespace=namespace, watch=False, label_selector=labels)
    for ingress in ingresses.items:
        ingressdata = ingress_read_namespaced(networkv1api, namespace, ingress.metadata.name)
        spec = encoder_dump(ingressdata)
        ingresslist['ingress-' + ingress.metadata.name] = {
            'spec': spec if not estimate else None,
            'fi': k8sfileinfo(objtype=K8SObjType.K8SOBJ_INGRESS, nsname=namespace,
                              name=ingress.metadata.name,
                              ftype=NOT_EMPTY_FILE,
                              size=len(spec),
                              creation_timestamp=ingressdata.metadata.creation_timestamp),
        }
    return ingresslist


def ingress_restore_namespaced(networkv1api, file_info, file_content):
    ingress = encoder_load(file_content, file_info.name)
    metadata = prepare_metadata(ingress.metadata)
    # Instantiate the daemon_set object
    statefulset = client.V1Ingress(
        api_version=ingress.api_version,
        kind="Ingress",
        spec=ingress.spec,
        metadata=metadata
    )
    if file_info.objcache is not None:
        # object exist so we replace it
        response = networkv1api.replace_namespaced_ingress(k8sfile2objname(file_info.name),
                                                             file_info.namespace, statefulset, pretty='true')
    else:
        # object does not exist, so create one as required
        response = networkv1api.create_namespaced_ingress(file_info.namespace, statefulset, pretty='true')
    return {'response': response}
