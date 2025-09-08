# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

import os
import shutil

from .testutils import (
    GeoDiffTests,
    TestError,
    check_nchanges,
    create_dir,
    geodiff_test_dir,
    tmpdir,
)


class UnitTestsPythonConcurrentCommits(GeoDiffTests):
    def test_2_inserts(self):
        print("********************************************************")
        print(
            "PYTHON: geopackage 2 concurent modifications (base) -> (A) and (base) -> (B)"
        )
        testname = "2_inserts"
        create_dir(testname)

        base = geodiff_test_dir() + "/base.gpkg"
        modifiedA = geodiff_test_dir() + "/" + testname + "/" + "inserted_1_A.gpkg"
        modifiedB = geodiff_test_dir() + "/" + testname + "/" + "inserted_1_B.gpkg"
        changesetbaseA = tmpdir() + "/py" + testname + "/" + "changeset_base_to_A.bin"
        changesetAB = tmpdir() + "/py" + testname + "/" + "changeset_A_to_B.bin"
        conflictAB = tmpdir() + "/py" + testname + "/" + "conflict_A_to_B.json"
        changesetBbase = tmpdir() + "/py" + testname + "/" + "changeset_B_to_base.bin"
        patchedAB = tmpdir() + "/py" + testname + "/" + "patched_AB.gpkg"
        patchedAB2 = tmpdir() + "/py" + testname + "/" + "patched_AB_2.gpkg"
        changesetAB2 = tmpdir() + "/py" + testname + "/" + "changeset_AB2.bin"
        conflictAB2 = tmpdir() + "/py" + testname + "/" + "conflict_A_to_B2.json"

        print("create changeset base to A")
        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        check_nchanges(self.geodiff, changesetbaseA, 1)

        print("create changeset A to B")
        self.geodiff.create_rebased_changeset(
            base, modifiedB, changesetbaseA, changesetAB, conflictAB
        )
        check_nchanges(self.geodiff, changesetAB, 1)
        if os.path.exists(conflictAB):
            raise TestError("expected no conflicts")

        print("apply changeset to A to get AB")
        shutil.copyfile(modifiedA, patchedAB)
        self.geodiff.apply_changeset(patchedAB, changesetAB)

        print("check that then new data has both features\n")
        self.geodiff.create_changeset(base, patchedAB, changesetBbase)
        check_nchanges(self.geodiff, changesetBbase, 2)

        print("check direct rebase")
        shutil.copyfile(modifiedB, patchedAB2)
        self.geodiff.rebase(base, modifiedA, patchedAB2, conflictAB2)
        if os.path.exists(conflictAB2):
            raise TestError("expected no conflicts")

        self.geodiff.create_changeset(base, patchedAB2, changesetAB2)
        check_nchanges(self.geodiff, changesetAB2, 2)
