# -*- coding: utf-8 -*-
'''
    pygeodiff.main
    --------------
    Main entry of the library
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .geodifflib import GeoDiffLib


class GeoDiff:
    """
        geodiff is a module to create and apply changesets to GIS files (geopackage)
    """
    
    def __init__(self, libname=None):
        """
            if libname is None, it tries to import c-extension from wheel
            messages are shown in stdout/stderr. 
            Use environment variable GEODIFF_LOGGER_LEVEL 0(Nothing)-4(Debug) to
            set level (Errors by default)
        """
        self.clib = GeoDiffLib(libname)

    def set_logger_callback(self, callback):
        """
            Assign custom logger
            Replace default stdout/stderr logger with custom.
        """
        return self.clib.set_logger_callback(callback)

    LevelError = 1
    LevelWarning = 2
    LevelInfo = 3
    LevelDebug = 4

    def set_maximum_logger_level(self, maxLevel):
        """
           Assign maximum level of messages that are passed to logger callbac
           Based on maxLogLevel, the messages are filtered by level:
           maxLogLevel = 0 nothing is passed to logger callback
           maxLogLevel = 1 errors are passed to logger callback
           maxLogLevel = 2 errors and warnings are passed to logger callback
           maxLogLevel = 3 errors, warnings and infos are passed to logger callback
           maxLogLevel = 4 errors, warnings, infos, debug messages are passed to logger callback
        """
        return self.clib.set_maximum_logger_level(maxLevel)

    def create_changeset(self, base, modified, changeset):
        """
            Creates changeset file (binary) in such way that
            if CHANGESET is applied to BASE by applyChangeset,
            MODIFIED will be created

            BASE --- CHANGESET ---> MODIFIED

            \param base [input] BASE sqlite3/geopackage file
            \param modified [input] MODIFIED sqlite3/geopackage file
            \param changeset [output] changeset between BASE -> MODIFIED

            raises SqliteDiffError on error
        """
        return self.clib.create_changeset(base, modified, changeset)

    def invert_changeset(self, changeset, changeset_inv):
        """
            Inverts changeset file (binary) in such way that
            if CHANGESET_INV is applied to MODIFIED by applyChangeset,
            BASE will be created

            \param changeset [input] changeset between BASE -> MODIFIED
            \param changeset_inv [output] changeset between MODIFIED -> BASE

            \returns number of conflics

            raises SqliteDiffError on error
        """
        return self.clib.invert_changeset(changeset, changeset_inv)

    def rebase(self, base, modified_their, modified, conflict):
        """
            Rebases local modified version from base to modified_their version

                   --- > MODIFIED_THEIR
            BASE -|
                   ----> MODIFIED (local) ---> MODIFIED_THEIR_PLUS_MINE

            Steps performed on MODIFIED (local) file:
            1. undo local changes MODIFIED -> BASE
            2. apply changes from MODIFIED_THEIR
            3. apply rebased local changes and create MODIFIED_THEIR_PLUS_MINE

            Note, when rebase is not successfull, modified could be in random state.
            This works in general, even when base==modified, or base==modified_theirs

             \param base [input] BASE sqlite3/geopackage file
             \param modified_their [input] MODIFIED sqlite3/geopackage file
             \param modified [input] local copy of the changes to be rebased
             \param conflict [output] file where the automatically resolved conflicts are stored. If there are no conflicts, file is not created

             raises SqliteDiffError on error
        """
        return self.clib.rebase(base, modified_their, modified, conflict)

    def create_rebased_changeset(self, base, modified, changeset_their, changeset, conflict):
        """
            Creates changeset file (binary) in such way that
            if CHANGESET is applied to MODIFIED_THEIR by
            applyChangeset, the new state will contain all
            changes from MODIFIED and MODIFIED_THEIR.

                   --- CHANGESET_THEIR ---> MODIFIED_THEIR --- CHANGESET ---> MODIFIED_THEIR_PLUS_MINE
            BASE -|
                   -----------------------> MODIFIED

             \param base [input] BASE sqlite3/geopackage file
             \param modified [input] MODIFIED sqlite3/geopackage file
             \param changeset_their [input] changeset between BASE -> MODIFIED_THEIR
             \param changeset [output] changeset between MODIFIED_THEIR -> MODIFIED_THEIR_PLUS_MINE
             \param conflict [output] file where the automatically resolved conflicts are stored. If there are no conflicts, file is not created

             raises SqliteDiffError on error
        """
        return self.clib.create_rebased_changeset(base, modified, changeset_their, changeset, conflict)

    def apply_changeset(self, base, changeset):
        """
            Applies changeset file (binary) to BASE

            \param base [input/output] BASE sqlite3/geopackage file
            \param changeset [input] changeset to apply to BASE
            \returns number of conflicts

            raises SqliteDiffError on error
        """
        return self.clib.apply_changeset(base, changeset)

    def list_changes(self, changeset, json):
        """
            Lists changeset content JSON file
            JSON contains all changes in human/machine readable name
            \returns number of changes

             raises SqliteDiffError on error
        """
        return self.clib.list_changes(changeset, json)

    def list_changes_summary(self, changeset, json):
        """
            Lists changeset summary content JSON file
            JSON contains a list of how many inserts/edits/deletes is contained in changeset for each table
            \returns number of changes

             raises SqliteDiffError on error
        """
        return self.clib.list_changes_summary(changeset, json)


    def has_changes(self, changeset):
        """
            \returns whether changeset contains at least one change

            raises SqliteDiffError on error
        """
        return self.clib.has_changes(changeset)

    def changes_count(self, changeset):
        """
            \returns number of changes

             raises SqliteDiffError on error
        """
        return self.clib.changes_count(changeset)

    def version(self):
        """
            geodiff version
        """
        return self.clib.version()


def main():
    diff_lib = GeoDiff()
    print("pygeodiff " + diff_lib.version())
