# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *
import os
import shutil

def basetest(
        geodiff,
        testname,
        basename,
        modifiedname,
        expected_changes ):
  print( "********************************************************" )
  print( "PYTHON: " + testname )

  if os.path.exists(tmpdir() + "/py" + testname):
      shutil.rmtree(tmpdir() + "/py" + testname)
  os.makedirs(tmpdir() + "/py" + testname)

  base = testdir() + "/" + basename
  modified = testdir() + "/" + testname + "/" + modifiedname
  changeset = tmpdir() + "/py" + testname + "/" + "changeset_" + basename + ".bin"
  changeset2 = tmpdir() + "/py" + testname + "/" + "changeset_after_apply_" + basename + ".bin"
  patched = tmpdir() + "/py" + testname + "/" + "patched_" + modifiedname
  json = tmpdir() + "/py" + testname + "/" + basename + ".json"

  print( "diff" )
  geodiff.create_changeset( base, modified, changeset )
  check_nchanges( geodiff, changeset, expected_changes )

  print( "apply" )
  geodiff.apply_changeset( base, patched, changeset )

  print( "check that now it is same file\n" )
  geodiff.create_changeset( patched, modified, changeset2 )
  check_nchanges( geodiff, changeset2, 0 )

  print( "check export to JSON ")
  try:
    geodiff.list_changes_json(base , changeset, json)
  except:
      return

  if os.path.exists(json):
      with open(json, 'r') as fin:
          print(fin.read())
  else:
      print("no JSON output")


class UnitTestsPythonSingleCommit(GeoDiffTests):
    def test_sqlite_no_gis(self):
        basetest(
            self.geodiff,
             "pure_sqlite",
             "base.sqlite",
             "modified_base.sqlite",
             4)

    def test_geopackage(self):
        basetest(self.geodiff,
             "1_geopackage",
             "base.gpkg",
             "modified_1_geom.gpkg",
             2)
