#!/usr/bin/env python3
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
import sys

from setuptools import setup, find_packages

if sys.version_info < (3, 0):
    sys.exit('This version of the Backend supports only Python 3 or above')

setup(
    name='baculak8s',
    version='2.2.0',
    author='Radoslaw Korzeniewski, Francisco Manuel Garcia Botella',
    author_email='radekk@korzeniewski.net, francisco.garcia@baculasystems.com',
    packages=find_packages(exclude=('tests', 'tests.*')),
    # packages=packages,
    license="Bacula® - The Network Backup Solution",
    data_files=[
        ('/opt/bacula/bin', ['bin/k8s_backend'])
    ],
    # scripts=['bin/k8s_backend']
)
