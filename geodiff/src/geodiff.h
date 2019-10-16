/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFF_H
#define GEODIFF_H

#if defined _WIN32 || defined __CYGWIN__
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
* NOTE:
* the messages printed to stdout can be controlled by
* environment variable GEODIFF_LOGGER_LEVEL
* GEODIFF_LOGGER_LEVEL = 0 nothing is printed
* GEODIFF_LOGGER_LEVEL = 1 errors are printed
* GEODIFF_LOGGER_LEVEL = 2 errors and warnings are printed
* GEODIFF_LOGGER_LEVEL = 3 errors, warnings and infos are printed
* GEODIFF_LOGGER_LEVEL = 4 errors, warnings, infos, debug messages are printed
*/

/**
 * Initialize library
 * Call before usage of any other function from the library
 */
GEODIFF_EXPORT void GEODIFF_init();

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
 * \returns GEODIFF_SUCCESS on success
 */
GEODIFF_EXPORT int GEODIFF_createRebasedChangeset(
  const char *base,
  const char *modified,
  const char *changeset_their,
  const char *changeset );

/**
 * Applies changeset file (binary) to BASE
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
