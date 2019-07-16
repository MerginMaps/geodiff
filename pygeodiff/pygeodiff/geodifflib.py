# -*- coding: utf-8 -*-
'''
    pygeodiff.geodifflib
    --------------------
    This module provides wrapper of geodiff C library
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

import ctypes
from .__about__ import __version__


class GeoDiffLibError(Exception):
    pass


class GeoDiffLibConflictError(GeoDiffLibError):
    pass


class GeoDiffLibVersionError(GeoDiffLibError):
    pass


# keep in sync with c-library
SUCCESS=0
ERROR=1
CONFLICT=2


def _parse_return_code(rc, msg):
    if rc == SUCCESS:
        return
    elif rc == ERROR:
        raise GeoDiffLibError(msg)
    elif rc == CONFLICT:
        raise GeoDiffLibConflictError(msg)


class GeoDiffLib:
    def __init__(self, name):
        self.libname = name
        self.lib = ctypes.CDLL(name)
        self.init()
        self.check_version()

    def init(self):
        func = self.lib.GEODIFF_init
        func()

    def version(self):
        func = self.lib.GEODIFF_version
        func.restype = ctypes.c_char_p
        ver = func()
        return ver.decode('utf-8')

    def check_version(self):
        cversion = self.version()
        pyversion = __version__
        if cversion != pyversion:
            raise GeoDiffLibVersionError("version mismatch (%s C vs %s PY)".format(cversion, pyversion))

    def create_changeset(self, base, modified, changeset):
        func = self.lib.GEODIFF_createChangeset
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        b_string2 = modified.encode('utf-8')
        b_string3 = changeset.encode('utf-8')

        res = func(b_string1, b_string2, b_string3)
        _parse_return_code(res, "createChangeset")

    def create_rebased_changeset(self, base, modified, changeset_their, changeset):
        func = self.lib.GEODIFF_createRebasedChangeset
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        b_string2 = modified.encode('utf-8')
        b_string3 = changeset_their.encode('utf-8')
        b_string4 = changeset.encode('utf-8')

        res = func(b_string1, b_string2, b_string3, b_string4)
        _parse_return_code(res, "createRebasedChangeset")

    def apply_changeset(self, base, patched, changeset):
        func = self.lib.GEODIFF_applyChangeset
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        b_string2 = patched.encode('utf-8')
        b_string3 = changeset.encode('utf-8')

        res = func(b_string1, b_string2, b_string3)
        _parse_return_code(res, "createRebasedChangeset")

    def list_changes(self, base ):
        func = self.lib.GEODIFF_listChanges
        func.argtypes = [ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        nchanges = func(b_string1)
        if nchanges < 0:
            raise GeoDiffLibError("listChanges")
        return nchanges
