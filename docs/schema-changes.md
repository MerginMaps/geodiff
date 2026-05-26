# Schema changes

Geodiff supports diffing databases with different schemata. It identifies table
and column additions/deletions.

Tables and columns are always created empty and any data present in the
database is recreated manually via `INSERT`/`UPDATE` entries, written after the
schema change entry. Likewise, deletion entries expect the table/column to be
empty, so `DELETE`/`UPDATE` entries clearing the data are written beforehand.
This simplifies inverting and rebasing, since the schema change entries work
separately from e.g. the ID renaming machinery.

## Limitations and pitfalls

Since we only look at the final state of the database, default values in
columns are not supported. Any default specified during creation of the column
will be simulated by an `UPDATE` for each row. This means that only the rows
present in the modified database will get the "default" value, and the default
won't be propagated when the diff is applied onto base.

Renaming columns is supported only as a deletion & addition. This has similar
pitfalls to the default values - on rebase, values in the second database won't
be moved. Same with renaming tables.

The intermediate states created by applying the resulting diff (e.g. "nulling
out" column before dropping it) may conflict with database constraints.
