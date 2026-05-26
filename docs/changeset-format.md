
# Changeset Format

The format for changesets is based on the SQLite3 session extension's internal
format. Below are details of the format:

## Summary

A changeset is a linear list of operations of various types, identified by a
one-byte tag:

- Table record (`'T'`)
- Data entry (`18`, `23`, `9`)
- Create table entry (`'a'`)
- Drop table entry (`'A'`)
- Add column entry (`'c'`)
- Drop column entry (`'C'`)

Data operations on a single table are grouped together, preceded by a single
table record. The operations are processed as if they were executed
sequentially.

## Table record

The table record identifies the table and its columns:

- 1 byte: Constant 0x54 (capital 'T')
- Varint: Number of columns in the table.
- nCol bytes: 0x01 for PK columns, 0x00 otherwise.
- N bytes: Unqualified table name (encoded using UTF-8). Null-terminated.

## Data entry

A data entry is a DELETE, UPDATE or INSERT operation on one table (identified
by last table record):

- 1 byte: Either SQLITE_INSERT (0x12), UPDATE (0x17) or DELETE (0x09).
- 1 byte: The "indirect-change" flag.
- old.* record: (delete and update only)
- new.* record: (insert and update only)

The "old.*" and "new.*" records, if present, are N field records in the
format described above under "RECORD FORMAT", where N is the number of
columns in the table. The i'th field of each record is associated with
the i'th column of the table, counting from left to right in the order
in which columns were declared in the CREATE TABLE statement.

The new.* record that is part of each INSERT change contains the values
that make up the new row. Similarly, the old.* record that is part of each
DELETE change contains the values that made up the row that was deleted
from the database. In the changeset format, the records that are part
of INSERT or DELETE changes never contain any undefined (type byte 0x00)
fields.

Within the old.* record associated with an UPDATE change, all fields
associated with table columns that are not PRIMARY KEY columns and are
not modified by the UPDATE change are set to "undefined". Other fields
are set to the values that made up the row before the UPDATE that the
change records took place. Within the new.* record, fields associated
with table columns modified by the UPDATE change contain the new
values. Fields associated with table columns that are not modified
are set to "undefined".

## Create table entry

This entry creates a new empty table:

- 1 byte: Constant 0x61 (lowercase 'a')
- Null-terminated string: Table name
- Varint: Number of columns in the table.
- nCol entries: Table column info.

## Drop table entry

This entry deletes an existing table by name. The table must be empty. Column
information is kept for the purpose of rebasing and inverting the changeset.

- 1 byte: Constant 0x41 (uppercase 'A')
- Null-terminated string: Table name
- Varint: Number of columns in the table.
- nCol entries: Table column info.

## Add column entry

This entry adds a new column to an existing table. All existing rows will have
`NULL` filled in.

- 1 byte: Constant 0x63 (lowercase 'c')
- Null-terminated string: Table name
- Table column info.

## Drop column entry

This entry deletes an existing column from a table. All existing rows must have
`NULL` values in this column. Column information is kept for the purpose of
rebasing and inverting the changeset.

- 1 byte: Constant 0x43 (uppercase 'C')
- Null-terminated string: Table name
- Table column info.

# Record Format

Unlike the SQLite database record format, each field is self-contained -
there is no separation of header and data. Each field begins with a
single byte describing its type, as follows:

       0x00: Undefined value.
       0x01: Integer value.
       0x02: Real value.
       0x03: Text value.
       0x04: Blob value.
       0x05: SQL NULL value.

Note that the above match the definitions of SQLITE_INTEGER, SQLITE_TEXT
and so on in sqlite3.h. For undefined and NULL values, the field consists
only of the single type byte. For other types of values, the type byte
is followed by:

- Text values:
  A varint containing the number of bytes in the value (encoded using
  UTF-8). Followed by a buffer containing the UTF-8 representation
  of the text value. There is no null terminator.

- Blob values:
  A varint containing the number of bytes in the value, followed by
  a buffer containing the value itself.

- Integer values:
  An 8-byte big-endian integer value.

- Real values:
  An 8-byte big-endian IEEE 754-2008 real value.


# Table column info

- Null-terminated string: column name
- 1 byte: Column type (same as record)
- 1 byte: Flags packed as bits. From LSb:
  - is primary key
  - is autoincrement
  - is geometry column
  - geometry has Z coordinate
  - geometry has M coordinate
- Null-terminated string: geometry type (`POINT`, `LINE`, ...)
- Varint: SRS ID for geometry

# Varint Format

Varint values are encoded in the same way as varints in the SQLite
record format.

The variable-length integer encoding is as follows:

```
 KEY:
         A = 0xxxxxxx    7 bits of data and one flag bit
         B = 1xxxxxxx    7 bits of data and one flag bit
         C = xxxxxxxx    8 bits of data

  7 bits - A
 14 bits - BA
 21 bits - BBA
 28 bits - BBBA
 35 bits - BBBBA
 42 bits - BBBBBA
 49 bits - BBBBBBA
 56 bits - BBBBBBBA
 64 bits - BBBBBBBBC
```
