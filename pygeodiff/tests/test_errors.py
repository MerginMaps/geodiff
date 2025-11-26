# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2020 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

import shutil
import pygeodiff
from .testutils import (
    GeoDiffTests,
    TestError,
    create_dir,
    geodiff_test_dir,
    tmpdir,
)


class UnitTestsPythonErrors(GeoDiffTests):
    def test_unsupported_change_error(self):
        create_dir("unsupported_change")

        base = geodiff_test_dir() + "/base.gpkg"
        modified = geodiff_test_dir() + "/modified_scheme/added_attribute.gpkg"
        changeset = tmpdir() + "/pyunsupported_change/changeset.bin"

        try:
            self.geodiff.create_changeset(base, modified, changeset)
            raise TestError("expected GeoDiffLibError")
        except pygeodiff.GeoDiffLibError:
            pass

    def test_conflict_error(self):
        create_dir("conflict_error")
        base = geodiff_test_dir() + "/base.gpkg"
        modifiedA = geodiff_test_dir() + "/2_updates/updated_A.gpkg"
        modifiedB = geodiff_test_dir() + "/2_updates/updated_B.gpkg"
        base2 = tmpdir() + "/pyconflict_error/base2.gpkg"
        changesetbaseA = tmpdir() + "/pyconflict_error/changesetbaseA.bin"
        shutil.copyfile(modifiedB, base2)

        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        try:
            self.geodiff.apply_changeset(base2, changesetbaseA)
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
