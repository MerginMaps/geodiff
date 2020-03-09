# -*- coding: utf-8 -*-
'''
    pygeodiff.geodifflib
    --------------------
    This module provides wrapper of geodiff C library
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

import ctypes
import os
import platform
from ctypes.util import find_library
from .__about__ import __version__


class GeoDiffLibError(Exception):
    pass


class GeoDiffLibConflictError(GeoDiffLibError):
    pass

class GeoDiffLibUnsupportedChangeError(GeoDiffLibError):
    pass

class GeoDiffLibVersionError(GeoDiffLibError):
    pass


# keep in sync with c-library
SUCCESS=0
ERROR=1
CONFLICT=2
UNSUPPORTED_CHANGE=3

def _parse_return_code(rc, msg):
    if rc == SUCCESS:
        return
    elif rc == ERROR:
        raise GeoDiffLibError(msg)
    elif rc == CONFLICT:
        raise GeoDiffLibConflictError(msg)
    elif rc == UNSUPPORTED_CHANGE:
        raise GeoDiffLibUnsupportedChangeError(msg)
    else:
        raise GeoDiffLibVersionError("Internal error (enum " + str(rc) + " not handled)")

class GeoDiffLib:
    def __init__(self, name):
        if name is None:
            self.libname = self.package_libname()
            if not os.path.exists(self.libname):
                # not found, try system library
                self.libname = find_library("geodiff")
        else:
            self.libname = name

        if self.libname is None:
            raise GeoDiffLibVersionError("Unable to locate GeoDiff library, tried " + self.package_libname() + " and geodiff on system.")

        self.lib = ctypes.CDLL(self.libname, use_errno=True)
        self.init()
        self.check_version()

    def package_libname(self):
        # assume that the package is installed through PIP
        if platform.system() == 'Windows':
            prefix = ""
            arch = platform.architecture()[0]  # 64bit or 32bit
            if "32" in arch:
                suffix = "-win32.dll"
            else:
                suffix = ".dll"
        elif platform.system() == 'Darwin':
            prefix = "lib"
            suffix = ".dylib"
        else:
            prefix = "lib"
            suffix = ".so"
        whl_lib = prefix + 'pygeodiff-' + __version__ + '-python' + suffix
        dir_path = os.path.dirname(os.path.realpath(__file__))
        return os.path.join(dir_path, whl_lib)

    def init(self):
        func = self.lib.GEODIFF_init
        func()

    def set_logger_callback(self, callback):
        func = self.lib.GEODIFF_setLoggerCallback
        CMPFUNC = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_char_p)
        func.argtypes = [CMPFUNC]
        # do not remove self, callback needs to be member
        self.callbackLogger = CMPFUNC(callback)
        func(self.callbackLogger)

    def set_maximum_logger_level(self, maxLevel):
        func = self.lib.GEODIFF_setMaximumLoggerLevel
        func.argtypes = [ctypes.c_int]
        func(maxLevel)

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

    def invert_changeset(self, changeset, changeset_inv):
        func = self.lib.GEODIFF_invertChangeset
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode('utf-8')
        b_string2 = changeset_inv.encode('utf-8')

        res = func(b_string1, b_string2)
        _parse_return_code(res, "invert_changeset")

    def create_rebased_changeset(self, base, modified, changeset_their, changeset, conflict):
        func = self.lib.GEODIFF_createRebasedChangeset
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        b_string2 = modified.encode('utf-8')
        b_string3 = changeset_their.encode('utf-8')
        b_string4 = changeset.encode('utf-8')
        b_string5 = conflict.encode('utf-8')

        res = func(b_string1, b_string2, b_string3, b_string4, b_string5)
        _parse_return_code(res, "createRebasedChangeset")

    def rebase(self, base, modified_their, modified, conflict):
        func = self.lib.GEODIFF_rebase
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        b_string2 = modified_their.encode('utf-8')
        b_string3 = modified.encode('utf-8')
        b_string4 = conflict.encode('utf-8')
        res = func(b_string1, b_string2, b_string3, b_string4)
        _parse_return_code(res, "rebase")

    def apply_changeset(self, base, changeset):
        func = self.lib.GEODIFF_applyChangeset
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode('utf-8')
        b_string2 = changeset.encode('utf-8')

        res = func(b_string1, b_string2)
        _parse_return_code(res, "apply_changeset")

    def list_changes(self, changeset, result):
        func = self.lib.GEODIFF_listChanges
        func.argtypes = [ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode('utf-8')
        b_string2 = result.encode('utf-8')
        res = func(b_string1, b_string2)
        _parse_return_code(res, "list_changes")

    def list_changes_summary(self, changeset, result):
        func = self.lib.GEODIFF_listChangesSummary
        func.argtypes = [ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode('utf-8')
        b_string2 = result.encode('utf-8')
        res = func(b_string1, b_string2)
        _parse_return_code(res, "list_changes_summary")

    def has_changes(self, changeset):
        func = self.lib.GEODIFF_hasChanges
        func.argtypes = [ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode('utf-8')

        nchanges = func(b_string1)
        if nchanges < 0:
            raise GeoDiffLibError("has_changes")
        return nchanges == 1

    def changes_count(self, changeset):
        func = self.lib.GEODIFF_changesCount
        func.argtypes = [ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode('utf-8')

        nchanges = func(b_string1)
        if nchanges < 0:
            raise GeoDiffLibError("changes_count")
        return nchanges
