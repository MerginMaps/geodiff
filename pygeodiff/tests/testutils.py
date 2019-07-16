# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

import unittest
import os
import tempfile
import pygeodiff


class TestError(Exception):
  pass


REFDIF = os.path.dirname(os.path.realpath(__file__))


def testdir():
    return os.path.join(REFDIF, os.pardir, os.pardir, "geodiff", "tests", "testdata")


def tmpdir():
  return tempfile.gettempdir()


def check_nchanges( geodiff, changeset, expected_number_of_changes ):
  nchanges = geodiff.list_changes( changeset )
  if nchanges != expected_number_of_changes:
    raise TestError( "expecting {} changes, found {}".format(expected_number_of_changes, nchanges))


class GeoDiffTests(unittest.TestCase):
    def setUp(self):
        # set env
        os.environ["GEODIFF_LOGGER_LEVEL"] = "4"
        # load lib
        lib = os.environ.get("GEODIFFLIB", None)
        if lib is None:
            raise TestError("missing GEODIFFLIB env variable")
        if not os.path.exists(lib):
            raise TestError("lib {} is missing ".format(lib))
        self.geodiff = pygeodiff.GeoDiff(lib)


