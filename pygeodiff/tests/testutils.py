# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

import unittest
import os
import tempfile
import pygeodiff
import json

class TestError(Exception):
  pass


REFDIF = os.path.dirname(os.path.realpath(__file__))


def testdir():
    return os.path.join(REFDIF, os.pardir, os.pardir, "geodiff", "tests", "testdata")


def tmpdir():
  return tempfile.gettempdir()


def check_nchanges(geodiff, changeset, expected_number_of_changes ):
  nchanges = geodiff.list_changes( changeset )
  if nchanges != expected_number_of_changes:
    raise TestError( "expecting {} changes, found {}".format(expected_number_of_changes, nchanges))


def is_valid_json(stream):
    try:
        json_object = json.loads(stream)
    except ValueError as e:
        return False
    return True


def test_json(geodiff, base, changeset, json, expect_success ):
    print("check export to JSON ")
    try:
        geodiff.list_changes_json(base, changeset, json)
        if not expect_success:
            raise TestError("json generation succeeded, but should have failed")
    except:
        if expect_success:
            raise TestError("json generation failed")

    if expect_success and not os.path.exists(json):
        raise TestError("missing generated JSON file")

    if os.path.exists(json):
        with open(json, 'r') as fin:
            data = fin.read()
            print(data)
            # check that json is valid
            if not is_valid_json(data):
                raise TestError(json + " is not valid JSON file:\n " + data)


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


