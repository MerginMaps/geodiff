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
        if libname is None, it tries to import c-extension from wheel
    """
    def __init__(self, libname=None):
        self.clib = GeoDiffLib(libname)

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
    def create_changeset(self, base, modified, changeset):
        return self.clib.create_changeset(base, modified, changeset)

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
         
         raises SqliteDiffError on error
    """
    def create_rebased_changeset(self, base, modified, changeset_their, changeset):
        return self.clib.create_rebased_changeset(base, modified, changeset_their, changeset)

    """
        Applies changeset file (binary) to BASE and creates PATCHED.
        
        \param base [input] BASE sqlite3/geopackage file
        \param patched [output] PATCHED sqlite3/geopackage file with changeset
        \param changeset [input] changeset between BASE -> PATCHED
        \returns number of conflics
        
        raises SqliteDiffError on error
    """
    def apply_changeset(self, base, patched, changeset):
        return self.clib.apply_changeset(base, patched, changeset)

    """ 
        Lists changeset content to stdout
        \returns number of changes
         
         reises SqliteDiffError on error
    """
    def list_changes(self, changeset):
        return self.clib.list_changes(changeset)

    def version(self):
        return self.clib.version()


def main():
    diff_lib = GeoDiff()
    print("pygeodiff " + diff_lib.version())
