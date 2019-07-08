# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *


def basetest(
        geodiff,
         testname, basename, modifiedname, expected_changes ):
  print( "********************************************************" )
  print( testname )

  base = testdir() + "/" + basename
  modified = testdir() + "/" + modifiedname
  changeset = tmpdir() + "/changeset_" + basename + ".bin"
  changeset2 = tmpdir() + "/changeset_after_apply_" + basename + ".bin"
  patched = tmpdir() + "/patched_" + modifiedname

  print( "diff" )
  geodiff.create_changeset( base, modified, changeset )

  check_nchanges( geodiff, changeset, expected_changes )

  print( "apply" )
  geodiff.apply_changeset( base, patched, changeset )

  print( "filesizes:" )
  print( "base:{}".format( os.stat( base ).st_size ))
  print("modified:{}".format(os.stat(base).st_size))
  print("patched:{}".format(os.stat(base).st_size))
  print("changeset:{}".format(os.stat(base).st_size))

  print( "check that now it is same file\n" )
  geodiff.create_changeset( patched, modified, changeset2 )
  check_nchanges( geodiff, changeset2, 0 )


class UnitTestsSingleCommit(GeoDiffTests):
    def test_sqlite_no_gis(self):
        basetest(
            self.geodiff,
             "sqlite 2 updated 1 added 1 deleted ",
             "base.sqlite",
             "modified_base.sqlite",
             4)

    def test_geopackage(self):
        basetest(self.geodiff,
                 "geopackage 1 updated geometry",
             "base.gpkg",
             "modified_1_geom.gpkg",
             3)
