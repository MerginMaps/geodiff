# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

import unittest
import os
import tempfile
import pygeodiff
import json
import shutil
import subprocess


class TestError(Exception):
    __test__ = False  # this is not a test class, this will make pytest to ignore it


REFDIF = os.path.dirname(os.path.realpath(__file__))


def geodiff_test_dir():
    return os.path.join(REFDIF, os.pardir, os.pardir, "geodiff", "tests", "testdata")


def tmpdir():
    return tempfile.gettempdir()


def create_dir(testname):
    if os.path.exists(tmpdir() + "/py" + testname):
        shutil.rmtree(tmpdir() + "/py" + testname)
    os.makedirs(tmpdir() + "/py" + testname)
    return tmpdir() + "/py" + testname


def check_nchanges(geodiff, changeset, expected_number_of_changes):
    # test has_changes
    has_changes = geodiff.has_changes(changeset)
    if expected_number_of_changes == 0 and has_changes:
        raise TestError("expected no changes")
    if expected_number_of_changes != 0 and not has_changes:
        raise TestError("expected changes")

    # test changes_count API
    nchanges = geodiff.changes_count(changeset)
    if nchanges != expected_number_of_changes:
        raise TestError(
            "expecting {} changes, found {}".format(
                expected_number_of_changes, nchanges
            )
        )


def is_valid_json(stream):
    try:
        json.loads(stream)
        print(stream)
    except json.decoder.JSONDecodeError as e:
        raise TestError("JSON:\n " + stream + "\n is not valid :\n" + str(e))


def _test_json(function, changeset, json, expect_success):
    try:
        function(changeset, json)
        if not expect_success:
            raise TestError("json generation succeeded, but should have failed")
    except:
        if expect_success:
            raise TestError("json generation failed")

    if expect_success and not os.path.exists(json):
        raise TestError("missing generated JSON file")

    if os.path.exists(json):
        with open(json, "r") as fin:
            data = fin.read()
            is_valid_json(data)


def test_json(geodiff, changeset, json, expect_success):
    print("check export to JSON summary")
    _test_json(geodiff.list_changes_summary, changeset, json, expect_success)

    print("check export to JSON ")
    _test_json(geodiff.list_changes, changeset, json, expect_success)


test_json.__test__ = (
    False  # this is not a standalone test, this will make pytest to ignore it
)


def compare_json(json_file, expected_json):
    print("comparing JSON to " + expected_json)
    if not os.path.exists(json_file):
        raise TestError("missing generated JSON file")

    with open(json_file, "r") as fin:
        json_generated = fin.read()

    with open(expected_json, "r") as fin:
        json_expected = fin.read()

    a = json.loads(json_generated)
    b = json.loads(json_expected)
    if not dict_diff(a, b):
        print("---- JSON GENERATED ----")
        print(json_generated)
        print("---- JSON EXPECTED ----")
        print(json_expected)
        raise TestError("JSON generated is different from expected")


def file_contains(text_file, expected_str):
    try:
        with open(text_file, "r") as file:
            content = file.read()
            if expected_str not in content:
                raise TestError(
                    "file " + text_file + " does not contain " + expected_str
                )
    except UnicodeDecodeError:
        raise TestError("file " + text_file + " is not text file")


def logger(level, rawString):
    msg = rawString.decode("utf-8")
    print("GEODIFFTEST: " + str(level) + " " + msg)


def dict_diff(a, b):
    for key in a.keys():
        if key not in b:
            return False

        if isinstance(a[key], dict):
            return find_diff(a[key], b[key])
        else:
            if a[key] != b[key]:
                return False
    return True


class GeoDiffTests(unittest.TestCase):
    def setUp(self):
        # load lib
        lib = os.environ.get("GEODIFFLIB", None)
        if lib is None:
            print(
                "missing GEODIFFLIB env variable, trying to use the geodiff from wheel"
            )
        self.geodiff = pygeodiff.GeoDiff(lib)
        self.geodiff.set_logger_callback(logger)
        self.geodiff.set_maximum_logger_level(pygeodiff.GeoDiff.LevelDebug)


class GeoDiffCliTests(unittest.TestCase):
    def setUp(self):
        # load lib
        self.cli = os.environ.get("GEODIFFCLI", None)
        if self.cli is None:
            raise TestError("missing GEODIFFCLI env variable with path to geodiff-cli")

    def run_command(self, args, check_in_output=None, expect_fail=False):
        assert self.cli is not None
        cmd = [self.cli] + args
        cmd_str = " ".join(cmd)
        output = ""
        try:
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as err:
            if expect_fail:
                return
            else:
                raise TestError(
                    "command "
                    + cmd_str
                    + " returned non-zero code "
                    + str(err.returncode)
                )

        if expect_fail:
            raise TestError(
                "command " + cmd_str + " returned zero code even we expected fail"
            )

        if check_in_output is not None:
            if str(check_in_output) not in str(output):
                raise TestError(
                    "expected output "
                    + check_in_output
                    + " not found in command "
                    + cmd_str
                )
