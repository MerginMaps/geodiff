/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFUTILS_H
#define GEODIFFUTILS_H

#include <string>
#include <memory>
#include <exception>

#include "sqlite3.h"

class Buffer;

class GeoDiffException: public std::exception
{
  public:
    GeoDiffException( const std::string &msg );
    virtual const char *what() const throw();
  private:
    std::string mMsg;
};

/**
 * Logger
 *
 * the messages printed to stdout can be controlled by
 * environment variable GEODIFF_LOGGER_LEVEL
 * GEODIFF_LOGGER_LEVEL = 0 nothing is printed
 * GEODIFF_LOGGER_LEVEL = 1 errors are printed
 * GEODIFF_LOGGER_LEVEL = 2 errors and warnings are printed
 * GEODIFF_LOGGER_LEVEL = 3 errors, warnings and infos are printed
 * GEODIFF_LOGGER_LEVEL = 4 errors, warnings, infos, debug messages are printed
 */
class Logger
{
  public:
    enum LoggerLevel
    {
      LevelNothing = 0,
      LevelErrors = 1,
      LevelWarnings = 2,
      LevelInfos = 3,
      LevelDebug = 4
    };

    static Logger &instance();
    LoggerLevel level() const;
    Logger( Logger const & ) = delete;
    void operator=( Logger const & ) = delete;
    void debug( const std::string &msg );
    void warn( const std::string &msg );
    void error( const std::string &msg );
    void info( const std::string &msg );
    //! Prints error message
    void error( const GeoDiffException &exp );
  private:
    Logger();
    void log( LoggerLevel level, const std::string &msg );
    void levelFromEnv();

    LoggerLevel mLevel = LevelErrors; //by default record errors
};

//! copy file from to location. override if exists
void filecopy( const std::string &to, const std::string &from );

//! remove a file if exists
void fileremove( const std::string &path );

//! whether file exists
bool fileexists( const std::string &path );

#endif // GEODIFFUTILS_H
