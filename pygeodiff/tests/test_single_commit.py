# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *
import os
import shutil
import pygeodiff

def basetest(
        geodiff,
        testname,
        basename,
        modifiedname,
        expected_changes,
        expected_json_success,
        expected_json=None):
  print( "********************************************************" )
  print( "PYTHON: " + testname )

  if os.path.exists(tmpdir() + "/py" + testname):
      shutil.rmtree(tmpdir() + "/py" + testname)
  os.makedirs(tmpdir() + "/py" + testname)

  base = testdir() + "/" + basename
  modified = testdir() + "/" + testname + "/" + modifiedname
  changeset = tmpdir() + "/py" + testname + "/" + "changeset_" + basename + ".bin"
  changeset2 = tmpdir() + "/py" + testname + "/" + "changeset_after_apply_" + basename + ".bin"
  changeset_inv = tmpdir() + "/py" + testname + "/" + "changeset_inv" + basename + ".bin"
  patched = tmpdir() + "/py" + testname + "/" + "patched_" + modifiedname
  json = tmpdir() + "/py" + testname + "/" + basename + ".json"

  print( "diff" )
  geodiff.create_changeset( base, modified, changeset )
  check_nchanges( geodiff, changeset, expected_changes )

  print( "apply" )
  shutil.copyfile(base, patched)
  geodiff.apply_changeset( patched, changeset )

  print( "check that now it is same file\n" )
  geodiff.create_changeset( patched, modified, changeset2 )
  check_nchanges( geodiff, changeset2, 0 )

  print( "check we can create inverted changeset")
  os.remove(changeset2)
  geodiff.invert_changeset(changeset, changeset_inv)
  geodiff.apply_changeset(patched, changeset_inv)
  geodiff.create_changeset( patched, base, changeset2 )
  check_nchanges( geodiff, changeset2, 0 )

  #json
  test_json(geodiff, changeset, json, expected_json_success)

  if expected_json:
      compare_json(json, expected_json)


class UnitTestsPythonSingleCommit(GeoDiffTests):
    def test_sqlite_no_gis(self):
        basetest(
            self.geodiff,
             "pure_sqlite",
             "base.sqlite",
             "modified_base.sqlite",
             4,
             False)

    def test_geopackage(self):
        basetest(self.geodiff,
             "1_geopackage",
             "base.gpkg",
             "modified_1_geom.gpkg",
             2,
             True)

    def test_complex_geopackage(self):
        basetest(self.geodiff,
             "complex",
             "base.gpkg",
             "complex1.gpkg",
             7,
             True,
             expected_json=testdir() + "/complex/complex1.json")

    def test_error_input(self):
        print("********************************************************")
        print("PYTHON: " + "error input")
        try:
            self.geodiff.create_changeset("aaaa", "aaaa", "aaaa")
            raise TestError("expected failure")
        except pygeodiff.GeoDiffLibError:
            # OK
            pass
