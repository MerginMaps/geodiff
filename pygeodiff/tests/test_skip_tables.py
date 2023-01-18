# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2022 Alexander Bruy
    :license: MIT, see LICENSE for more details.
"""

from .testutils import *
import os
import shutil
import pygeodiff

class UnitTestsPythonSingleCommit(GeoDiffTests):
    def test_skip_create(self):
        base = geodiff_test_dir() + "/" + "skip_tables" + "/" + "base.gpkg"
        modified = geodiff_test_dir() + "/" + "skip_tables" + "/" + "modified_all.gpkg"
        modified_points = geodiff_test_dir() + "/" + "skip_tables" + "/" + "modified_points.gpkg"
        changeset = tmpdir() + "/py" + "test_skip_create" + "/" + "changeset_points.bin"
        changeset2 = tmpdir() + "/py" + "test_skip_create" + "/" + "changeset_points2.bin"
        changeset_inv = tmpdir() + "/py" + "test_skip_create" + "/" + "changeset_inv.bin"
        patched = tmpdir() + "/py" + "test_skip_create" + "/" + "patched_points.gpkg"

        create_dir("test_skip_create")

        # ignore lines table when creating changeset
        self.geodiff.set_tables_to_skip(["lines"])

        # create changeset
        self.geodiff.create_changeset(base, modified, changeset)
        check_nchanges(self.geodiff, changeset, 4)

        # apply changeset
        shutil.copyfile(base, patched)
        self.geodiff.apply_changeset(patched, changeset)

        # check that now it is same file
        self.geodiff.create_changeset(patched, modified, changeset2)
        check_nchanges(self.geodiff, changeset2, 0)

        # check we can create inverted changeset
        os.remove(changeset2)
        self.geodiff.invert_changeset(changeset, changeset_inv)
        self.geodiff.apply_changeset(patched, changeset_inv)
        self.geodiff.create_changeset_dr("sqlite", "", patched, "sqlite", "", base, changeset2)
        check_nchanges(self.geodiff, changeset2, 0 )

        self.geodiff.set_tables_to_skip([])

    def test_skip_apply(self):
        base = geodiff_test_dir() + "/" + "skip_tables" + "/" + "base.gpkg"
        modified = geodiff_test_dir() + "/" + "skip_tables" + "/" + "modified_all.gpkg"
        modified_points = geodiff_test_dir() + "/" + "skip_tables" + "/" + "modified_points.gpkg"
        changeset = tmpdir() + "/py" + "test_skip_apply" + "/" + "changeset_points.bin"
        changeset2 = tmpdir() + "/py" + "test_skip_apply" + "/" + "changeset_points2.bin"
        changeset_inv = tmpdir() + "/py" + "test_skip_apply" + "/" + "changeset_inv.bin"
        patched = tmpdir() + "/py" + "test_skip_apply" + "/" + "patched_points.gpkg"

        create_dir("test_skip_apply")

        # create changeset
        self.geodiff.create_changeset(base, modified, changeset)
        check_nchanges(self.geodiff, changeset, 6)

        # ignore lines table when creating changeset
        self.geodiff.set_tables_to_skip(["lines"])

        # apply changeset
        shutil.copyfile(base, patched)
        self.geodiff.apply_changeset(patched, changeset)

        # check that now it is same file
        self.geodiff.create_changeset(patched, modified, changeset2)
        check_nchanges(self.geodiff, changeset2, 0)

        # check we can create inverted changeset
        os.remove(changeset2)
        self.geodiff.invert_changeset(changeset, changeset_inv)
        self.geodiff.apply_changeset(patched, changeset_inv)
        self.geodiff.create_changeset_dr("sqlite", "", patched, "sqlite", "", base, changeset2)
        check_nchanges(self.geodiff, changeset2, 0 )

        self.geodiff.set_tables_to_skip([])
