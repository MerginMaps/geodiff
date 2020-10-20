/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFF_H
#define GEODIFF_H

#if defined _WIN32 || defined __CYGWIN__
#define UNICODE
#  ifdef geodiff_EXPORT
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

/**
 * Initialize library
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
GEODIFF_EXPORT void GEODIFF_init();

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
GEODIFF_EXPORT void GEODIFF_setLoggerCallback( GEODIFF_LoggerCallback loggerCallback );

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
GEODIFF_EXPORT void GEODIFF_setMaximumLoggerLevel( GEODIFF_LoggerLevel maxLogLevel );


//! Returns version in format X.Y.Z where xyz are positive integers
GEODIFF_EXPORT const char *GEODIFF_version();

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
GEODIFF_EXPORT int GEODIFF_invertChangeset( const char *changeset, const char *changeset_inv );

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
  const char *base,
  const char *changeset );


/**
 * \returns -1 on error, 0 no changes, 1 has changes
 */
GEODIFF_EXPORT int GEODIFF_hasChanges( const char *changeset );

/**
 * \returns number of changes, -1 on error
 */
GEODIFF_EXPORT int GEODIFF_changesCount( const char *changeset );

/**
 * Expand changeset to JSON
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_listChanges(
  const char *changeset,
  const char *jsonfile
);

/**
 * Export summary of changeset to JSON
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_listChangesSummary(
  const char *changeset,
  const char *jsonfile
);

#ifdef __cplusplus
}
#endif

#endif // GEODIFF_H
