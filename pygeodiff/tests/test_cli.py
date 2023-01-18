# -*- coding: utf-8 -*-
"""
    :copyright: (c) 2023 Peter Petrik
    :license: MIT, see LICENSE for more details.
"""

from .testutils import *
import os
import shutil

class UnitTestsCliCalls(GeoDiffCliTests):
    """ Some quick tests of various CLI commands just to make sure they are not broken """

    def test_cli_calls(self):
        print("********************************************************")
        print("PYTHON: test API calls")

        outdir = create_dir("cli-calls")

        print("-- invalid")
        self.run_command([], expect_fail=True )
        self.run_command(["badcommand"], expect_fail=True )
        
        print("-- dump" )
        self.run_command(["dump"], expect_fail=True )
        self.run_command(["dump", geodiff_test_dir()+'/base.gpkg'], expect_fail=True )
        self.run_command(["dump", geodiff_test_dir()+'/base.gpkg', outdir + "/dump2.diff", "extra_arg" ], expect_fail=True )
        self.run_command(["dump", geodiff_test_dir()+'/base.gpkg', outdir + "/dump.diff"] )
        
        print("-- as-json" )
        self.run_command(["as-json"], expect_fail=True )
        self.run_command(["as-json", "arg1", "extra_arg"], expect_fail=True )
        self.run_command(["as-json", outdir + "/dump.diff"], check_in_output="feature2" )
        self.run_command(["as-json", outdir + "/dump.diff", outdir + "/dump.json"] )
        file_contains( outdir + "/dump.json", "feature3")
        
        print("-- as-summary" )
        self.run_command(["as-summary"], expect_fail=True )
        self.run_command(["as-summary", "arg1", "extra_arg"], expect_fail=True )
        self.run_command(["as-summary", outdir + "/dump.diff"], check_in_output="geodiff_summary" )
        self.run_command(["as-summary", outdir + "/dump.diff", outdir + "/dump.json"] )
        file_contains( outdir + "/dump.json", "geodiff_summary")

        print("-- diff" )
        self.run_command(["diff"], expect_fail=True )
        self.run_command(["diff", '--driver1'], expect_fail=True )
        self.run_command(["diff", '--skip-tables'], expect_fail=True )
        self.run_command(["diff", geodiff_test_dir()+'/non-existent.gpkg'], expect_fail=True )
        self.run_command(["diff", geodiff_test_dir()+'/non-existent.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], expect_fail=True )
        self.run_command(["diff", geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg', outdir + "/diff.diff", 'extra_arg'], expect_fail=True )
        self.run_command(["diff", '--json', '--summary', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], expect_fail=True )
        self.run_command(["diff", '--driver1', 'sqlite', '\'\'', '--driver2', 'sqlite', '\'\'', '--summary', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff_summary", expect_fail=True )
        self.run_command(["diff", '--driver1', 'sqlite', '--driver2', 'sqlite', '\'\'', '--summary', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff_summary", expect_fail=True )
        self.run_command(["diff", '--json', '--skip-tables', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff", expect_fail=True )
                
        self.run_command(["diff", geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'] )
        self.run_command(["diff", '--json', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="update" )
        self.run_command(["diff", '--json', '--skip-tables', 'simple', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff" ) #empty diff
        self.run_command(["diff", '--summary', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff_summary" )
        self.run_command(["diff", '--driver', 'sqlite', '\'\'', '--summary', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff_summary" )
        self.run_command(["diff", '--driver-1', 'sqlite', '\'\'', '--driver-2', 'sqlite', '\'\'', '--summary', geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], check_in_output="geodiff_summary" )
        self.run_command(["diff", geodiff_test_dir()+'/base.gpkg', geodiff_test_dir()+'/2_inserts/inserted_1_A.gpkg', outdir + "/diff.diff"] )

        print("-- copy" )
        self.run_command(["copy"], expect_fail=True )
        self.run_command(["copy", geodiff_test_dir()+'/non-existent.gpkg', outdir+'/copy.gpkg'], expect_fail=True )
        self.run_command(["copy", geodiff_test_dir()+'/base.gpkg', outdir+'/copy.gpkg', 'extra_arg'], expect_fail=True )
        self.run_command(["copy", '--skip-tables', geodiff_test_dir()+'/base.gpkg', outdir+'/copy.gpkg'], expect_fail=True )
        self.run_command(["copy", '--driver-1', geodiff_test_dir()+'/base.gpkg', outdir+'/copy.gpkg'], expect_fail=True )
        self.run_command(["copy", '--driver-2', geodiff_test_dir()+'/base.gpkg', outdir+'/copy.gpkg'], expect_fail=True )
        self.run_command(["copy", '--driver', 'sqlite', '\'\'',  '--skip-tables', 'unknown', geodiff_test_dir()+'/base.gpkg', outdir+'/copy2.gpkg'], expect_fail=True)
        
        self.run_command(["copy", geodiff_test_dir()+'/base.gpkg', outdir+'/copy.gpkg'])
        
        print("-- apply" )
        self.run_command(["copy", geodiff_test_dir()+'/base.gpkg', outdir+'/copyA.gpkg'])
        self.run_command(["copy", geodiff_test_dir()+'/base.gpkg', outdir+'/copyB.gpkg'])
        
        self.run_command(["apply"], expect_fail=True )
        self.run_command(["apply", '--driver'], expect_fail=True )
        self.run_command(["apply", '--skip-tables'], expect_fail=True )
        self.run_command(["apply", outdir+'/copyA.gpkg'], expect_fail=True )
        self.run_command(["apply", outdir+'/copyA.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], expect_fail=True ) # second arg is diff
        self.run_command(["apply", outdir+'/copyA.gpkg', outdir + "/diff.diff", 'extra_arg'], expect_fail=True )
        self.run_command(["apply", outdir+'/copyA.gpkg', outdir + "/diff.diff", 'extra_arg'], expect_fail=True )
        self.run_command(["apply", '--driver', outdir+'/copyA.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'], expect_fail=True )
        self.run_command(["apply", '--driver', 'sqlite', outdir+'/copyA.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'], expect_fail=True )
        self.run_command(["apply", '--skip-tables', outdir+'/copyA.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'], expect_fail=True )
        self.run_command(["apply", '--invalid-flag', outdir+'/copyA.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'], expect_fail=True )
        
        self.run_command(["apply", '--driver', 'sqlite', '\'\'', '--skip-tables', '\'\'', outdir+'/copyA.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'] )
        self.run_command(["apply", outdir+'/copyB.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'] )
        
        print("-- rebase-diff" )
        self.run_command(["copy", geodiff_test_dir()+'/base.gpkg', outdir+'/copyF.gpkg'])
        
        self.run_command(["rebase-diff"], expect_fail=True )
        self.run_command(["rebase-diff", outdir+'/copyF.gpkg'], expect_fail=True )
        self.run_command(["rebase-diff", outdir+'/copyF.gpkg', geodiff_test_dir()+'/1_geopackage/modified_1_geom.gpkg'], expect_fail=True ) # second arg is diff
        self.run_command(["rebase-diff", outdir+'/copyF.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff', geodiff_test_dir()+'/2_updates/base-updated_A.diff', outdir + "/rebase-diff.diff" , outdir + "/confF.confict" , 'extra_arg'], expect_fail=True )
        self.run_command(["rebase-diff", outdir+'/copyF.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff', geodiff_test_dir()+'/bad.diff', outdir + "/rebase-diff.diff" , outdir + "/confF.confict"], expect_fail=True )
        self.run_command(["rebase-diff", outdir+'/copyF.gpkg', geodiff_test_dir()+'/bad.diff', geodiff_test_dir()+'/2_updates/base-updated_A.diff', outdir + "/rebase-diff.diff" , outdir + "/confF.confict"], expect_fail=True )
        self.run_command(["rebase-diff", outdir+'/copyF.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff', geodiff_test_dir()+'/2_updates/base-updated_A.diff', outdir + "/rebase-diff.diff"], expect_fail=True )

        self.run_command(["rebase-diff", outdir+'/copyF.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff', geodiff_test_dir()+'/2_updates/base-updated_A.diff', outdir + "/rebase-diff.diff" , outdir + "/confF.confict"] )
        
        print("-- rebase-db" )
        self.run_command(["copy", geodiff_test_dir()+'/base.gpkg', outdir+'/copyD.gpkg'])
        
        self.run_command(["rebase-db"], expect_fail=True )
        self.run_command(["rebase-db", "--bad_flag"], expect_fail=True )
        self.run_command(["rebase-db", geodiff_test_dir()+'/base.gpkg', outdir+'/copyD.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff'], expect_fail=True) # missing arg
        self.run_command(["rebase-db", geodiff_test_dir()+'/bad.gpkg', outdir+'/copyD.gpkg', geodiff_test_dir()+'/bad.diff',outdir + "/rebasedb.conflicts.json"], expect_fail=True)
        self.run_command(["rebase-db", geodiff_test_dir()+'/base.gpkg', outdir+'/copyD.gpkg', geodiff_test_dir()+'/bad.diff',outdir + "/rebasedb.conflicts.json"], expect_fail=True)
        self.run_command(["rebase-db", geodiff_test_dir()+'/base.gpkg', outdir+'/bad.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff',outdir + "/rebasedb.conflicts.json"], expect_fail=True)
        
        self.run_command(["rebase-db", geodiff_test_dir()+'/base.gpkg', outdir+'/copyD.gpkg', geodiff_test_dir()+'/2_inserts/base-inserted_1_A.diff',outdir + "/rebasedb.conflicts.json"])
        
        print("-- invert" )
        self.run_command(["invert"], expect_fail=True )
        self.run_command(["invert", "--bad_flag"], expect_fail=True )
        self.run_command(["invert", geodiff_test_dir()+'/concat/bar-insert.diff'], expect_fail=True)
        self.run_command(["invert", geodiff_test_dir()+'/concat/bar-insert.diff', outdir+'/invert.diff', 'extra_arg'], expect_fail=True)
        self.run_command(["invert", geodiff_test_dir()+'/concat/non-existent-file.diff', outdir+'/invert.diff'], expect_fail=True)
        
        self.run_command(["invert", geodiff_test_dir()+'/concat/bar-insert.diff', outdir+'/invert.diff'])
        self.run_command(["as-json", outdir+'/invert.diff'], check_in_output="points" )
        
        print("-- concat" )
        self.run_command(["concat"], expect_fail=True )
        self.run_command(["concat", geodiff_test_dir()+'/concat/non-existent-file.diff', geodiff_test_dir()+'/concat/bar-update.diff', outdir+'/concat-fail.diff'], expect_fail=True)
        self.run_command(["concat", geodiff_test_dir()+'/concat/bar-insert.diff', geodiff_test_dir()+'/concat/bar-update.diff', outdir+'/concat2.diff'])
        self.run_command(["as-json", outdir+'/concat2.diff'], check_in_output="MODIFIED" )
        self.run_command(["concat", geodiff_test_dir()+'/concat/bar-insert.diff', geodiff_test_dir()+'/concat/bar-update.diff', geodiff_test_dir()+'/concat/bar-delete.diff', outdir+'/concat3.diff'])
        self.run_command(["as-json", outdir+'/concat3.diff'], check_in_output="geodiff" ) # empty file
        
        print("-- schema" )
        self.run_command(["schema"], expect_fail=True )
        self.run_command(["schema", geodiff_test_dir()+'/non-existent.gpkg'], expect_fail=True )
        self.run_command(["schema", geodiff_test_dir()+'/base.gpkg'], check_in_output="MEDIUMINT" )
        self.run_command(["schema", geodiff_test_dir()+'/base.gpkg', outdir+'/schema.txt'])
        file_contains( outdir + "/schema.txt", "MEDIUMINT")
        self.run_command(["schema", geodiff_test_dir()+'/base.gpkg', outdir+'/schema-fail.txt', "extra_arg"], expect_fail=True )
        
        print("-- drivers") 
        self.run_command(["drivers"], check_in_output="sqlite" )
        self.run_command(["drivers", "extra_arg"], expect_fail=True )
        
        print("-- version") 
        self.run_command(["version"], check_in_output="." )
        self.run_command(["version", "extra_arg"], expect_fail=True )
        
        print("-- help")
        self.run_command(["help"], check_in_output="Lutra Consulting")
        self.run_command(["help", "extra_arg"], expect_fail=True )