# -*- coding: utf-8 -*-
"""
    pygeodiff.geodifflib
    --------------------
    This module provides wrapper of geodiff C library
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

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
        self._register_functions()

    def _register_functions(self):
        self._readChangeset = self.lib.GEODIFF_readChangeset
        self._readChangeset.argtypes = [ctypes.c_char_p]
        self._readChangeset.restype = ctypes.c_void_p

        # ChangesetReader
        self._CR_nextEntry = self.lib.GEODIFF_CR_nextEntry
        self._CR_nextEntry.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._CR_nextEntry.restype = ctypes.c_void_p

        self._CR_destroy = self.lib.GEODIFF_CR_destroy
        self._CR_destroy.argtypes = [ctypes.c_void_p]

        # ChangesetEntry
        self._CE_operation = self.lib.GEODIFF_CE_operation
        self._CE_operation.argtypes = [ctypes.c_void_p]
        self._CE_operation.restype = ctypes.c_int

        self._CE_table = self.lib.GEODIFF_CE_table
        self._CE_table.argtypes = [ctypes.c_void_p]
        self._CE_table.restype = ctypes.c_void_p

        self._CE_count = self.lib.GEODIFF_CE_countValues
        self._CE_count.argtypes = [ctypes.c_void_p]
        self._CE_count.restype = ctypes.c_int

        self._CE_old_value = self.lib.GEODIFF_CE_oldValue
        self._CE_old_value.argtypes = [ctypes.c_void_p, ctypes.c_int]
        self._CE_old_value.restype = ctypes.c_void_p

        self._CE_new_value = self.lib.GEODIFF_CE_newValue
        self._CE_new_value.argtypes = [ctypes.c_void_p, ctypes.c_int]
        self._CE_new_value.restype = ctypes.c_void_p

        self._CE_destroy = self.lib.GEODIFF_CE_destroy
        self._CE_destroy.argtypes = [ctypes.c_void_p]

        # ChangesetTable
        self._CT_name = self.lib.GEODIFF_CT_name
        self._CT_name.argtypes = [ctypes.c_void_p]
        self._CT_name.restype = ctypes.c_char_p

        self._CT_column_count = self.lib.GEODIFF_CT_columnCount
        self._CT_column_count.argtypes = [ctypes.c_void_p]
        self._CT_column_count.restype = ctypes.c_int

        self._CT_column_is_pkey = self.lib.GEODIFF_CT_columnIsPkey
        self._CT_column_is_pkey.argtypes = [ctypes.c_void_p, ctypes.c_int]
        self._CT_column_is_pkey.restype = ctypes.c_bool

        # Value
        self._V_type = self.lib.GEODIFF_V_type
        self._V_type.argtypes = [ctypes.c_void_p]
        self._V_type.restype = ctypes.c_int

        self._V_get_int = self.lib.GEODIFF_V_getInt
        self._V_get_int.argtypes = [ctypes.c_void_p]
        self._V_get_int.restype = ctypes.c_int

        self._V_get_double = self.lib.GEODIFF_V_getDouble
        self._V_get_double.argtypes = [ctypes.c_void_p]
        self._V_get_double.restype = ctypes.c_double

        self._V_get_data_size = self.lib.GEODIFF_V_getDataSize
        self._V_get_data_size.argtypes = [ctypes.c_void_p]
        self._V_get_data_size.restype = ctypes.c_int

        self._V_get_data = self.lib.GEODIFF_V_getData
        self._V_get_data.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

        self._V_destroy = self.lib.GEODIFF_V_destroy
        self._V_destroy.argtypes = [ctypes.c_void_p]


    def package_libname(self):
        # assume that the package is installed through PIP
        if platform.system() == 'Windows':
            prefix = ""
            arch = platform.architecture()[0]  # 64bit or 32bit
            if "32" in arch:
                suffix = "-win32.pyd"
            else:
                suffix = ".pyd"
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
            raise GeoDiffLibVersionError("version mismatch ({} C vs {} PY)".format(cversion, pyversion))

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

    def concat_changes(self, list_changesets, output_changeset):
        # make array of char* with utf-8 encoding from python list of strings
        arr = (ctypes.c_char_p * len(list_changesets))()
        for i in range(len(list_changesets)):
            arr[i] = list_changesets[i].encode('utf-8')

        res = self.lib.GEODIFF_concatChanges(
            ctypes.c_int(len(list_changesets)),
            arr,
            ctypes.c_char_p(output_changeset.encode('utf-8')))
        _parse_return_code(res, "concat_changes")

    def make_copy(self, driver_src, driver_src_info, src, driver_dst, driver_dst_info, dst):
        res = self.lib.GEODIFF_makeCopy(
            ctypes.c_char_p(driver_src.encode('utf-8')),
            ctypes.c_char_p(driver_src_info.encode('utf-8')),
            ctypes.c_char_p(src.encode('utf-8')),
            ctypes.c_char_p(driver_dst.encode('utf-8')),
            ctypes.c_char_p(driver_dst_info.encode('utf-8')),
            ctypes.c_char_p(dst.encode('utf-8')))
        _parse_return_code(res, "make_copy")

    def make_copy_sqlite(self, src, dst):
        res = self.lib.GEODIFF_makeCopySqlite(
            ctypes.c_char_p(src.encode('utf-8')),
            ctypes.c_char_p(dst.encode('utf-8')))
        _parse_return_code(res, "make_copy_sqlite")

    def create_changeset_ex(self, driver, driver_info, base, modified, changeset):
        res = self.lib.GEODIFF_createChangesetEx(
            ctypes.c_char_p(driver.encode('utf-8')),
            ctypes.c_char_p(driver_info.encode('utf-8')),
            ctypes.c_char_p(base.encode('utf-8')),
            ctypes.c_char_p(modified.encode('utf-8')),
            ctypes.c_char_p(changeset.encode('utf-8')))
        _parse_return_code(res, "create_changeset_ex")

    def create_changeset_dr(self, driver_src, driver_src_info, src, driver_dst, driver_dst_info, dst, changeset):
        func = self.lib.GEODIFF_createChangesetDr
        func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        b_string1 = driver_src.encode('utf-8')
        b_string2 = driver_src_info.encode('utf-8')
        b_string3 = src.encode('utf-8')
        b_string4 = driver_dst.encode('utf-8')
        b_string5 = driver_dst_info.encode('utf-8')
        b_string6 = dst.encode('utf-8')
        b_string7 = changeset.encode('utf-8')

        res = func(b_string1, b_string2, b_string3, b_string4, b_string5, b_string6, b_string7)
        _parse_return_code( res, "CreateChangesetDr" )

    def apply_changeset_ex(self, driver, driver_info, base, changeset):
        res = self.lib.GEODIFF_applyChangesetEx(
            ctypes.c_char_p(driver.encode('utf-8')),
            ctypes.c_char_p(driver_info.encode('utf-8')),
            ctypes.c_char_p(base.encode('utf-8')),
            ctypes.c_char_p(changeset.encode('utf-8')))
        _parse_return_code(res, "apply_changeset_ex")

    def create_rebased_changeset_ex(self, driver, driver_info, base, base2modified, base2their, rebased, conflict_file):
        res = self.lib.GEODIFF_createRebasedChangesetEx(
            ctypes.c_char_p(driver.encode('utf-8')),
            ctypes.c_char_p(driver_info.encode('utf-8')),
            ctypes.c_char_p(base.encode('utf-8')),
            ctypes.c_char_p(base2modified.encode('utf-8')),
            ctypes.c_char_p(base2their.encode('utf-8')),
            ctypes.c_char_p(rebased.encode('utf-8')),
            ctypes.c_char_p(conflict_file.encode('utf-8')))
        _parse_return_code(res, "create_rebased_changeset_ex")

    def rebase_ex(self, driver, driver_info, base, modified, base2their, conflict_file):
        res = self.lib.GEODIFF_rebaseEx(
            ctypes.c_char_p(driver.encode('utf-8')),
            ctypes.c_char_p(driver_info.encode('utf-8')),
            ctypes.c_char_p(base.encode('utf-8')),
            ctypes.c_char_p(modified.encode('utf-8')),
            ctypes.c_char_p(base2their.encode('utf-8')),
            ctypes.c_char_p(conflict_file.encode('utf-8')))
        _parse_return_code(res, "rebase_ex")

    def dump_data(self, driver, driver_info, src, changeset):
        res = self.lib.GEODIFF_dumpData(
            ctypes.c_char_p(driver.encode('utf-8')),
            ctypes.c_char_p(driver_info.encode('utf-8')),
            ctypes.c_char_p(src.encode('utf-8')),
            ctypes.c_char_p(changeset.encode('utf-8')))
        _parse_return_code(res, "dump_data")

    def schema(self, driver, driver_info, src, json):
        res = self.lib.GEODIFF_schema(
            ctypes.c_char_p(driver.encode('utf-8')),
            ctypes.c_char_p(driver_info.encode('utf-8')),
            ctypes.c_char_p(src.encode('utf-8')),
            ctypes.c_char_p(json.encode('utf-8')))
        _parse_return_code(res, "schema")

    def read_changeset(self, changeset):

        b_string1 = changeset.encode('utf-8')

        reader_ptr = self._readChangeset(b_string1)
        if reader_ptr is None:
            raise GeoDiffLibError("Unable to open reader for: " + changeset)
        return ChangesetReader(self, reader_ptr)


class ChangesetReader(object):
    """ Wrapper around GEODIFF_CR_* functions from C API """

    def __init__(self, geodiff, reader_ptr):
        self.geodiff = geodiff
        self.reader_ptr = reader_ptr

    def __del__(self):
        self.geodiff._CR_destroy(self.reader_ptr)

    def next_entry(self):
        ok = ctypes.c_bool()
        entry_ptr = self.geodiff._CR_nextEntry(self.reader_ptr, ctypes.byref(ok))
        if not ok:
            raise GeoDiffLibError("Failed to read entry!")
        if entry_ptr is not None:
            return ChangesetEntry(self.geodiff, entry_ptr)
        else:
            return None

    def __iter__(self):
        return self

    def __next__(self):
        entry = self.next_entry()
        if entry is not None:
            return entry
        else:
            raise StopIteration

    next = __next__  # python 2.x compatibility (requires next())


class ChangesetEntry(object):
    """ Wrapper around GEODIFF_CE_* functions from C API """

    # constants as defined in ChangesetEntry::OperationType enum
    OP_INSERT = 18
    OP_UPDATE = 23
    OP_DELETE = 9

    def __init__(self, geodiff, entry_ptr):
        self.geodiff = geodiff
        self.entry_ptr = entry_ptr

        self.operation = self.geodiff._CE_operation(self.entry_ptr)
        self.values_count = self.geodiff._CE_count(self.entry_ptr)

        if self.operation == self.OP_DELETE or self.operation == self.OP_UPDATE:
            self.old_values = []
            for i in range(self.values_count):
                v_ptr = self.geodiff._CE_old_value(self.entry_ptr, i)
                self.old_values.append(self._convert_value(v_ptr))

        if self.operation == self.OP_INSERT or self.operation == self.OP_UPDATE:
            self.new_values = []
            for i in range(self.values_count):
                v_ptr = self.geodiff._CE_new_value(self.entry_ptr, i)
                self.new_values.append(self._convert_value(v_ptr))

        table = self.geodiff._CE_table(entry_ptr)
        self.table = ChangesetTable(geodiff, table)

    def __del__(self):
        self.geodiff._CE_destroy(self.entry_ptr)

    def _convert_value(self, v_ptr):
        v_type = self.geodiff._V_type(v_ptr)
        if v_type == 0:
            v_val = UndefinedValue()
        elif v_type == 1:
            v_val = self.geodiff._V_get_int(v_ptr)
        elif v_type == 2:
            v_val = self.geodiff._V_get_double(v_ptr)
        elif v_type == 3 or v_type == 4:  # 3==text, 4==blob
            size = self.geodiff._V_get_data_size(v_ptr)
            buffer = ctypes.create_string_buffer(size)
            self.geodiff._V_get_data(v_ptr, buffer)
            v_val = buffer.raw
            if v_type == 3:
                v_val = v_val.decode('utf-8')
        elif v_type == 5:
            v_val = None
        else:
            raise GeoDiffLibError("unknown value type {}".format(v_type))
        self.geodiff._V_destroy(v_ptr)
        return v_val


class ChangesetTable(object):
    """ Wrapper around GEODIFF_CT_* functions from C API """

    def __init__(self, geodiff, table_ptr):
        self.geodiff = geodiff
        self.table_ptr = table_ptr

        self.name = self.geodiff._CT_name(table_ptr).decode('utf-8')
        self.column_count = self.geodiff._CT_column_count(table_ptr)
        self.column_is_pkey = []
        for i in range(self.column_count):
            self.column_is_pkey.append(self.geodiff._CT_column_is_pkey(table_ptr, i))


class UndefinedValue(object):
    """ Marker that a value in changeset is undefined. This is different
    from NULL value (which is represented as None). Undefined values are
    used for example as values of columns in UPDATE operation that did
    not get modified. """

    def __repr__(self):
        return "<N/A>"
