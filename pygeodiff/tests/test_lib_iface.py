# -*- coding: utf-8 -*-
"""
:copyright: (c) 2026 David Koňařík
:license: MIT, see LICENSE for more details.
"""

import gc
import unittest

from pygeodiff import GeoDiff, GeoDiffLibError, shutdown


class UnitTestsLibLazyloading(unittest.TestCase):
    def tearDown(self):
        gc.collect()  # Clean up after failed tests

    def test_load(self):
        geodiff = GeoDiff()
        geodiff.version()
        self.assertIsNotNone(GeoDiff._clib_weakref)
        self.assertIsNotNone(GeoDiff._clib_weakref())

    def test_shutdown(self):
        geodiff = GeoDiff()
        geodiff.shutdown()
        self.assertIsNone(geodiff.clib)
        self.assertIsNone(GeoDiff._clib_weakref())

    def test_auto_shutdown(self):
        geodiff = GeoDiff()
        geodiff.version()
        self.assertIsNotNone(GeoDiff._clib_weakref)
        self.assertIsNotNone(GeoDiff._clib_weakref())
        del geodiff
        self.assertIsNone(GeoDiff._clib_weakref())

    def test_multiple(self):
        geodiff1 = GeoDiff()
        geodiff1.version()
        self.assertIsNotNone(GeoDiff._clib_weakref)
        self.assertIsNotNone(GeoDiff._clib_weakref())
        geodiff2 = GeoDiff()
        geodiff2.version()
        del geodiff1
        self.assertIsNotNone(GeoDiff._clib_weakref())
        del geodiff2
        self.assertIsNone(GeoDiff._clib_weakref())

    def test_global_shutdown(self):
        geodiff = GeoDiff()
        geodiff.version()
        self.assertIsNotNone(GeoDiff._clib_weakref)
        self.assertIsNotNone(GeoDiff._clib_weakref())
        shutdown()
        self.assertIsNone(GeoDiff._clib_weakref)
        self.assertRaises(AttributeError, geodiff.version)

    def test_logger_callback(self):
        geodiff = GeoDiff()
        loglines = []
        geodiff.set_logger_callback(lambda p, l: loglines.append((p, l)))
        geodiff.set_maximum_logger_level(4)
        # Call function with bad params to create log line
        try:
            geodiff.clib.changes_count(geodiff.context, "")
        except GeoDiffLibError:
            pass
        self.assertGreater(len(loglines), 0)
