# -*- coding: utf-8 -*-
"""
:copyright: (c) 2025 Colin Blackburn, Leo Rudczenko, John Stevenson (British Geological Survey)
:license: MIT, see LICENSE for more details.
"""

# This module tests the behaviour of geodiff when there are database-level constraints
# applied to the tables.

# Where tests are parametrised with `db_constrained`, the same test is run with and
# without the db constraints.  Tests against unconstrained databases act as
# regression tests for existing geodiff behaviour.  Tests are parametrised with
# `user_a_data_first` to show where rebase outcome depends on the order that
# database files are passed to the rebase function.

import json
import os
from pathlib import Path
import re
from typing import Tuple

import pytest

import pygeodiff
from pygeodiff import GeoDiffLibConflictError
from pygeodiff.tests.testutils import GeoDiffTestSqlDb

GEODIFFLIB = os.environ.get("GEODIFFLIB", None)

# These tests cover simple rebase scenarios, with or without database constraints
# applied.


driver_parametrize = pytest.mark.parametrize(
    "driver",
    [
        "sqlite",
        pytest.param(
            "postgres",
            marks=pytest.mark.skipif(
                "GEODIFF_PG_CONNINFO" not in os.environ,
                reason="Can't test Postgres without GEODIFF_PG_CONNINFO env variable",
            ),
        ),
    ],
)


@pytest.mark.parametrize(
    "db_constrained",
    [
        pytest.param(False, id="no_db_constraint"),
        pytest.param(True, id="db_constraint"),
    ],
)
@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
@driver_parametrize
def test_geodiff_rebase_happy_path_single_table(
    db_constrained, user_a_data_first, driver, tmp_path
):
    """
    This test checks that rebase succeeds on changes to a single table with
    or without unique constraints.  This test applies INSERT, UPDATE and DELETE
    changes that should not produce any conflicts.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    diff = tmp_path / "diff.bin"
    original, user_a, user_b = create_db_files(
        driver, tmp_path, db_constrained=db_constrained
    )

    # Apply changes to databases to give the following expected values
    expected = {
        "species": [
            {"species_id": "MPL", "name": "Maple_updated"},  # updated
            {"species_id": "OAK", "name": "Oak_updated"},  # updated
            # 'PIN' deleted
            {"species_id": "SPC", "name": "Spruce"},  # inserted
            {"species_id": "BCH", "name": "Birch"},  # inserted
        ],
    }

    with user_a.open() as conn_a:
        conn_a.execute(
            """
            UPDATE species SET name='Maple_updated' WHERE species_id = 'MPL'
            """
        )
        conn_a.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'BCH', 'Birch'
            )
            """
        )

    with user_b.open() as conn_b:
        conn_b.execute(
            """
            UPDATE species SET name='Oak_updated' WHERE species_id = 'OAK'
            """
        )
        conn_b.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'SPC', 'Spruce'
            )
            """
        )
        conn_b.execute(
            """
            DELETE FROM trees WHERE species_id = 'PIN'
            """
        )
        conn_b.execute(
            """
            DELETE FROM species WHERE species_id = 'PIN'
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    geodiff.create_changeset_ex(
        driver, original.driver_info, original.db, theirs.db, str(diff)
    )
    # Act (this will raise a GeoDiffLibError if geodiff cannot handle foreign keys)
    geodiff.rebase_ex(
        driver,
        original.driver_info,
        original.db,
        mine.db,
        str(diff),
        str(conflict),
    )

    # Assert that rebased database contains expected changes and no conflict exists
    assert_data_as_expected(mine, expected)
    assert not conflict.exists()


@pytest.mark.parametrize(
    "db_constrained",
    [
        pytest.param(False, id="no_db_constraint"),
        pytest.param(True, id="db_constraint"),
    ],
)
@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
@driver_parametrize
def test_geodiff_rebase_happy_path_fk_tables(
    db_constrained, user_a_data_first, driver, tmp_path
):
    """
    This test checks that rebase succeeds on changes on multiple tables related
    by a foreign key constraint.  This test applies INSERT, UPDATE and DELETE
    changes that should not produce any conflicts.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    diff = tmp_path / "diff.txt"
    original, user_a, user_b = create_db_files(
        driver, tmp_path, db_constrained=db_constrained
    )

    # Apply changes to databases to give the following expected values
    expected = {
        "species": [
            {"species_id": "MPL", "name": "Maple"},
            {"species_id": "OAK", "name": "Oak"},
            # 'PIN' deleted
            {"species_id": "SPC", "name": "Spruce"},  # inserted
            {"species_id": "BCH", "name": "Birch"},  # inserted
        ],
        "trees": [
            {"tree_id": 20251103001, "species_id": "MPL", "age": 1},  # age updated
            {"tree_id": 20251103002, "species_id": "OAK", "age": 99},  # age updated
            # 20241103003 deleted
            {"tree_id": 20251103004, "species_id": "SPC", "age": 29},  # inserted
            {"tree_id": 20251103005, "species_id": "BCH", "age": 46},  # inserted
        ],
    }

    with user_a.open() as conn_a:
        conn_a.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'BCH', 'Birch'
            )
            """
        )
        conn_a.execute(
            """
            INSERT INTO trees (tree_id, species_id, age) VALUES (
                20251103005, 'BCH', 46
            )
            """
        )
        conn_a.execute(
            """
            DELETE FROM trees WHERE tree_id = 20251103003
            """
        )
        conn_a.execute(
            """
            DELETE FROM species WHERE species_id = 'PIN'
            """
        )
        conn_a.execute(
            """
            UPDATE trees SET age=1 WHERE tree_id=20251103001
            """
        )

    with user_b.open() as conn_b:
        conn_b.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'SPC', 'Spruce'
            )
            """
        )
        conn_b.execute(
            """
            INSERT INTO trees (tree_id, species_id, age) VALUES (
                20251103004, 'SPC', 29
            )
            """
        )
        conn_b.execute(
            """
            UPDATE trees SET age=99 WHERE tree_id=20251103002
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    geodiff.create_changeset_ex(
        driver, original.driver_info, original.db, theirs.db, str(diff)
    )
    # Act (this will raise a GeoDiffLibError if geodiff cannot handle foreign keys)
    geodiff.rebase_ex(
        driver,
        original.driver_info,
        original.db,
        mine.db,
        str(diff),
        str(conflict),
    )

    # Assert that rebased database contains expected changes and no conflict exists
    assert_data_as_expected(mine, expected)
    assert not conflict.exists()


# The next tests cover scenarios with conflicting changes or where changes
# result in constraint violations.


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
@driver_parametrize
def test_geodiff_rebase_conflicting_edits(user_a_data_first, driver, tmp_path):
    """
    This test checks that rebase handles conflicting edits to the same row and
    column.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    diff = tmp_path / "diff.txt"
    original, user_a, user_b = create_db_files(driver, tmp_path, db_constrained=True)

    # Apply changes to databases (both users edit same value)
    with user_a.open() as conn_a:
        conn_a.execute(
            """
            UPDATE species SET name='Maple_A' WHERE species_id='MPL'
            """
        )

    with user_b.open() as conn_b:
        conn_b.execute(
            """
            UPDATE species SET name='Maple_B' WHERE species_id='MPL'
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
        # user_a changes went in first then were overwritten by user_b changes.
        expected = {
            "species": [
                {"species_id": "MPL", "name": "Maple_B"},
                {"species_id": "OAK", "name": "Oak"},
                {"species_id": "PIN", "name": "Pine"},
            ]
        }
    else:
        theirs, mine = user_b, user_a
        # user_b changes went in first then were overwritten by user_a changes.
        expected = {
            "species": [
                {"species_id": "MPL", "name": "Maple_A"},
                {"species_id": "OAK", "name": "Oak"},
                {"species_id": "PIN", "name": "Pine"},
            ]
        }

    geodiff.create_changeset_ex(
        driver, original.driver_info, original.db, theirs.db, str(diff)
    )
    # Act (this will raise a GeoDiffLibError if geodiff cannot handle foreign keys)
    geodiff.rebase_ex(
        driver, original.driver_info, original.db, mine.db, str(diff), str(conflict)
    )

    # Assert that rebased database contains expected changes and conflict is recorded
    assert_data_as_expected(mine, expected)
    assert conflict.exists()
    conflict_json = json.loads(conflict.read_text())
    assert conflict_json["geodiff"][0]["type"] == "conflict"
    assert conflict_json["geodiff"][0]["table"] == "species"


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
@driver_parametrize
def test_geodiff_rebase_unique_constraint_violation(
    user_a_data_first, driver, tmp_path
):
    """
    This test covers a rebase where combining edits causes a unique constraint
    violation.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    diff = tmp_path / "diff.txt"
    original, user_a, user_b = create_db_files(driver, tmp_path, db_constrained=True)

    # Apply changes to databases (both users create the same species_id)
    with user_a.open() as conn_a:
        conn_a.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'BCH', 'Birch'
            )
            """
        )

    with user_b.open() as conn_b:
        conn_b.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'BCH', 'Beech'
            )
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    geodiff.create_changeset_ex(
        driver, original.driver_info, original.db, theirs.db, str(diff)
    )
    # Assert that rebasing fails due to DB constraints on application of diffs
    with pytest.raises(GeoDiffLibConflictError):
        geodiff.rebase_ex(
            driver, original.driver_info, original.db, mine.db, str(diff), str(conflict)
        )


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
@driver_parametrize
def test_geodiff_rebase_fkey_constraint_violation(user_a_data_first, driver, tmp_path):
    """
    This test covers a rebase where combining edits causes a foreign key constraint
    violation.

    Note: SQLite only enforces Foreign Key constraints when 'PRAGMA foreign_keys = ON'
    has been applied.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    diff = tmp_path / "diff.txt"
    original, user_a, user_b = create_db_files(driver, tmp_path, db_constrained=True)

    # Apply changes to databases (user_a deletes species_id for tree inserted by user_b)
    with user_a.open() as conn_a:
        # Delete pine trees and parent pine species
        conn_a.execute(
            """
            DELETE FROM trees WHERE species_id = 'PIN'
            """
        )
        conn_a.execute(
            """
            DELETE FROM species WHERE species_id = 'PIN'
            """
        )

    with user_b.open() as conn_b:
        # Add a pine tree
        conn_b.execute(
            """
            INSERT INTO trees (tree_id, species_id, age) VALUES (
                20251103004, 'PIN', 101
            )
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    geodiff.create_changeset_ex(
        driver, original.driver_info, original.db, theirs.db, str(diff)
    )
    # Assert that rebasing fails due to DB constraints on application of diffs
    with pytest.raises(
        GeoDiffLibConflictError, match=".*FOREIGN KEY constraint failed.*"
    ):
        geodiff.rebase_ex(
            driver, original.driver_info, original.db, mine.db, str(diff), str(conflict)
        )


# Helper functions are defined below here.


def assert_data_as_expected(db: GeoDiffTestSqlDb, expected_data: dict[str, list[dict]]):
    """
    Assert that table contents are as expected.  Use `set` comparison because
    the row ordering depends on the order that tables are passed to `rebase`.
    """
    with db.open() as conn:
        for table, rows in expected_data.items():
            column_names = rows[0].keys()
            results = conn.execute(
                f"""SELECT {", ".join(column_names)} FROM {table}"""
            ).fetchall()

            assert set(results) == set(tuple(row.values()) for row in rows)


def create_db_files(
    driver: str, tmp_path: Path, db_constrained: bool = True
) -> Tuple[GeoDiffTestSqlDb, GeoDiffTestSqlDb, GeoDiffTestSqlDb]:
    """
    Create 3 Postgres schemata or SQLite files at the given filepaths, each
    filled with the same data.

    If `db_constrained`, the database will apply UNIQUE and FOREIGN
    KEY constraints.
    """

    if driver == "sqlite":
        original = tmp_path / "original.db"
        user_a = tmp_path / "user_a.db"
        user_b = tmp_path / "user_b.db"

        pkey_type = "integer"
    elif driver == "postgres":
        original = "original"
        user_a = "user_a"
        user_b = "user_b"

        pkey_type = "serial"

    # Define tables
    create_species_sql = f"""
        CREATE TABLE species (
            fid {pkey_type} PRIMARY KEY DEFERRABLE,
            species_id TEXT UNIQUE,
            name TEXT
        )"""
    create_trees_sql = f"""
        CREATE TABLE trees (
            fid {pkey_type} PRIMARY KEY DEFERRABLE,
            tree_id BIGINT UNIQUE DEFERRABLE NOT NULL,
            species_id TEXT NOT NULL,
            age INTEGER,
            FOREIGN KEY("species_id") REFERENCES "species"("species_id") DEFERRABLE
        )"""

    if not db_constrained:
        # Remove database constraints from table definitions
        create_species_sql = re.sub(r" UNIQUE", "", create_species_sql)
        create_trees_sql = re.sub(r" UNIQUE", "", create_trees_sql)
        create_trees_sql = re.sub(
            r",\W*FOREIGN.*REFERENCES.*?\)", "", create_trees_sql, re.MULTILINE
        )

    if driver == "sqlite" or not db_constrained:
        # Remove DEFERRABLE specifier
        create_species_sql = re.sub(r"\bDEFERRABLE\b", "", create_species_sql)
        create_trees_sql = re.sub(r"\bDEFERRABLE\b", "", create_trees_sql)

    # Define initial data
    species_data = [
        {"species_id": "MPL", "name": "Maple"},
        {"species_id": "OAK", "name": "Oak"},
        {"species_id": "PIN", "name": "Pine"},
    ]
    trees_data = [
        {"tree_id": 20251103001, "species_id": "MPL", "age": 25},
        {"tree_id": 20251103002, "species_id": "OAK", "age": 30},
        {"tree_id": 20251103003, "species_id": "PIN", "age": 18},
    ]

    # Create databases
    dbs = []
    for dbname in [original, user_a, user_b]:
        db = GeoDiffTestSqlDb(driver, dbname, destroy_existing=True)
        dbs.append(db)
        with db.open() as conn:
            conn.execute(create_species_sql)
            conn.execute(create_trees_sql)
            conn.executemany(
                """
                INSERT INTO species (species_id, name)
                VALUES (:species_id, :name)
                """,
                species_data,
            )
            conn.executemany(
                """
                INSERT INTO trees (tree_id, species_id, age)
                VALUES (:tree_id, :species_id, :age)
                """,
                trees_data,
            )

    return tuple(dbs)
