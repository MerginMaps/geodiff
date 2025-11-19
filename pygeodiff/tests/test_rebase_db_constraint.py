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
import sqlite3
from typing import Tuple

import pytest

import pygeodiff
from pygeodiff import GeoDiffLibError, GeoDiffLibConflictError

GEODIFFLIB = os.environ.get("GEODIFFLIB", None)


class LoggingGeoDiff(pygeodiff.GeoDiff):
    """
    Custom version of GeoDiff that captures log messages from the underlying
    C++ library and prints them for viewing with `pytest -vs`.  It can be used
    in place of pygeodiff.GeoDiff.  The log messages can be tested with:

    assert "value" in str(geodiff.log_messages)
    """
    def __init__(self, *args):
        super().__init__(*args)
        self.log_messages: list[dict] = []
        self.set_maximum_logger_level(4)
        self.set_logger_callback(self._handle_log_message)

    def _handle_log_message(self, level: int, raw_message: bytes):
        message: str = raw_message.decode('utf-8')
        self.log_messages.append({'level': level, 'msg': message})

        level_names = {
            1: "ERROR",
            2: "WARNING",
            3: "INFO",
            4: "DEBUG"
        }
        print(f"{level_names[level]}: {message}")


# These tests cover simple rebase scenarios, with or without database constraints
# applied.


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
def test_geodiff_rebase_happy_path_single_table(
    db_constrained, user_a_data_first, tmp_path
):
    """
    This test checks that rebase succeeds on changes to a single table with
    or without unique constraints.  This test applies INSERT, UPDATE and DELETE
    changes that should not produce any conflicts.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=db_constrained)

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

    with sqlite3.connect(user_a) as conn_a:
        conn_a.execute(
            """
            UPDATE species SET name='Maple_updated' WHERE species_id='MPL'
            """
        )
        conn_a.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'BCH', 'Birch'
            )
            """
        )

    with sqlite3.connect(user_b) as conn_b:
        conn_b.execute(
            """
            UPDATE species SET name='Oak_updated' WHERE species_id='OAK'
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
            DELETE FROM trees WHERE species_id IS 'PIN'
            """
        )
        conn_b.execute(
            """
            DELETE FROM species WHERE species_id IS 'PIN'
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    # Act (this will raise a GeoDiffLibError if geodiff cannot handle foreign keys)
    geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))

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
def test_geodiff_rebase_happy_path_fk_tables(
    db_constrained, user_a_data_first, tmp_path
):
    """
    This test checks that rebase succeeds on changes on multiple tables related
    by a foreign key constraint.  This test applies INSERT, UPDATE and DELETE
    changes that should not produce any conflicts.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=db_constrained)

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

    with sqlite3.connect(user_a) as conn_a:
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
            DELETE FROM trees WHERE tree_id IS 20251103003
            """
        )
        conn_a.execute(
            """
            DELETE FROM species WHERE species_id IS 'PIN'
            """
        )
        conn_a.execute(
            """
            UPDATE trees SET age=1 WHERE tree_id=20251103001
            """
        )

    with sqlite3.connect(user_b) as conn_b:
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

    # Act (this will raise a GeoDiffLibError if geodiff cannot handle foreign keys)
    geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))

    # Assert that rebased database contains expected changes and no conflict exists
    assert_data_as_expected(mine, expected)
    assert not conflict.exists()


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_happy_path_no_changes(
    user_a_data_first, tmp_path
):
    """
    This test checks that rebase does nothing when no changes have been made.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Apply changes to databases to give the following expected values
    expected = {
        "species": [
            {"species_id": "MPL", "name": "Maple"},
            {"species_id": "OAK", "name": "Oak"},
            {"species_id": "PIN", "name": "Pine"},
        ],
        "trees": [
            {"tree_id": 20251103001, "species_id": "MPL", "age": 25},
            {"tree_id": 20251103002, "species_id": "OAK", "age": 30},
            {"tree_id": 20251103003, "species_id": "PIN", "age": 18},
        ],
    }

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    # Act
    geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))

    # Assert that rebased database contains expected changes and no conflict exists
    assert_data_as_expected(mine, expected)
    assert not conflict.exists()


# The next tests cover scenarios with conflicting changes or where changes
# result in constraint violations.


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_conflicting_edits(user_a_data_first, tmp_path):
    """
    This test checks that rebase handles conflicting edits to the same row and
    column.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Apply changes to databases (both users edit same value)
    with sqlite3.connect(user_a) as conn_a:
        conn_a.execute(
            """
            UPDATE species SET name='Maple_A' WHERE species_id='MPL'
            """
        )

    with sqlite3.connect(user_b) as conn_b:
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

    # Act
    geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))

    # Assert that rebased database contains expected changes and conflict is recorded
    assert_data_as_expected(mine, expected)
    assert conflict.exists()
    conflict_json = json.loads(conflict.read_text())
    assert conflict_json["geodiff"][0]["type"] == "conflict"
    assert conflict_json["geodiff"][0]["table"] == "species"


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_unique_constraint_violation(user_a_data_first, tmp_path):
    """
    This test covers a rebase where combining edits causes a unique constraint
    violation.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Apply changes to databases (both users create the same species_id)
    with sqlite3.connect(user_a) as conn_a:
        conn_a.execute(
            """
            INSERT INTO species (species_id, name) VALUES (
                'BCH', 'Birch'
            )
            """
        )

    with sqlite3.connect(user_b) as conn_b:
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

    # Assert that rebasing fails due to DB constraints on application of diffs
    # and that exception provides information on failed constraint.
    with pytest.raises(GeoDiffLibConflictError, match=".*constraint conflict.*"):
        geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_fkey_constraint_violation(user_a_data_first, tmp_path):
    """
    This test covers a rebase where combining edits causes a foreign key constraint
    violation.

    Note: SQLite only enforces Foreign Key constraints when 'PRAGMA foreign_keys = ON'
    has been applied.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Apply changes to databases (user_a deletes species_id for tree inserted by user_b)
    with sqlite3.connect(user_a) as conn_a:
        # Delete pine trees and parent pine species
        conn_a.execute(
            """
            DELETE FROM trees WHERE species_id IS 'PIN'
            """
        )
        conn_a.execute(
            """
            DELETE FROM species WHERE species_id IS 'PIN'
            """
        )

    with sqlite3.connect(user_b) as conn_b:
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

    # Assert that rebasing fails due to DB constraints on application of diffs
    # and that exception provides information on failed constraint.
    with pytest.raises(GeoDiffLibConflictError, match=".*FOREIGN KEY constraint failed."):
        geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_fail_on_insert(user_a_data_first, tmp_path):
    """
    This test covers a rebase where insert fails for a non-constraint reason. A
    trigger is used to force the failure.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Apply changes to databases
    with sqlite3.connect(user_a) as conn_a:
        conn_a.execute(
            """
            INSERT INTO species (species_id, name) VALUES ('BCH', 'Birch')
            """
        )

    with sqlite3.connect(user_b) as conn_b:
        conn_b.execute(
            """
            INSERT INTO species (species_id, name) VALUES ('SPC', 'Spruce')
            """
        )

    for db in user_a, user_b:
        # Add a trigger to prevent further inserts
        with sqlite3.connect(db) as conn:
            conn.execute(
                """
                CREATE TRIGGER "fail_on_insert"
                    BEFORE INSERT ON "species"
                    BEGIN
                        SELECT RAISE(FAIL, "Cannot add more species");
                    END;
                """
            )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    # Assert that rebasing fails due to DB constraints on application of diffs
    # and that exception provides information on failed constraint.
    with pytest.raises(GeoDiffLibConflictError, match="Cannot add more species"):
        geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_db_connection_error(user_a_data_first, tmp_path):
    """
    This test covers a rebase where a database connection cannot be made.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Delete database
    original.unlink()

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    # Assert
    with pytest.raises(GeoDiffLibError, match=""):
        geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))


@pytest.mark.parametrize(
    "user_a_data_first", [True, False], ids=["user_a_data_first", "user_b_data_first"]
)
def test_geodiff_rebase_missing_table_error(user_a_data_first, tmp_path):
    """
    This test covers a rebase where a table is missing from one of the databases.
    """
    # Arrange
    geodiff = pygeodiff.GeoDiff(GEODIFFLIB)
    conflict = tmp_path / "conflict.txt"
    original, user_a, user_b = create_db_files(tmp_path, db_constrained=True)

    # Apply changes to databases
    with sqlite3.connect(original) as conn:
        conn.execute(
            """
            DROP TABLE trees
            """
        )

    # Set the argument order. Rebased db has "their" changes before "mine".
    if user_a_data_first:
        theirs, mine = user_a, user_b
    else:
        theirs, mine = user_b, user_a

    # Assert
    with pytest.raises(GeoDiffLibError, match=""):
        geodiff.rebase(str(original), str(theirs), str(mine), str(conflict))


# Helper functions are defined below here.


def assert_data_as_expected(db: Path, expected_data: dict[str, list[dict]]):
    """
    Assert that table contents are as expected.  Use `set` comparison because
    the row ordering depends on the order that tables are passed to `rebase`.
    """
    with sqlite3.connect(db) as conn:
        for table, rows in expected_data.items():
            column_names = rows[0].keys()
            results = conn.execute(
                f"""SELECT {", ".join(column_names)} FROM {table}"""
            ).fetchall()

            assert set(results) == set(tuple(row.values()) for row in rows)


def create_db_files(
    tmp_path: Path, db_constrained: bool = True
) -> Tuple[Path, Path, Path]:
    """
    Create 3 SQLite files at the given filepaths, each with the same tables
    and data.

    If `db_constrained`, the database will apply UNIQUE and FOREIGN
    KEY constraints.
    """
    original = tmp_path / "original.db"
    user_a = tmp_path / "user_a.db"
    user_b = tmp_path / "user_b.db"

    # Define tables
    create_species_sql = """
        CREATE TABLE species (
            fid INTEGER PRIMARY KEY,
            species_id TEXT UNIQUE,
            name TEXT
        )"""
    create_trees_sql = """
        CREATE TABLE trees (
            fid INTEGER PRIMARY KEY,
            tree_id INTEGER UNIQUE NOT NULL,
            species_id TEXT NOT NULL,
            age INTEGER,
            FOREIGN KEY("species_id") REFERENCES "species"("species_id")
        )"""

    if not db_constrained:
        # Remove database constraints from table definitions
        create_species_sql = re.sub(r" UNIQUE", "", create_species_sql)
        create_trees_sql = re.sub(r" UNIQUE", "", create_trees_sql)
        create_trees_sql = re.sub(
            r",\W*FOREIGN.*REFERENCES.*?\)", "", create_trees_sql, flags=re.MULTILINE
        )

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

    # Create geopackages
    with sqlite3.connect(original) as conn:
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

    user_a.write_bytes(original.read_bytes())
    user_b.write_bytes(original.read_bytes())

    return original, user_a, user_b
