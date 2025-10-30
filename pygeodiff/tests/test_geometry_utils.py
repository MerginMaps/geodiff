# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2022 Alexander Bruy
    :license: MIT, see LICENSE for more details.
"""

from .testutils import GeoDiffTests


class UnitTestsGeometryUtils(GeoDiffTests):
    def test_create_wkb_from_gpkg_header(self):
        gpkg_wkb = b"GP\x00\x01\x11\x0f\x00\x00\x01\x01\x00\x00\x00\x9e\xe8Z\x89\xa1\xd6MAK\xca\x04\xb9\x873WA"  # noqa
        expected_wkb = (
            b"\x01\x01\x00\x00\x00\x9e\xe8Z\x89\xa1\xd6MAK\xca\x04\xb9\x873WA"
        )
        wkb = self.geodiff.create_wkb_from_gpkg_header(gpkg_wkb)
        self.assertEqual(wkb, expected_wkb)
