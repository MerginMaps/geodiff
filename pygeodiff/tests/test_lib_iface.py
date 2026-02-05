# -*- coding: utf-8 -*-
"""
:copyright: (c) 2026 David Koňařík
:license: MIT, see LICENSE for more details.
"""

import unittest

from pygeodiff import GeoDiff


class UnitTestsLibLazyloading(unittest.TestCase):
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
