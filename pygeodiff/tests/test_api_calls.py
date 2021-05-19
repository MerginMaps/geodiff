# -*- coding: utf-8 -*-
'''
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .testutils import *
import os
import shutil

class UnitTestsPythonApiCalls(GeoDiffTests):
    """ Some quick tests of various API calls just to make sure they are not broken """

    def test_api_calls(self):
        print("********************************************************")
        print("PYTHON: test API calls")

        outdir = tmpdir() + "/pyapi-calls"
        create_dir("api-calls")

        print("-- concat_changes")
        self.geodiff.concat_changes(
            [testdir()+'/concat/foo-insert-update-1.diff', testdir()+'/concat/foo-insert-update-2.diff'],
            outdir+'/concat.diff')

        print("-- make_copy")
        self.geodiff.make_copy(
            'sqlite', '',
            testdir()+'/base.gpkg',
            'sqlite', '',
            outdir+'/make-copy.gpkg')

        print("-- make_copy_sqlite")
        self.geodiff.make_copy_sqlite(
            testdir()+'/base.gpkg',
            outdir+'/make-copy-sqlite.gpkg')

        print("-- create_changeset_ex")
        self.geodiff.create_changeset_ex(
            'sqlite', '',
            testdir()+'/base.gpkg',
            testdir()+'/1_geopackage/modified_1_geom.gpkg',
            outdir+'/create-ex.diff')

        print("-- apply_changeset_ex")
        self.geodiff.make_copy_sqlite(
            testdir()+'/base.gpkg',
            outdir+'/apply-ex.gpkg')
        self.geodiff.apply_changeset_ex(
            'sqlite', '',
            outdir+'/apply-ex.gpkg',
            testdir()+'/1_geopackage/base-modified_1_geom.diff')

        print("-- create_rebased_changeset_ex")
        self.geodiff.create_changeset_ex(
            'sqlite', '',
            testdir()+'/base.gpkg',
            testdir()+'/2_inserts/inserted_1_B.gpkg',
            outdir+'/rebased-ex-base2their.diff')
        self.geodiff.create_rebased_changeset_ex(
            'sqlite', '',
            testdir()+'/base.gpkg',
            testdir()+'/2_inserts/base-inserted_1_A.diff',
            outdir+'/rebased-ex-base2their.diff',
            outdir+'/rebased-ex.diff',
            outdir+'/rebased-ex-conflicts.json')

        print("-- rebase_ex")
        self.geodiff.make_copy_sqlite(
            testdir()+'/2_inserts/inserted_1_B.gpkg',
            outdir+'/rebase-ex.gpkg')
        self.geodiff.rebase_ex(
            'sqlite', '',
            testdir()+'/base.gpkg',
            outdir+'/rebase-ex.gpkg',
            testdir()+'/2_inserts/base-inserted_1_A.diff',
            outdir+'/rebase-ex-conflicts.json')

        print("-- dump_data")
        self.geodiff.dump_data(
            'sqlite', '',
            testdir()+'/base.gpkg',
            outdir+'/dump-data.diff')

        print("-- schema")
        self.geodiff.schema(
            'sqlite', '',
            testdir()+'/base.gpkg',
            outdir+'/schema.json')
