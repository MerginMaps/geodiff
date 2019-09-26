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
  # test has_changes
  has_changes = geodiff.has_changes(changeset)
  if expected_number_of_changes == 0 and has_changes:
      raise TestError("expected no changes")
  if expected_number_of_changes != 0 and not has_changes:
      raise TestError("expected changes")

  # test changes_count API
  nchanges = geodiff.changes_count( changeset )
  if nchanges != expected_number_of_changes:
    raise TestError( "expecting {} changes, found {}".format(expected_number_of_changes, nchanges))


def is_valid_json(stream):
    try:
        json.loads(stream)
        print(stream)
    except json.decoder.JSONDecodeError as e:
        raise TestError("JSON:\n " + stream + "\n is not valid :\n" + str(e))


def test_json(geodiff, changeset, json, expect_success ):
    print("check export to JSON ")
    try:
        geodiff.list_changes(changeset, json)
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
            is_valid_json(data)


def compare_json(json, expected_json):
    print ("comparing JSON to " + expected_json)
    if not os.path.exists(json):
        raise TestError("missing generated JSON file")

    with open(json, 'r') as fin:
        json_generated = fin.read()

    with open(expected_json, 'r') as fin:
        json_expected = fin.read()

    if json_generated.strip() != json_expected.strip():
        raise TestError("JSON generated is different from expected")


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


