# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2020 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *
import os
import shutil
import pygeodiff

class UnitTestsPythonErrors(GeoDiffTests):
    def test_unsupported_change_error(self):
        base = testdir() + "/base.gpkg"
        modified = testdir() + "/modified_scheme/added_attribute.gpkg"
        changeset = tmpdir() + "/unsupported_change/changeset.bin"

        try:
            self.geodiff.create_changeset(base, modified, changeset)
            raise TestError("expected GeoDiffLibUnsupportedChangeError")
        except pygeodiff.GeoDiffLibUnsupportedChangeError:
            pass

    def test_conflict_error(self):
        base = testdir() + "/base_fk.gpkg"
        modifiedA = testdir() + "/fk_2_updates/modified_fk_A.gpkg"
        modifiedB = testdir() + "/fk_2_updates/modified_fk_B.gpkg"
        changesetbaseA = tmpdir() + "/conflict_error/changesetbaseA.bin"
        changesetAB = tmpdir() + "/conflict_error/changesetAB.bin"
        conflict = tmpdir() + "/conflict_error/conflict.bin"

        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        try:
            self.geodiff.create_rebased_changeset(base, modifiedB, changesetbaseA, changesetAB, conflict)
            raise TestError("expected GeoDiffLibError")
        except pygeodiff.GeoDiffLibError:
            pass

    def test_not_implemented(self):
        base = testdir() + "/base.gpkg"
        modifiedA = testdir() + "/2_updates/updated_A.gpkg"
        modifiedB = testdir() + "/2_updates/updated_B.gpkg"
        modifiedC = testdir() + "/2_deletes/deleted_A.gpkg"
        changesetbaseA = tmpdir() + "/not_implemented/changesetbaseA.bin"
        changesetAB = tmpdir() + "/not_implemented/changesetAB.bin"
        conflict = tmpdir() + "/not_implemented/conflict.bin"

        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        try:
            self.geodiff.create_rebased_changeset(modifiedC, modifiedB, changesetbaseA, changesetAB, conflict)
            raise TestError("expected GeoDiffLibError")
        except pygeodiff.GeoDiffLibError:
            pass

    def test_error_input(self):
        print("********************************************************")
        print("PYTHON: " + "error input")
        try:
            self.geodiff.create_changeset("aaaa", "aaaa", "aaaa")
            raise TestError("expected failure")
        except pygeodiff.GeoDiffLibError:
            # OK
            pass
