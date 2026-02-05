# -*- coding: utf-8 -*-
"""
    pygeodiff
    -----------
    This module provides tools for create diffs of geospatial data formats
    :copyright: (c) 2019-2022 Lutra Consulting Ltd.
    :license: MIT, see LICENSE for more details.
"""

from .main import GeoDiff, shutdown
from .geodifflib import (
    GeoDiffLibError,
    GeoDiffLibConflictError,
    GeoDiffLibUnsupportedChangeError,
    GeoDiffLibVersionError,
    ChangesetEntry,
    ChangesetReader,
    UndefinedValue,
)

__all__ = [
    "GeoDiff",
    "shutdown",
    "GeoDiffLibError",
    "GeoDiffLibConflictError",
    "GeoDiffLibUnsupportedChangeError",
    "GeoDiffLibVersionError",
    "ChangesetEntry",
    "ChangesetReader",
    "UndefinedValue",
]
