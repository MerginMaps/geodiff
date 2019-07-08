# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *


class UnitTestsConsurrentCommits(GeoDiffTests):
    def test_2_inserts(self):
        print("********************************************************")
        print("geopackage 2 concurent modifications (base) -> (A) and (base) -> (B)")

        base = testdir() + "/base.gpkg"
        modifiedA = testdir() + "/" + "inserted_1_A.gpkg"
        modifiedB = testdir() + "/" + "inserted_1_B.gpkg"
        changesetbaseA = tmpdir() + "/changeset_base_to_A.bin"
        changesetAB = tmpdir() + "/changeset_A_to_B.bin"
        changesetBbase = tmpdir() + "/changeset_B_to_base.bin"
        patchedAB = tmpdir() + "/patched_AB.gpkg"

        print("create changeset base to A")
        self.geodiff.create_changeset(base, modifiedA, changesetbaseA)
        check_nchanges(self.geodiff, changesetbaseA,  2 * 1 + 3) # 3 updates in total and 2 inserts for each feature

        print("create changeset A to B")
        self.geodiff.create_rebased_changeset(base, modifiedB, changesetbaseA, changesetAB)
        check_nchanges(self.geodiff, changesetAB,  2 * 1 + 3) # 3 updates in total and 2 inserts for each feature

        print("apply changeset to A to get AB")
        self.geodiff.apply_changeset(modifiedA, patchedAB, changesetAB)

        print("check that then new data has both features\n")
        self.geodiff.create_changeset(base, patchedAB, changesetBbase)
        check_nchanges(self.geodiff, changesetBbase, 2 * 2 + 3)
