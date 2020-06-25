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
        create_dir("unsupported_change")

        base = testdir() + "/base.gpkg"
        modified = testdir() + "/modified_scheme/added_attribute.gpkg"
        changeset = tmpdir() + "/pyunsupported_change/changeset.bin"

        try:
            self.geodiff.create_changeset(base, modified, changeset)
            raise TestError("expected GeoDiffLibError")
        except pygeodiff.GeoDiffLibError:
            pass

    def test_conflict_error(self):
        create_dir("conflict_error")
        base = testdir() + "/base.gpkg"
        modifiedA = testdir() + "/2_updates/updated_A.gpkg"
        modifiedB = testdir() + "/2_updates/updated_B.gpkg"
        base2 = tmpdir() + "/pyconflict_error/base2.gpkg"
        changesetbaseA = tmpdir() + "/pyconflict_error/changesetbaseA.bin"
        shutil.copyfile(modifiedB, base2)

        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        try:
            self.geodiff.apply_changeset( base2, changesetbaseA )
            raise TestError("expected GeoDiffLibError")
        except pygeodiff.GeoDiffLibError:
            pass

    def test_not_implemented(self):
        create_dir("not_implemented")
        base = testdir() + "/base_fk.gpkg"

        modifiedA = testdir() + "/fk_2_updates/modified_fk_A.gpkg"
        modifiedB = testdir() + "/fk_2_updates/modified_fk_B.gpkg"

        base2 = tmpdir() + "/pynot_implemented/base2.gpkg"
        changesetbaseA = tmpdir() + "/pynot_implemented/changesetbaseA.bin"
        changesetAB = tmpdir() + "/pynot_implemented/changesetAB.bin"
        conflict = tmpdir() + "/pynot_implemented/conflict.bin"

        shutil.copyfile(base, base2)
        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)

        try:
            self.geodiff.create_rebased_changeset(base2, modifiedB, changesetbaseA, changesetAB, conflict)
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
