
# Changeset Format

The format for changesets is borrowed from SQLite3 session extension's internal format
and it is currently 100% compatible with it. Below are details of the format, extracted
from SQLite3 source code.

## Summary

A changeset is a collection of DELETE, UPDATE and INSERT operations on
one or more tables. Operations on a single table are grouped together,
but may occur in any order (i.e. deletes, updates and inserts are all
mixed together).

Each group of changes begins with a table header:

- 1 byte: Constant 0x54 (capital 'T')
- Varint: Number of columns in the table.
- nCol bytes: 0x01 for PK columns, 0x00 otherwise.
- N bytes: Unqualified table name (encoded using UTF-8). Nul-terminated.

Followed by one or more changes to the table.

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
  of the text value. There is no nul terminator.

- Blob values:
  A varint containing the number of bytes in the value, followed by
  a buffer containing the value itself.

- Integer values:
  An 8-byte big-endian integer value.

- Real values:
  An 8-byte big-endian IEEE 754-2008 real value.


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
