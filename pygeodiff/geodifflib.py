# -*- coding: utf-8 -*-
"""
    pygeodiff.geodifflib
    --------------------
    This module provides wrapper of geodiff C library
    :copyright: (c) 2019-2022 Lutra Consulting Ltd.
    :license: MIT, see LICENSE for more details.
"""

import ctypes
import os
import platform
from ctypes.util import find_library
from .__about__ import __version__
import copy


class GeoDiffLibError(Exception):
    pass


class GeoDiffLibConflictError(GeoDiffLibError):
    pass


class GeoDiffLibUnsupportedChangeError(GeoDiffLibError):
    pass


class GeoDiffLibVersionError(GeoDiffLibError):
    pass


# keep in sync with c-library
SUCCESS = 0
ERROR = 1
CONFLICT = 2
UNSUPPORTED_CHANGE = 3


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
            raise GeoDiffLibVersionError(
                "Unable to locate GeoDiff library, tried "
                + self.package_libname()
                + " and geodiff on system."
            )

        try:
            self.lib = ctypes.CDLL(self.libname, use_errno=True)
        except OSError:
            raise GeoDiffLibVersionError(
                "Unable to load geodiff library " + self.libname
            )

        self.check_version()
        self._register_functions()

    def shutdown(self):
        if platform.system() == "Windows":
            from _ctypes import FreeLibrary

            FreeLibrary(self.lib._handle)
        self.lib = None

    def _register_functions(self):
        self._readChangeset = self.lib.GEODIFF_readChangeset
        self._readChangeset.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self._readChangeset.restype = ctypes.c_void_p

        # ChangesetReader
        self._CR_nextEntry = self.lib.GEODIFF_CR_nextEntry
        self._CR_nextEntry.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
        ]
        self._CR_nextEntry.restype = ctypes.c_void_p

        self._CR_destroy = self.lib.GEODIFF_CR_destroy
        self._CR_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

        # ChangesetEntry
        self._CE_operation = self.lib.GEODIFF_CE_operation
        self._CE_operation.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._CE_operation.restype = ctypes.c_int

        self._CE_table = self.lib.GEODIFF_CE_table
        self._CE_table.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._CE_table.restype = ctypes.c_void_p

        self._CE_count = self.lib.GEODIFF_CE_countValues
        self._CE_count.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._CE_count.restype = ctypes.c_int

        self._CE_old_value = self.lib.GEODIFF_CE_oldValue
        self._CE_old_value.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
        self._CE_old_value.restype = ctypes.c_void_p

        self._CE_new_value = self.lib.GEODIFF_CE_newValue
        self._CE_new_value.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
        self._CE_new_value.restype = ctypes.c_void_p

        self._CE_destroy = self.lib.GEODIFF_CE_destroy
        self._CE_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

        # ChangesetTable
        self._CT_name = self.lib.GEODIFF_CT_name
        self._CT_name.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._CT_name.restype = ctypes.c_char_p

        self._CT_column_count = self.lib.GEODIFF_CT_columnCount
        self._CT_column_count.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._CT_column_count.restype = ctypes.c_int

        self._CT_column_is_pkey = self.lib.GEODIFF_CT_columnIsPkey
        self._CT_column_is_pkey.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_int,
        ]
        self._CT_column_is_pkey.restype = ctypes.c_bool

        # Value
        self._V_type = self.lib.GEODIFF_V_type
        self._V_type.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._V_type.restype = ctypes.c_int

        self._V_get_int = self.lib.GEODIFF_V_getInt
        self._V_get_int.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._V_get_int.restype = ctypes.c_int

        self._V_get_double = self.lib.GEODIFF_V_getDouble
        self._V_get_double.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._V_get_double.restype = ctypes.c_double

        self._V_get_data_size = self.lib.GEODIFF_V_getDataSize
        self._V_get_data_size.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self._V_get_data_size.restype = ctypes.c_int

        self._V_get_data = self.lib.GEODIFF_V_getData
        self._V_get_data.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p]

        self._V_destroy = self.lib.GEODIFF_V_destroy
        self._V_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

    def _parse_return_code(self, context, rc, ctx):
        if rc == SUCCESS:
            return

        get_error = self.lib.GEODIFF_CX_lastError
        get_error.restype = ctypes.c_char_p
        get_error.argtypes = [ctypes.c_void_p]
        err = get_error(context).decode("utf-8")

        msg = "Error in " + ctx + ":\n" + err
        if rc == ERROR:
            raise GeoDiffLibError(msg)
        elif rc == CONFLICT:
            raise GeoDiffLibConflictError(msg)
        elif rc == UNSUPPORTED_CHANGE:
            raise GeoDiffLibUnsupportedChangeError(msg)
        else:
            raise GeoDiffLibVersionError(
                "Internal error (enum " + str(rc) + " not handled)"
            )

    def package_libname(self):
        # assume that the package is installed through PIP
        if platform.system() == "Windows":
            prefix = ""
            arch = platform.architecture()[0]  # 64bit or 32bit
            if "32" in arch:
                suffix = "-win32.pyd"
            else:
                suffix = ".pyd"
        elif platform.system() == "Darwin":
            prefix = "lib"
            suffix = ".dylib"
        else:
            prefix = "lib"
            suffix = ".so"
        whl_lib = prefix + "pygeodiff-" + __version__ + "-python" + suffix
        dir_path = os.path.dirname(os.path.realpath(__file__))
        return os.path.join(dir_path, whl_lib)

    def create_context(self):
        func = self.lib.GEODIFF_createContext
        func.restype = ctypes.c_void_p
        context = func()
        if context is None:
            raise GeoDiffLibVersionError("Unable to create GeoDiff context")
        return context

    def destroy_context(self, context):
        if context is not None:
            func = self.lib.GEODIFF_CX_destroy
            func.argtypes = [ctypes.c_void_p]
            func(context)

    def set_logger_callback(self, context, callback):
        func = self.lib.GEODIFF_CX_setLoggerCallback
        cFuncType = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_char_p)
        func.argtypes = [ctypes.c_void_p, cFuncType]
        if callback:
            # do not remove self, callback needs to be member
            callbackLogger = cFuncType(callback)
        else:
            callbackLogger = cFuncType()
        func(context, callbackLogger)
        return callbackLogger

    def set_maximum_logger_level(self, context, maxLevel):
        func = self.lib.GEODIFF_CX_setMaximumLoggerLevel
        func.argtypes = [ctypes.c_void_p, ctypes.c_int]
        func(context, maxLevel)

    def set_tables_to_skip(self, context, tables):
        # make array of char* with utf-8 encoding from python list of strings
        arr = (ctypes.c_char_p * len(tables))()
        for i in range(len(tables)):
            arr[i] = tables[i].encode("utf-8")

        self.lib.GEODIFF_CX_setTablesToSkip(
            ctypes.c_void_p(context), ctypes.c_int(len(tables)), arr
        )

    def version(self):
        func = self.lib.GEODIFF_version
        func.restype = ctypes.c_char_p
        ver = func()
        return ver.decode("utf-8")

    def check_version(self):
        cversion = self.version()
        pyversion = __version__
        if cversion != pyversion:
            raise GeoDiffLibVersionError(
                "version mismatch ({} C vs {} PY)".format(cversion, pyversion)
            )

    def drivers(self, context):
        _driver_count_f = self.lib.GEODIFF_driverCount
        _driver_count_f.argtypes = [ctypes.c_void_p]
        _driver_count_f.restype = ctypes.c_int

        _driver_name_from_index_f = self.lib.GEODIFF_driverNameFromIndex
        _driver_name_from_index_f.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_char_p,
        ]
        _driver_name_from_index_f.restype = ctypes.c_int

        drivers_list = []
        driversCount = _driver_count_f(context)
        for index in range(driversCount):
            name_raw = 256 * ""
            b_string1 = name_raw.encode("utf-8")
            res = _driver_name_from_index_f(context, index, b_string1)
            self._parse_return_code(context, res, "drivers")
            name = b_string1.decode("utf-8")
            drivers_list.append(name)

        return drivers_list

    def driver_is_registered(self, context, name):
        func = self.lib.GEODIFF_driverIsRegistered
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        func.restype = ctypes.c_bool

        b_string1 = name.encode("utf-8")
        return func(context, b_string1)

    def create_changeset(self, context, base, modified, changeset):
        func = self.lib.GEODIFF_createChangeset
        func.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode("utf-8")
        b_string2 = modified.encode("utf-8")
        b_string3 = changeset.encode("utf-8")

        res = func(context, b_string1, b_string2, b_string3)
        self._parse_return_code(context, res, "createChangeset")

    def invert_changeset(self, context, changeset, changeset_inv):
        func = self.lib.GEODIFF_invertChangeset
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode("utf-8")
        b_string2 = changeset_inv.encode("utf-8")

        res = func(context, b_string1, b_string2)
        self._parse_return_code(context, res, "invert_changeset")

    def create_rebased_changeset(
        self, context, base, modified, changeset_their, changeset, conflict
    ):
        func = self.lib.GEODIFF_createRebasedChangeset
        func.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode("utf-8")
        b_string2 = modified.encode("utf-8")
        b_string3 = changeset_their.encode("utf-8")
        b_string4 = changeset.encode("utf-8")
        b_string5 = conflict.encode("utf-8")

        res = func(context, b_string1, b_string2, b_string3, b_string4, b_string5)
        self._parse_return_code(context, res, "createRebasedChangeset")

    def rebase(self, context, base, modified_their, modified, conflict):
        func = self.lib.GEODIFF_rebase
        func.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode("utf-8")
        b_string2 = modified_their.encode("utf-8")
        b_string3 = modified.encode("utf-8")
        b_string4 = conflict.encode("utf-8")
        res = func(context, b_string1, b_string2, b_string3, b_string4)
        self._parse_return_code(context, res, "rebase")

    def apply_changeset(self, context, base, changeset):
        func = self.lib.GEODIFF_applyChangeset
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = base.encode("utf-8")
        b_string2 = changeset.encode("utf-8")

        res = func(context, b_string1, b_string2)
        self._parse_return_code(context, res, "apply_changeset")

    def list_changes(self, context, changeset, result):
        func = self.lib.GEODIFF_listChanges
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode("utf-8")
        b_string2 = result.encode("utf-8")
        res = func(context, b_string1, b_string2)
        self._parse_return_code(context, res, "list_changes")

    def list_changes_summary(self, context, changeset, result):
        func = self.lib.GEODIFF_listChangesSummary
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode("utf-8")
        b_string2 = result.encode("utf-8")
        res = func(context, b_string1, b_string2)
        self._parse_return_code(context, res, "list_changes_summary")

    def has_changes(self, context, changeset):
        func = self.lib.GEODIFF_hasChanges
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode("utf-8")

        nchanges = func(context, b_string1)
        if nchanges < 0:
            raise GeoDiffLibError("has_changes")
        return nchanges == 1

    def changes_count(self, context, changeset):
        func = self.lib.GEODIFF_changesCount
        func.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        func.restype = ctypes.c_int

        # create byte objects from the strings
        b_string1 = changeset.encode("utf-8")

        nchanges = func(context, b_string1)
        if nchanges < 0:
            raise GeoDiffLibError("changes_count")
        return nchanges

    def concat_changes(self, context, list_changesets, output_changeset):
        # make array of char* with utf-8 encoding from python list of strings
        arr = (ctypes.c_char_p * len(list_changesets))()
        for i in range(len(list_changesets)):
            arr[i] = list_changesets[i].encode("utf-8")

        res = self.lib.GEODIFF_concatChanges(
            ctypes.c_void_p(context),
            ctypes.c_int(len(list_changesets)),
            arr,
            ctypes.c_char_p(output_changeset.encode("utf-8")),
        )
        self._parse_return_code(context, res, "concat_changes")

    def make_copy(
        self,
        context,
        driver_src,
        driver_src_info,
        src,
        driver_dst,
        driver_dst_info,
        dst,
    ):
        res = self.lib.GEODIFF_makeCopy(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver_src.encode("utf-8")),
            ctypes.c_char_p(driver_src_info.encode("utf-8")),
            ctypes.c_char_p(src.encode("utf-8")),
            ctypes.c_char_p(driver_dst.encode("utf-8")),
            ctypes.c_char_p(driver_dst_info.encode("utf-8")),
            ctypes.c_char_p(dst.encode("utf-8")),
        )
        self._parse_return_code(context, res, "make_copy")

    def make_copy_sqlite(self, context, src, dst):
        res = self.lib.GEODIFF_makeCopySqlite(
            ctypes.c_void_p(context),
            ctypes.c_char_p(src.encode("utf-8")),
            ctypes.c_char_p(dst.encode("utf-8")),
        )
        self._parse_return_code(context, res, "make_copy_sqlite")

    def create_changeset_ex(
        self, context, driver, driver_info, base, modified, changeset
    ):
        res = self.lib.GEODIFF_createChangesetEx(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver.encode("utf-8")),
            ctypes.c_char_p(driver_info.encode("utf-8")),
            ctypes.c_char_p(base.encode("utf-8")),
            ctypes.c_char_p(modified.encode("utf-8")),
            ctypes.c_char_p(changeset.encode("utf-8")),
        )
        self._parse_return_code(context, res, "create_changeset_ex")

    def create_changeset_dr(
        self,
        context,
        driver_src,
        driver_src_info,
        src,
        driver_dst,
        driver_dst_info,
        dst,
        changeset,
    ):
        func = self.lib.GEODIFF_createChangesetDr
        func.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        func.restype = ctypes.c_int

        b_string1 = driver_src.encode("utf-8")
        b_string2 = driver_src_info.encode("utf-8")
        b_string3 = src.encode("utf-8")
        b_string4 = driver_dst.encode("utf-8")
        b_string5 = driver_dst_info.encode("utf-8")
        b_string6 = dst.encode("utf-8")
        b_string7 = changeset.encode("utf-8")

        res = func(
            context,
            b_string1,
            b_string2,
            b_string3,
            b_string4,
            b_string5,
            b_string6,
            b_string7,
        )
        self._parse_return_code(context, res, "CreateChangesetDr")

    def apply_changeset_ex(self, context, driver, driver_info, base, changeset):
        res = self.lib.GEODIFF_applyChangesetEx(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver.encode("utf-8")),
            ctypes.c_char_p(driver_info.encode("utf-8")),
            ctypes.c_char_p(base.encode("utf-8")),
            ctypes.c_char_p(changeset.encode("utf-8")),
        )
        self._parse_return_code(context, res, "apply_changeset_ex")

    def create_rebased_changeset_ex(
        self,
        context,
        driver,
        driver_info,
        base,
        base2modified,
        base2their,
        rebased,
        conflict_file,
    ):
        res = self.lib.GEODIFF_createRebasedChangesetEx(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver.encode("utf-8")),
            ctypes.c_char_p(driver_info.encode("utf-8")),
            ctypes.c_char_p(base.encode("utf-8")),
            ctypes.c_char_p(base2modified.encode("utf-8")),
            ctypes.c_char_p(base2their.encode("utf-8")),
            ctypes.c_char_p(rebased.encode("utf-8")),
            ctypes.c_char_p(conflict_file.encode("utf-8")),
        )
        self._parse_return_code(context, res, "create_rebased_changeset_ex")

    def rebase_ex(
        self, context, driver, driver_info, base, modified, base2their, conflict_file
    ):
        res = self.lib.GEODIFF_rebaseEx(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver.encode("utf-8")),
            ctypes.c_char_p(driver_info.encode("utf-8")),
            ctypes.c_char_p(base.encode("utf-8")),
            ctypes.c_char_p(modified.encode("utf-8")),
            ctypes.c_char_p(base2their.encode("utf-8")),
            ctypes.c_char_p(conflict_file.encode("utf-8")),
        )
        self._parse_return_code(context, res, "rebase_ex")

    def dump_data(self, context, driver, driver_info, src, changeset):
        res = self.lib.GEODIFF_dumpData(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver.encode("utf-8")),
            ctypes.c_char_p(driver_info.encode("utf-8")),
            ctypes.c_char_p(src.encode("utf-8")),
            ctypes.c_char_p(changeset.encode("utf-8")),
        )
        self._parse_return_code(context, res, "dump_data")

    def schema(self, context, driver, driver_info, src, json):
        res = self.lib.GEODIFF_schema(
            ctypes.c_void_p(context),
            ctypes.c_char_p(driver.encode("utf-8")),
            ctypes.c_char_p(driver_info.encode("utf-8")),
            ctypes.c_char_p(src.encode("utf-8")),
            ctypes.c_char_p(json.encode("utf-8")),
        )
        self._parse_return_code(context, res, "schema")

    def read_changeset(self, context, changeset):
        b_string1 = changeset.encode("utf-8")

        reader_ptr = self._readChangeset(context, b_string1)
        if reader_ptr is None:
            raise GeoDiffLibError("Unable to open reader for: " + changeset)
        return ChangesetReader(self, context, reader_ptr)

    def create_wkb_from_gpkg_header(self, context, geometry):
        func = self.lib.GEODIFF_createWkbFromGpkgHeader
        func.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_char),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_char)),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        func.restype = ctypes.c_int

        out = ctypes.POINTER(ctypes.c_char)()
        out_size = ctypes.c_size_t(len(geometry))
        res = func(
            context,
            geometry,
            ctypes.c_size_t(len(geometry)),
            ctypes.byref(out),
            ctypes.byref(out_size),
        )
        self._parse_return_code(context, res, "create_wkb_from_gpkg_header")
        wkb = copy.deepcopy(out[: out_size.value])
        return wkb


class ChangesetReader(object):
    """Wrapper around GEODIFF_CR_* functions from C API"""

    def __init__(self, geodiff, context, reader_ptr):
        self.geodiff = geodiff
        self.reader_ptr = reader_ptr
        self.context = context

    def __del__(self):
        self.geodiff._CR_destroy(self.context, self.reader_ptr)

    def next_entry(self):
        ok = ctypes.c_bool()
        entry_ptr = self.geodiff._CR_nextEntry(
            self.context, self.reader_ptr, ctypes.byref(ok)
        )
        if not ok:
            raise GeoDiffLibError("Failed to read entry!")
        if entry_ptr is not None:
            return ChangesetEntry(self.geodiff, self.context, entry_ptr)
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
    """Wrapper around GEODIFF_CE_* functions from C API"""

    # constants as defined in ChangesetEntry::OperationType enum
    OP_INSERT = 18
    OP_UPDATE = 23
    OP_DELETE = 9

    def __init__(self, geodiff, context, entry_ptr):
        self.geodiff = geodiff
        self.entry_ptr = entry_ptr
        self.context = context

        self.operation = self.geodiff._CE_operation(self.context, self.entry_ptr)
        self.values_count = self.geodiff._CE_count(self.context, self.entry_ptr)

        if self.operation == self.OP_DELETE or self.operation == self.OP_UPDATE:
            self.old_values = []
            for i in range(self.values_count):
                v_ptr = self.geodiff._CE_old_value(self.context, self.entry_ptr, i)
                self.old_values.append(self._convert_value(v_ptr))

        if self.operation == self.OP_INSERT or self.operation == self.OP_UPDATE:
            self.new_values = []
            for i in range(self.values_count):
                v_ptr = self.geodiff._CE_new_value(self.context, self.entry_ptr, i)
                self.new_values.append(self._convert_value(v_ptr))

        table = self.geodiff._CE_table(self.context, entry_ptr)
        self.table = ChangesetTable(geodiff, self.context, table)

    def __del__(self):
        self.geodiff._CE_destroy(self.context, self.entry_ptr)

    def _convert_value(self, v_ptr):
        v_type = self.geodiff._V_type(self.context, v_ptr)
        if v_type == 0:
            v_val = UndefinedValue()
        elif v_type == 1:
            v_val = self.geodiff._V_get_int(self.context, v_ptr)
        elif v_type == 2:
            v_val = self.geodiff._V_get_double(self.context, v_ptr)
        elif v_type == 3 or v_type == 4:  # 3==text, 4==blob
            size = self.geodiff._V_get_data_size(self.context, v_ptr)
            buffer = ctypes.create_string_buffer(size)
            self.geodiff._V_get_data(self.context, v_ptr, buffer)
            v_val = buffer.raw
            if v_type == 3:
                v_val = v_val.decode("utf-8")
        elif v_type == 5:
            v_val = None
        else:
            raise GeoDiffLibError("unknown value type {}".format(v_type))
        self.geodiff._V_destroy(self.context, v_ptr)
        return v_val


class ChangesetTable(object):
    """Wrapper around GEODIFF_CT_* functions from C API"""

    def __init__(self, geodiff, context, table_ptr):
        self.geodiff = geodiff
        self.table_ptr = table_ptr
        self.context = context

        self.name = self.geodiff._CT_name(self.context, table_ptr).decode("utf-8")
        self.column_count = self.geodiff._CT_column_count(self.context, table_ptr)
        self.column_is_pkey = []
        for i in range(self.column_count):
            self.column_is_pkey.append(
                self.geodiff._CT_column_is_pkey(self.context, table_ptr, i)
            )


class UndefinedValue(object):
    """Marker that a value in changeset is undefined. This is different
    from NULL value (which is represented as None). Undefined values are
    used for example as values of columns in UPDATE operation that did
    not get modified."""

    def __repr__(self):
        return "<N/A>"
