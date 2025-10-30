# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

from .testutils import (
    GeoDiffTests,
    TestError,
    create_dir,
    geodiff_test_dir,
    logger,
)


class UnitTestsPythonApiCalls(GeoDiffTests):
    """Some quick tests of various API calls just to make sure they are not broken"""

    def test_global_settigs(self):
        print("********************************************************")
        print("PYTHON: test setting logger to None and Back")
        self.geodiff.set_logger_callback(None)
        self.geodiff.set_logger_callback(logger)

    def test_api_calls(self):
        print("********************************************************")
        print("PYTHON: test API calls")

        outdir = create_dir("api-calls")

        print("-- driver_api")
        if len(self.geodiff.drivers()) < 1:
            raise TestError("no drivers registered")

        if not self.geodiff.driver_is_registered("sqlite"):
            raise TestError("sqlite driver not registered")

        print("-- concat_changes")
        self.geodiff.concat_changes(
            [
                geodiff_test_dir() + "/concat/foo-insert-update-1.diff",
                geodiff_test_dir() + "/concat/foo-insert-update-2.diff",
            ],
            outdir + "/concat.diff",
        )

        self.geodiff.concat_changes(
            [
                geodiff_test_dir() + "/concat/bar-insert.diff",
                geodiff_test_dir() + "/concat/bar-update.diff",
                geodiff_test_dir() + "/concat/bar-delete.diff",
            ],
            outdir + "/concat.diff",
        )

        # This is not a valid concat - you delete feature and then update (deleted feature) and
        # then insert it.  But it should not crash.
        # Ideally update is ignored (invalid step) and insert is applied
        # https://github.com/MerginMaps/geodiff/issues/174
        self.geodiff.concat_changes(
            [
                geodiff_test_dir() + "/concat/bar-delete.diff",
                geodiff_test_dir() + "/concat/bar-update.diff",
                geodiff_test_dir() + "/concat/bar-insert.diff",
            ],
            outdir + "/concat.diff",
        )

        print("-- make_copy")
        self.geodiff.make_copy(
            "sqlite",
            "",
            geodiff_test_dir() + "/base.gpkg",
            "sqlite",
            "",
            outdir + "/make-copy.gpkg",
        )

        print("-- make_copy_sqlite")
        self.geodiff.make_copy_sqlite(
            geodiff_test_dir() + "/base.gpkg", outdir + "/make-copy-sqlite.gpkg"
        )

        print("-- create_changeset_ex")
        self.geodiff.create_changeset_ex(
            "sqlite",
            "",
            geodiff_test_dir() + "/base.gpkg",
            geodiff_test_dir() + "/1_geopackage/modified_1_geom.gpkg",
            outdir + "/create-ex.diff",
        )

        print("-- apply_changeset_ex")
        self.geodiff.make_copy_sqlite(
            geodiff_test_dir() + "/base.gpkg", outdir + "/apply-ex.gpkg"
        )
        self.geodiff.apply_changeset_ex(
            "sqlite",
            "",
            outdir + "/apply-ex.gpkg",
            geodiff_test_dir() + "/1_geopackage/base-modified_1_geom.diff",
        )

        print("-- create_rebased_changeset_ex")
        self.geodiff.create_changeset_ex(
            "sqlite",
            "",
            geodiff_test_dir() + "/base.gpkg",
            geodiff_test_dir() + "/2_inserts/inserted_1_B.gpkg",
            outdir + "/rebased-ex-base2their.diff",
        )
        self.geodiff.create_rebased_changeset_ex(
            "sqlite",
            "",
            geodiff_test_dir() + "/base.gpkg",
            geodiff_test_dir() + "/2_inserts/base-inserted_1_A.diff",
            outdir + "/rebased-ex-base2their.diff",
            outdir + "/rebased-ex.diff",
            outdir + "/rebased-ex-conflicts.json",
        )

        print("-- rebase_ex")
        self.geodiff.make_copy_sqlite(
            geodiff_test_dir() + "/2_inserts/inserted_1_B.gpkg",
            outdir + "/rebase-ex.gpkg",
        )
        self.geodiff.rebase_ex(
            "sqlite",
            "",
            geodiff_test_dir() + "/base.gpkg",
            outdir + "/rebase-ex.gpkg",
            geodiff_test_dir() + "/2_inserts/base-inserted_1_A.diff",
            outdir + "/rebase-ex-conflicts.json",
        )

        print("-- dump_data")
        self.geodiff.dump_data(
            "sqlite", "", geodiff_test_dir() + "/base.gpkg", outdir + "/dump-data.diff"
        )

        print("-- schema")
        self.geodiff.schema(
            "sqlite", "", geodiff_test_dir() + "/base.gpkg", outdir + "/schema.json"
        )
