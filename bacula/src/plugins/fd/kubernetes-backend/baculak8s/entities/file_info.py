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

import fnmatch
import re

NOT_EMPTY_FILE = "F"
EMPTY_FILE = "E"
DIRECTORY = "D"
SYMBOLIC_LINK = "S"
HARD_LINK = "L"
DEFAULT_FILE_MODE = "100640"  # -rw-r-----
DEFAULT_DIR_MODE = "040755"  # drwxr-xr-x
MEGABYTE = 1024 * 1024


class FileInfo(object):
    """
        Entity representing information about a File
    """

    def __init__(self, name,
                 ftype, size, uid, gid,
                 accessed_at, modified_at, created_at,
                 mode, nlink, index=None,
                 namespace=None, objtype=None,
                 fullfname=[]):
        self.name = name
        self.type = ftype
        self.size = size
        self.uid = uid
        self.gid = gid
        self.mode = mode
        self.nlink = nlink
        self.index = index
        self.accessed_at = accessed_at
        self.modified_at = modified_at
        self.created_at = created_at
        self.namespace = namespace
        self.objtype = objtype
        self.objcache = None
        self.fullfname = fullfname

    def __str__(self):
        return '{{FileInfo name:{} namespace:{} type:{} objtype:{} cached:{}}}'\
            .format(str(self.name),
                    str(self.namespace),
                    str(self.type),
                    str(self.objtype),
                    self.objcache is not None)


    def set_name(self, name: str):
        self.name = name


    def is_bucket(self):
        return self.type == DIRECTORY and not self.name

    def match_any_glob(self, globs, current_matches):
        """
            Verifies whether this File matches any glob inside $globs$.
            If it does, the $current_matches$ list will be updated.
        """
        any_match = False

        for glob in globs:
            # Glob check
            if fnmatch.fnmatchcase(self.name, glob):
                any_match = True
                current_matches.append(glob)
                break

        return any_match

    def match_any_regex(self, regexes, current_matches):
        """
            Verifies whether this File matches any regex inside $regexs$.
            If it does, the $current_matches$ list will be updated.
        """

        any_match = False

        for regex in regexes:
            if re.match(regex, self.name):
                any_match = True
                current_matches.append(regex)
                break

        return any_match

    def apply_regexwhere_param(self, regexwhere):
        """
            Applies a $regexwhere$ into this File, updating it's name.
        """
        patterns = regexwhere.split(",")

        for pattern in patterns:
            re_flags = 0

            if pattern.endswith("/i"):
                re_flags += re.IGNORECASE

            separator = pattern[0]
            splitted = pattern \
                .strip(separator) \
                .split(separator)

            splitted[1] = splitted[1].replace("$", "\\")

            # We remove empty entries
            splitted = list(filter(None, splitted))
            new_name = re.sub(r'%s' % splitted[0],
                              r'' + splitted[1],
                              self.name,
                              flags=re_flags)
            self.name = new_name
