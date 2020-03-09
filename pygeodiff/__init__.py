# -*- coding: utf-8 -*-
'''
    pygeodiff
    -----------
    This module provides tools for create diffs of geospatial data formats
    :copyright: (c) 2019 Peter Petrik
    :license: MIT, see LICENSE for more details.
'''

from .main import GeoDiff
from .geodifflib import (
    GeoDiffLibError,
    GeoDiffLibConflictError,
    GeoDiffLibUnsupportedChangeError,
    GeoDiffLibVersionError
)
