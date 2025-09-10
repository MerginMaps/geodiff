"""
With the issue as described in https://github.com/MerginMaps/geodiff/issues/210 the test:

test_geodiff_rebase_unique - with id 'unique_constraint'

should fail *expectedly* while all others should pass.

$ pytest pygeodiff/tests -vk geodiff_rebase
pygeodiff/tests/test_geodiff_rebase.py::test_geodiff_rebase_unique[user_a_first-no_constraint] PASSED
pygeodiff/tests/test_geodiff_rebase.py::test_geodiff_rebase_unique[user_a_first-unique_constraint] XFAIL
    (Expected to fail due to issue 210, when this xpasses remove this decorator)
pygeodiff/tests/test_geodiff_rebase.py::test_geodiff_rebase_unique[user_b_first-no_constraint] PASSED
pygeodiff/tests/test_geodiff_rebase.py::test_geodiff_rebase_unique[user_b_first-unique_constraint] XFAIL
    (Expected to fail due to issue 210, when this xpasses remove this decorator)

Once the issue is resolved the 'unique_constraint' tests should unexpectedly pass which will be treated
as FAILED tests with the additional information:

[XPASS(strict)] Expected to fail due to issue 210, when this xpasses remove this decorator

After removing the 'marks' parameter all tests should now pass.

Note: this test requires pytest and etlhelper to be installed. In a virtualenv, run:

pip install -r requirements_dev.txt
"""

import os
from pathlib import Path
import sqlite3
from typing import List, Tuple

import etlhelper as etl
from etlhelper.row_factories import tuple_row_factory
import pytest

import pygeodiff
from pygeodiff.geodifflib import GeoDiffLibConflictError, GeoDiffLibError

GEODIFFLIB = os.environ.get("GEODIFFLIB", None)

CREATE_TABLE = """
    CREATE TABLE trees (
        "fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
        "species" TEXT,
        "age" INTEGER,
        "user_id" TEXT UNIQUE
    )
"""

ORIGINAL_DATA = [
    {"species": "Maple", "age": 25, "user_id": "original_001"},
    {"species": "Oak", "age": 30, "user_id": "original_002"},
    {"species": "Pine", "age": 18, "user_id": "original_003"},
]


# For a failing test to develop against or once the tests are XPASSing,
# the xfail decorator should be replaced with:
#
# @pytest.mark.parametrize('create_table_ddl', [
#     pytest.param(CREATE_TABLE.replace(' UNIQUE', ''), id='no_constraint'),
#     pytest.param(CREATE_TABLE, id='unique_constraint')
# ])
@pytest.mark.parametrize('create_table_ddl', [
    pytest.param(CREATE_TABLE.replace(' UNIQUE', ''), id='no_constraint'),
    pytest.param(CREATE_TABLE,
                 marks=pytest.mark.xfail(
                    raises=GeoDiffLibError,
                    strict=True,
                    reason="Expected to fail due to issue 210, when this xpasses remove this decorator"),
                 id='unique_constraint')
])
@pytest.mark.parametrize('user_a_data_first', [True, False], ids=['user_a_data_first', 'user_b_data_first'])
def test_geodiff_rebase_unique(create_table_ddl, user_a_data_first, tmp_path):
    """
    This test exemplifies issue 210. With the UNIQUE constraint on the `user_id` column,
    the data should be able to be rebased as there are no conflicting values, but this
    currently fails for the reason outlined in the issue.

    Without the UNIQUE constraint the rebase takes place as expected, with no conflicts
    to be resolved.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_gpkg_files(create_table_ddl, tmp_path)
    # Insert a new row in each user data table
    with sqlite3.connect(user_a) as conn_a, sqlite3.connect(user_b) as conn_b:
        etl.execute("INSERT INTO trees VALUES (null, 'Fir', 12, 'user_a_001')", conn_a)
        etl.execute("INSERT INTO trees VALUES (null, 'Elm', 22, 'user_b_001')", conn_b)

    # Set the argument order, i.e. which gpkg should be the rebased result
    if user_a_data_first:
        older, newer = user_a, user_b
    else:
        older, newer = user_b, user_a

    # The expected data following a successful rebase
    expected = [
        ("Maple", 25, "original_001"),
        ("Oak", 30, "original_002"),
        ("Pine", 18, "original_003"),
        ("Elm", 22, "user_b_001"),  # Added row from user_b
        ("Fir", 12, "user_a_001"),  # Added row from user_a
    ]

    # Act & Assert
    try:
        geodiff.rebase(str(original), str(older), str(newer), str(conflict))
        # The rebased gpkg should contain all changes and no conflict file should be created
        assert_gpkg(newer, expected)
        assert not conflict.exists()
    except GeoDiffLibConflictError:
        # This error subclass SHOULD NOT be raised here
        pytest.fail("Incorrect exception raised")
    except GeoDiffLibError as excinfo:
        # UNIQUE constraint on the user_id column causes geodiff.rebase to fail
        assert excinfo.args[0] == 'rebase'
        raise excinfo


def create_gpkg_files(create_table_ddl: str, tmp_path: Path) -> Tuple[Path]:
    """
    Create 3 GeoPackage files at the given filepaths, create the
    same table with the same original data in each of them.
    """
    original = tmp_path / "original.gpkg"
    user_a = tmp_path / "user_a.gpkg"
    user_b = tmp_path / "user_b.gpkg"
    for gpkg in [original, user_a, user_b]:
        with sqlite3.connect(gpkg) as conn:
            # Add an empty table and then populate with initial data
            etl.execute(create_table_ddl, conn)
            etl.load("trees", conn, ORIGINAL_DATA)

    return original, user_a, user_b


def assert_gpkg(gpkg: Path, expected: List[tuple]):
    """
    Assert that the data (excluding fid) in the gpkg is the
    expected data, regardless of row order.
    """
    with sqlite3.connect(gpkg) as conn:
        result = etl.fetchall("SELECT species, age, user_id FROM trees", conn, row_factory=tuple_row_factory)
        assert set(result) == set(expected)
