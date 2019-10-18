# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *
import os
import shutil

class UnitTestsPythonConcurrentCommits(GeoDiffTests):
    def test_2_inserts(self):
        print("********************************************************")
        print("PYTHON: geopackage 2 concurent modifications (base) -> (A) and (base) -> (B)")
        testname = "2_inserts"
        if os.path.exists(tmpdir() + "/py" + testname):
            shutil.rmtree(tmpdir() + "/py" + testname)
        os.makedirs(tmpdir() + "/py" + testname)

        base = testdir() + "/base.gpkg"
        modifiedA = testdir() + "/" + testname + "/" + "inserted_1_A.gpkg"
        modifiedB = testdir() + "/" + testname + "/" + "inserted_1_B.gpkg"
        changesetbaseA = tmpdir() + "/py" + testname + "/" + "changeset_base_to_A.bin"
        changesetAB = tmpdir() + "/py" + testname + "/" + "changeset_A_to_B.bin"
        changesetBbase = tmpdir() + "/py" + testname + "/" + "changeset_B_to_base.bin"
        patchedAB = tmpdir() + "/py" + testname + "/" + "patched_AB.gpkg"
        patchedAB2 = tmpdir() + "/py" + testname + "/" + "patched_AB_2.gpkg"
        changesetAB2 = tmpdir() + "/py" + testname + "/" + "changeset_AB2.bin"

        print("create changeset base to A")
        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        check_nchanges(self.geodiff, changesetbaseA,  2)

        print("create changeset A to B")
        self.geodiff.create_rebased_changeset(base, modifiedB, changesetbaseA, changesetAB)
        check_nchanges(self.geodiff, changesetAB, 2)

        print("apply changeset to A to get AB")
        shutil.copyfile(modifiedA, patchedAB)
        self.geodiff.apply_changeset( patchedAB, changesetAB)

        print("check that then new data has both features\n")
        self.geodiff.create_changeset(base, patchedAB, changesetBbase)
        check_nchanges(self.geodiff, changesetBbase, 3)

        print("check direct rebase")
        shutil.copyfile(modifiedB, patchedAB2)
        self.geodiff.rebase(base, modifiedA, patchedAB2)
        self.geodiff.create_changeset(base, patchedAB2, changesetAB2)
        check_nchanges(self.geodiff, changesetAB2,  3)
