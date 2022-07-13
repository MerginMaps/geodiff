/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFF_H
#define GEODIFF_H

#include <stdint.h>
#include <stdio.h>

#ifdef GEODIFF_STATIC
#  define GEODIFF_EXPORT
#else
#if defined _WIN32 || defined __CYGWIN__
#define UNICODE
#  ifdef geodiff_EXPORTS
#    ifdef __GNUC__
#      define GEODIFF_EXPORT __attribute__ ((dllexport))
#    else
#      define GEODIFF_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#    endif
#  else
#    ifdef __GNUC__
#      define GEODIFF_EXPORT __attribute__ ((dllimport))
#    else
#      define GEODIFF_EXPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#    endif
#  endif
#else
#  if __GNUC__ >= 4
#    define GEODIFF_EXPORT __attribute__ ((visibility ("default")))
#  else
#    define GEODIFF_EXPORT
#  endif
#endif
#endif

#define GEODIFF_SUCCESS 0 //!< Success
#define GEODIFF_ERROR 1 //!< General error
#define GEODIFF_CONFICTS 2 //!< The changeset couldn't be applied directly
#define GEODIFF_UNSUPPORTED_CHANGE 3 //! The diff for such entry is unsupported/not-implemented

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

typedef void *GEODIFF_ContextH;

/**
 * Initialize session context
 *
 * The default logger is set, where all the messages printed are printed to stdout/stderr
 * and can be controlled by environment variable GEODIFF_LOGGER_LEVEL
 * GEODIFF_LOGGER_LEVEL = 0 nothing is printed
 * GEODIFF_LOGGER_LEVEL = 1 errors are printed
 * GEODIFF_LOGGER_LEVEL = 2 errors and warnings are printed
 * GEODIFF_LOGGER_LEVEL = 3 errors, warnings and infos are printed
 * GEODIFF_LOGGER_LEVEL = 4 errors, warnings, infos, debug messages are printed
 *
 * Default logger level is 1 (errors are printed)
 */
GEODIFF_EXPORT GEODIFF_ContextH GEODIFF_createContext();

GEODIFF_EXPORT void GEODIFF_CX_destroy( GEODIFF_ContextH contextHandle );

/**
* Type of message level to log
*/
enum GEODIFF_LoggerLevel
{
  LevelError = 1,
  LevelWarning = 2,
  LevelInfo = 3,
  LevelDebug = 4
};

/**
 * Callback function pointer to redirect log
 */
typedef void ( *GEODIFF_LoggerCallback )( GEODIFF_LoggerLevel level, const char *msg );

/**
 * Assign custom logger
 *
 * Replace default stdout/stderr logger with custom.
 * When loggerCallback is nullptr, no output is produced at all
 */
GEODIFF_EXPORT int GEODIFF_CX_setLoggerCallback( GEODIFF_ContextH contextHandle, GEODIFF_LoggerCallback loggerCallback );

/**
 * Assign maximum level of messages that are passed to logger callback
 *
 * Based on maxLogLevel, the messages are filtered by level:
 * maxLogLevel = 0 nothing is passed to logger callback
 * maxLogLevel = 1 errors are passed to logger callback
 * maxLogLevel = 2 errors and warnings are passed to logger callback
 * maxLogLevel = 3 errors, warnings and infos are passed to logger callback
 * maxLogLevel = 4 errors, warnings, infos, debug messages are passed to logger callback
 */
GEODIFF_EXPORT int GEODIFF_CX_setMaximumLoggerLevel( GEODIFF_ContextH contextHandle, GEODIFF_LoggerLevel maxLogLevel );

/**
 * Set list of tables to exclude from geodiff operations. Once defined, these tables
 * will be excluded from the following operations: create changeset, apply changeset,
 * rebase, get database schema, dump database contents, copy database between different
 * drivers.
 *
 * If empty list is passed, list will be reset.
 */
GEODIFF_EXPORT int GEODIFF_CX_setTablesToSkip( GEODIFF_ContextH contextHandle, int tablesCount, const char **tablesToSkip );


//! Returns version in format X.Y.Z where xyz are positive integers
GEODIFF_EXPORT const char *GEODIFF_version();

/**
 * Returns count of available/registered drivers
 */
GEODIFF_EXPORT int GEODIFF_driverCount( GEODIFF_ContextH context );

/**
 * Returns driver name by index
 * driverName should be allocated to 256 chars, will be populated with driver name
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_driverNameFromIndex( GEODIFF_ContextH contextHandle, int index, char *driverName );

/**
 * Returns whether the driver is available/registered
 */
GEODIFF_EXPORT bool GEODIFF_driverIsRegistered( GEODIFF_ContextH contextHandle, const char *driverName );

/**
 * Creates changeset file (binary) in such way that
 * if CHANGESET is applied to BASE by applyChangeset,
 * MODIFIED will be created
 *
 * BASE --- CHANGESET ---> MODIFIED
 *
 * \param base [input] BASE sqlite3/geopackage file
 * \param modified [input] MODIFIED sqlite3/geopackage file
 * \param changeset [output] changeset between BASE -> MODIFIED
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_createChangeset(
  GEODIFF_ContextH contextHandle,
  const char *base,
  const char *modified,
  const char *changeset );

/**
 * Inverts changeset file (binary) in such way that
 * if CHANGESET_INV is applied to MODIFIED by applyChangeset,
 * BASE will be created
 *
 * BASE --- CHANGESET ---> MODIFIED
 * MODIFIED --- CHANGESET_INV ---> BASE
 *
 * \param changeset [input] changeset between BASE -> MODIFIED
 * \param changeset_inv [output] changeset between MODIFIED -> BASE
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_invertChangeset(
  GEODIFF_ContextH contextHandle,
  const char *changeset,
  const char *changeset_inv );

/**
 * Creates changeset file (binary) in such way that
 * if CHANGESET is applied to MODIFIED_THEIR by
 * applyChangeset, the new state will contain all
 * changes from MODIFIED and MODIFIED_THEIR.
 *
 *        --- CHANGESET_THEIR ---> MODIFIED_THEIR --- CHANGESET ---> MODIFIED_THEIR_PLUS_MINE
 * BASE -|
 *        -----------------------> MODIFIED
 *
 * \param base [input] BASE sqlite3/geopackage file
 * \param modified [input] MODIFIED sqlite3/geopackage file
 * \param changeset_their [input] changeset between BASE -> MODIFIED_THEIR
 * \param changeset [output] changeset between MODIFIED_THEIR -> MODIFIED_THEIR_PLUS_MINE
 * \param conflictfile [output] json file containing all the automaticly resolved conflicts. If there are no conflicts, file is not created
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_createRebasedChangeset(
  GEODIFF_ContextH contextHandle,
  const char *base,
  const char *modified,
  const char *changeset_their,
  const char *changeset,
  const char *conflictfile
);


/**
 * Rebases local modified version from base to modified_their version
 *
 *        --- > MODIFIED_THEIR
 * BASE -|
 *        ----> MODIFIED (local) ---> MODIFIED_THEIR_PLUS_MINE
 *
 * Steps performed on MODIFIED (local) file:
 *    1. undo local changes MODIFIED -> BASE
 *    2. apply changes from MODIFIED_THEIR
 *    3. apply rebased local changes and create MODIFIED_THEIR_PLUS_MINE
 *
 * Note, when rebase is not successfull, modified could be in random state.
 * This works in general, even when base==modified, or base==modified_theirs
 *
 * \param base [input] BASE sqlite3/geopackage file
 * \param modified_their [input] MODIFIED sqlite3/geopackage file
 * \param modified [input/output] local copy of the changes to be rebased
 * \param conflictfile [output] json file containing all the automaticly resolved conflicts. If there are no conflicts, file is not created
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_rebase(
  GEODIFF_ContextH contextHandle,
  const char *base,
  const char *modified_their,
  const char *modified,
  const char *conflictfile
);


/**
 * Applies changeset file (binary) to BASE
 *
 * When changeset is correctly formed (for example after successful rebase),
 * the applyChanges should not raise any conflict. The GEODIFF_CONFLICTS error
 * suggests that the base or changeset are not matching each other (e.g. changeset
 * created from different base file) or can suggest an internal bug in rebase routine.
 * With WARN logging level client should be able to see the place of conflicts.
 *
 * \param base [input/output] BASE sqlite3/geopackage file
 * \param changeset [input] changeset to apply to BASE
 * \returns GEODIFF_SUCCESS on success
 *          GEODIFF_CONFICTS if the changeset was applied but conflicts were found
 */
GEODIFF_EXPORT int GEODIFF_applyChangeset(
  GEODIFF_ContextH contextHandle,
  const char *base,
  const char *changeset );


/**
 * \returns -1 on error, 0 no changes, 1 has changes
 */
GEODIFF_EXPORT int GEODIFF_hasChanges(
  GEODIFF_ContextH contextHandle,
  const char *changeset );

/**
 * \returns number of changes, -1 on error
 */
GEODIFF_EXPORT int GEODIFF_changesCount(
  GEODIFF_ContextH contextHandle,
  const char *changeset );

/**
 * Expand changeset to JSON
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_listChanges(
  GEODIFF_ContextH contextHandle,
  const char *changeset,
  const char *jsonfile
);

/**
 * Export summary of changeset to JSON
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_listChangesSummary(
  GEODIFF_ContextH contextHandle,
  const char *changeset,
  const char *jsonfile
);

/**
 * Combine multiple changeset files into a single changeset file. When the output changeset
 * is applied to a database, the result should be the same as if the input changesets were applied
 * one by one. The order of input files is important. At least two input files need to be
 * provided.
 *
 * Incompatible changes (which would cause conflicts when applied) will be discarded.
 *
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_concatChanges(
  GEODIFF_ContextH contextHandle,
  int inputChangesetsCount,
  const char **inputChangesets,
  const char *outputChangeset
);

/**
 * Makes a copy of the source dataset (a collection of tables) to the specified destination.
 *
 * This will open the source dataset, get list of tables, their structure, dump data
 * to a temporary changeset file. Then it will create the destination dataset, create tables
 * and insert data from changeset file.
 *
 * Supported drivers:
 *
 * - "sqlite" - does not need extra connection info (may be null). A dataset is a single Sqlite3
 *   database (a GeoPackage) - a path to a local file is expected.
 *
 * - "postgres" - only available if compiled with postgres support. Needs extra connection info
 *   argument which is passed to libpq's PQconnectdb(), see PostgreSQL docs for syntax.
 *   A datasource identifies a PostgreSQL schema name (namespace) within the current database.
 *
 */
GEODIFF_EXPORT int GEODIFF_makeCopy(
  GEODIFF_ContextH contextHandle,
  const char *driverSrcName,
  const char *driverSrcExtraInfo,
  const char *src,
  const char *driverDstName,
  const char *driverDstExtraInfo,
  const char *dst );

/**
 * Makes a copy of a SQLite database. If the destination database file exists, it will be overwritten.
 *
 * This is the preferred way of copying SQLite/GeoPackage files compared to just using raw copying
 * of files on the file system: it will take into account other readers/writers and WAL file,
 * so we should never end up with a corrupt copy.
 */
GEODIFF_EXPORT int GEODIFF_makeCopySqlite(
  GEODIFF_ContextH contextHandle,
  const char *src,
  const char *dst );

/**
 * This is an extended version of GEODIFF_createChangeset() which also allows specification
 * of the driver and its extra connection info. The original GEODIFF_createChangeset() function
 * only supports Sqlite driver.
 *
 * See documentation of GEODIFF_makeCopy() for details about supported drivers.
 */
GEODIFF_EXPORT int GEODIFF_createChangesetEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *base,
  const char *modified,
  const char *changeset );

/**
 * Compares 2 sources from various drivers by creating changeset. Sources are converted to geopackage and then compared via
 * GEODIFF_createChangesetEx() function.
 *
 * See documentation of GEODIFF_makeCopy() for details about supported drivers.
 */
GEODIFF_EXPORT int GEODIFF_createChangesetDr(
  GEODIFF_ContextH contextHandle,
  const char *driverSrcName,
  const char *driverSrcExtraInfo,
  const char *src,
  const char *driverDstName,
  const char *driverDstExtraInfo,
  const char *dst,
  const char *changeset );


/**
 * This is an extended version of GEODIFF_applyChangeset() which also allows specification
 * of the driver and its extra connection info. The original GEODIFF_applyChangeset() function
 * only supports Sqlite driver.
 *
 * See documentation of GEODIFF_makeCopy() for details about supported drivers.
 */
GEODIFF_EXPORT int GEODIFF_applyChangesetEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *base,
  const char *changeset );


/**
 * This function takes an existing changeset "base2modified" and rebases it on top of changes in
 * "base2their" and writes output to a new changeset "rebased"
 */
GEODIFF_EXPORT int GEODIFF_createRebasedChangesetEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *base,
  const char *base2modified,
  const char *base2their,
  const char *rebased,
  const char *conflictfile );


/**
 * This function takes care of updating "modified" dataset by taking any changes between "base"
 * and "modified" datasets and rebasing them on top of base2their changeset.
 */
GEODIFF_EXPORT int GEODIFF_rebaseEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *base,
  const char *modified,
  const char *base2their,
  const char *conflictfile );


/**
 * Dumps all data from the data source as INSERT statements to a new changeset file.
 */
GEODIFF_EXPORT int GEODIFF_dumpData(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *src,
  const char *changeset );

/**
 * Writes a JSON file containing database schema of tables as understood by geodiff.
 */
GEODIFF_EXPORT int GEODIFF_schema(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *src,
  const char *json );


typedef void *GEODIFF_ChangesetReaderH;
typedef void *GEODIFF_ChangesetEntryH;
typedef void *GEODIFF_ChangesetTableH;
typedef void *GEODIFF_ValueH;

/**
 * Tries to open a changeset file and returns a new changeset reader object if successful,
 * or null if it failed (the file does not exist, unknown format, ...) The ownership
 * of the returned object is passed to the caller - GEODIFF_CR_destroy() should be called
 * when the reader is not needed anymore.
 */
GEODIFF_EXPORT GEODIFF_ChangesetReaderH GEODIFF_readChangeset(
  GEODIFF_ContextH contextHandle,
  const char *changeset );

//
// ChangesetReader-related functions
//

/**
 * Returns the next entry from the changeset reader, or null if there are no more entries.
 * The ownership of the returned entry is passed to the caller - GEODIFF_CE_destroy() should
 * be called when the entry is not needed anymore.
 *
 * If an exception has occurred (e.g. bad file content), the passed "ok" variable will be set
 * to false. Normally it will be set to true (even we have reached the end of the file).
 */
GEODIFF_EXPORT GEODIFF_ChangesetEntryH GEODIFF_CR_nextEntry(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetReaderH readerHandle,
  bool *ok );

/**
 * Deletes an existing changeset reader object and frees any resources related to it.
 */
GEODIFF_EXPORT void GEODIFF_CR_destroy(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetReaderH readerHandle );

//
// ChangesetEntry-related functions
//

/**
 * Reads entry's operation type - whether it is an insert, update or delete.
 */
GEODIFF_EXPORT int GEODIFF_CE_operation(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetEntryH entryHandle );

/**
 * Returns table-related information object of the entry. The returned object is owned
 * by geodiff and does not need to be deleted by the caller. It is only valid while
 * the changeset entry is not deleted.
 */
GEODIFF_EXPORT GEODIFF_ChangesetTableH GEODIFF_CE_table(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetEntryH entryHandle );

/**
 * Returns number of items in the list of old/new values.
 */
GEODIFF_EXPORT int GEODIFF_CE_countValues(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetEntryH entryHandle );

/**
 * Returns old value of an entry (only valid for UPDATE and DELETE).
 * The ownership of the value object is passed to the caller - GEODIFF_V_destroy()
 * should be called when the value object is not needed anymore.
 */
GEODIFF_EXPORT GEODIFF_ValueH GEODIFF_CE_oldValue(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetEntryH entryHandle,
  int i );

/**
 * Returns new value of an entry (only valid for UPDATE and INSERT).
 * The ownership of the value object is passed to the caller - GEODIFF_V_destroy()
 * should be called when the value object is not needed anymore.
 */
GEODIFF_EXPORT GEODIFF_ValueH GEODIFF_CE_newValue(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetEntryH entryHandle,
  int i );

/**
 * Deletes an existing changeset entry object and frees any resources related to it.
 */
GEODIFF_EXPORT void GEODIFF_CE_destroy(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetEntryH entryHandle );

//
// ChangesetTable-related functions
//

/**
 * Returns name of the table. Ownership of the returned pointer is NOT passed to the caller
 * and should not be modified or freed.
 */
GEODIFF_EXPORT const char *GEODIFF_CT_name(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetTableH tableHandle );

/**
 * Returns number of columns in the table.
 */
GEODIFF_EXPORT int GEODIFF_CT_columnCount(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetTableH tableHandle );

/**
 * Returns whether column at the given index is a part of the table's primary key.
 */
GEODIFF_EXPORT bool GEODIFF_CT_columnIsPkey(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ChangesetTableH tableHandle,
  int i );


//
// Value-related functions
//

/**
 * Returns type of the value stored in the object
 */
GEODIFF_EXPORT int GEODIFF_V_type(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ValueH valueHandle );

/**
 * Returns integer value (if type is not TypeInt, the result is undefined)
 */
GEODIFF_EXPORT int64_t GEODIFF_V_getInt(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ValueH valueHandle );

/**
 * Returns double value (if type is not TypeDouble, the result is undefined)
 */
GEODIFF_EXPORT double GEODIFF_V_getDouble(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ValueH valueHandle );

/**
 * Returns number of bytes of text/blob value (if type is not TypeText or TypeBlob, the result is undefined)
 */
GEODIFF_EXPORT int GEODIFF_V_getDataSize(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ValueH valueHandle );

/**
 * Copies data of the text/blob value to given buffer (if type is not TypeText or TypeBlob, the result is undefined)
 */
GEODIFF_EXPORT void GEODIFF_V_getData(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ValueH valueHandle,
  char *data );

/**
 * Deletes an existing value object and frees any resources related to it.
 */
GEODIFF_EXPORT void GEODIFF_V_destroy(
  GEODIFF_ContextH contextHandle,
  GEODIFF_ValueH valueHandle );


/**
 * Extracts WKB geometry from the geometry encoded according to GeoPackage spec.
 * Strips GeoPackageBinaryHeader from the StandardGeoPackageBinary envelope and
 * returns geometry as WKB. More details at https://www.geopackage.org/spec/#gpb_format
 *
 * gpkgWkb is a geometry encoded according to GeoPackage spec
 * gpkgLength is a length of the passed geometry buffer
 * wkb is an output wkb, points to the gpkgWkb memory. No allocation/deallocation required.
 * wkbLength is a length of the returned wkb buffer
 *
 * returns GEODIFF_SUCCESS on success, GEOSIFF_ERROR on failure
 */
GEODIFF_EXPORT int GEODIFF_createWkbFromGpkgHeader(
  GEODIFF_ContextH contextHandle,
  const char *gpkgWkb,
  size_t gpkgLength,
  const char **wkb,
  size_t *wkbLength );


#ifdef __cplusplus
}
#endif

#endif // GEODIFF_H
