# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

from .testutils import *
import os
import shutil
import pygeodiff


class UnitTestsPythonCLibrary(GeoDiffTests):
    """Some test for initialization and unload of geodiff c-library"""

    def test_clib_load(self):
        print("********************************************************")
        print("PYTHON: test C-Library loading and shutdown")

        if self.geodiff.clib() is None:
            raise TestError("geodiff is not initialized")

        print("-- another pygeodiff")

        geodiff2 = pygeodiff.GeoDiff(clib_libname())
        if geodiff2.clib() is None:
            raise TestError("geodiff2 is not initialized")

        if self.geodiff.clib() != geodiff2.clib():
            raise TestError("geodiff2 clib is not the same as self.geodiff")

        print("-- shutdown")
        pygeodiff.shutdown()

        if self.geodiff.clib() is not None:
            raise TestError("geodiff is initialized, but should have been shut down")

        if geodiff2.clib() is not None:
            raise TestError("geodiff2 is initialized, but should have been shut down")
