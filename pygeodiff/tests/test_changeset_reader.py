# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2021 Martin Dobias
    :license: MIT, see LICENSE for more details.
"""

import os
from pygeodiff import (
    ChangesetEntry,
    ChangesetReader,
    GeoDiffLibError,
    UndefinedValue,
)
from .testutils import GeoDiffTests, geodiff_test_dir


class UnitTestsChangesetReader(GeoDiffTests):
    def test_invalid_file(self):
        changeset = os.path.join(geodiff_test_dir(), "missing-file.diff")
        with self.assertRaises(GeoDiffLibError):
            ch_reader = self.geodiff.read_changeset(changeset)

        # existing file, but wrong format... reader will be created fine
        # (there's no header to verify it's the right file type)
        changeset = os.path.join(geodiff_test_dir(), "base.gpkg")
        ch_reader = self.geodiff.read_changeset(changeset)
        with self.assertRaises(GeoDiffLibError):
            ch_reader.next_entry()

    def test_basic(self):
        changeset = os.path.join(
            geodiff_test_dir(), "1_geopackage", "base-modified_1_geom.diff"
        )
        ch_reader = self.geodiff.read_changeset(changeset)
        self.assertIsInstance(ch_reader, ChangesetReader)
        entry = ch_reader.next_entry()
        self.assertEqual(entry.operation, ChangesetEntry.OP_UPDATE)
        entry = ch_reader.next_entry()
        self.assertEqual(entry.operation, ChangesetEntry.OP_UPDATE)
        entry = ch_reader.next_entry()
        self.assertEqual(entry, None)

    def test_iterator(self):
        changeset = os.path.join(
            geodiff_test_dir(), "1_geopackage", "base-modified_1_geom.diff"
        )
        i = 0
        for entry in self.geodiff.read_changeset(changeset):
            self.assertEqual(entry.operation, ChangesetEntry.OP_UPDATE)
            if i == 0:  # change in gpkg_contents
                self.assertEqual(entry.table.name, "gpkg_contents")
                self.assertEqual(entry.old_values[0], "simple")
            else:  # change in 'simple' table
                self.assertEqual(entry.table.name, "simple")
                self.assertEqual(entry.table.column_is_pkey[0], True)
                self.assertEqual(entry.table.column_is_pkey[1], False)
                self.assertEqual(entry.old_values[0], 1)
                self.assertIsInstance(entry.new_values[0], UndefinedValue)
                self.assertIsInstance(entry.old_values[1], bytes)
                self.assertIsInstance(entry.new_values[1], bytes)
                self.assertEqual(len(entry.old_values[1]), len(entry.new_values[1]))
            i += 1
        self.assertEqual(i, 2)

    def test_insert(self):
        changeset = os.path.join(
            geodiff_test_dir(), "2_inserts", "base-inserted_1_A.diff"
        )
        i = 0
        for entry in self.geodiff.read_changeset(changeset):
            self.assertEqual(entry.table.name, "simple")
            self.assertEqual(entry.new_values[0], 4)
            self.assertEqual(entry.new_values[2], "my new point A")
            with self.assertRaises(AttributeError):
                print(
                    entry.old_values
                )  # with INSERT the "old_values" attribute is not set
            i += 1
        self.assertEqual(i, 1)

    def test_delete(self):
        changeset = os.path.join(geodiff_test_dir(), "2_deletes", "base-deleted_A.diff")
        i = 0
        for entry in self.geodiff.read_changeset(changeset):
            self.assertEqual(entry.table.name, "simple")
            self.assertEqual(entry.old_values[0], 2)
            self.assertEqual(entry.old_values[2], "feature2")
            with self.assertRaises(AttributeError):
                print(
                    entry.new_values
                )  # with DELETE the "new_values" attribute is not set
            i += 1
        self.assertEqual(i, 1)
