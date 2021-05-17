/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESET_H
#define CHANGESET_H

#include <assert.h>
#include <memory>
#include <string>
#include <vector>


/**
 * Representation of a single value stored in a column.
 * It can be one of types:
 * - NULL
 * - integer
 * - double
 * - string
 * - binary data (blob)
 *
 * There is also a special "undefined" value type which is different
 * from "null". The "undefined" value means that the particular value
 * has not changed, for example in UPDATE change if a column's value
 * is unchanged, its value will have this type.
 */
struct Value
{
    Value() {}
    ~Value() { reset(); }

    Value( const Value &other )
    {
      *this = other;
    }

    Value &operator=( const Value &other )
    {
      reset();
      mType = other.mType;
      mVal = other.mVal;
      if ( mType == TypeText || mType == TypeBlob )
      {
        mVal.str = new std::string( *mVal.str ); // make a deep copy
      }
      return *this;
    }

    bool operator==( const Value &other ) const
    {
      if ( mType != other.mType )
        return false;
      if ( mType == TypeUndefined || mType == TypeNull )
        return true;
      if ( mType == TypeInt )
        return getInt() == other.getInt();
      if ( mType == TypeDouble )
        return getDouble() == other.getDouble();
      if ( mType == TypeText || mType == TypeBlob )
        return getString() == other.getString();

      assert( false );
    }

    bool operator!=( const Value &other ) const
    {
      return !( *this == other );
    }

    //! Possible value types
    enum Type
    {
      TypeUndefined = 0,   //!< equal to "undefined" value type in sqlite3 session extension
      TypeInt       = 1,   //!< equal to SQLITE_INTEGER
      TypeDouble    = 2,   //!< equal to SQLITE_FLOAT
      TypeText      = 3,   //!< equal to SQLITE_TEXT
      TypeBlob      = 4,   //!< equal to SQLITE_BLOB
      TypeNull      = 5,   //!< equal to SQLITE_NULL
    };

    Type type() const { return mType; }

    int64_t getInt() const
    {
      assert( mType == TypeInt );
      return mVal.num_i;
    }
    double getDouble() const
    {
      assert( mType == TypeDouble );
      return mVal.num_f;
    }
    const std::string &getString() const
    {
      assert( mType == TypeText || mType == TypeBlob );
      return *mVal.str;
    }

    void setInt( int64_t n )
    {
      reset();
      mType = TypeInt;
      mVal.num_i = n;
    }
    void setDouble( double n )
    {
      reset();
      mType = TypeDouble;
      mVal.num_f = n;
    }
    void setString( Type t, const char *ptr, int size )
    {
      reset();
      assert( t == TypeText || t == TypeBlob );
      mType = t;
      mVal.str = new std::string( ptr, size );
    }
    void setUndefined()
    {
      reset();
    }
    void setNull()
    {
      reset();
      mType = TypeNull;
    }

    static Value makeInt( int64_t n ) { Value v; v.setInt( n ); return v; }
    static Value makeDouble( double n ) { Value v; v.setDouble( n ); return v; }
    static Value makeText( std::string s ) { Value v; v.setString( TypeText, s.data(), s.size() ); return v; }
    static Value makeNull() { Value v; v.setNull(); return v; }

  protected:
    void reset()
    {
      if ( mType == TypeText || mType == TypeBlob )
      {
        delete mVal.str;
      }
      mType = TypeUndefined;
    }

  protected:
    Type mType = TypeUndefined;
    union
    {
      int64_t num_i;
      double num_f;
      std::string *str;
    } mVal;

};


//! std::hash<Value> implementation
namespace std
{
  template<> struct hash<Value>
  {
    std::size_t operator()( const Value &v ) const
    {
      switch ( v.type() )
      {
        case Value::TypeUndefined:
          return 0xcccccccc;
        case Value::TypeInt:
          return std::hash<int64_t> {}( v.getInt() );
        case Value::TypeDouble:
          return std::hash<double> {}( v.getDouble() );
        case Value::TypeText:
        case Value::TypeBlob:
          return std::hash<std::string> {}( v.getString() );
        case Value::TypeNull:
          return 0xdddddddd;
      }
      assert( false );
      return 0;
    }
  };
}


/**
 * Table metadata stored in changeset file
 */
struct ChangesetTable
{
  //! Name of the table
  std::string name;
  //! Array of true/false values (one for each column) - indicating whether particular column is a part of primary key
  std::vector<bool> primaryKeys;

  //! Returns number of columns
  size_t columnCount() const { return primaryKeys.size(); }
};


/**
 * Details of a single change within a changeset
 *
 * Contents of old/new values array based on operation type:
 * - INSERT - new values contain data of the row to be inserted, old values array is invalid
 * - DELETE - old values contain data of the row to be deleted, new values array is invalid
 * - UPDATE - both old and new values arrays are valid, if a column has not changed, both
 *            old and new value have "undefined" value type. In addition to that, primary key
 *            columns of old value are always present (but new value of pkey columns is undefined
 *            if the primary key is not being changed).
 */
struct ChangesetEntry
{
  enum OperationType
  {
    OpInsert = 18,  //!< equal to SQLITE_INSERT
    OpUpdate = 23,  //!< equal to SQLITE_UPDATE
    OpDelete = 9,   //!< equal to SQLITE_DELETE
  };

  //! Type of the operation in this entry
  OperationType op;
  //! Column values for "old" record - only valid for UPDATE and DELETE
  std::vector<Value> oldValues;
  //! Column values for "new" record - only valid for UPDATE and INSERT
  std::vector<Value> newValues;
  /**
   * Optional pointer to the source table information as stored in changeset.
   *
   * When the changeset entry has been read by ChangesetReader, the table always will be set to a valid
   * instance. Do not delete the instance - it is owned by ChangesetReader.
   *
   * When the changeset entry is being passed to ChangesetWriter, the table pointer is ignored
   * and it does not need to be set (writer has an explicit beginTable() call to set table).
   */
  ChangesetTable *table = nullptr;

  //! a quick way for tests to create a changeset entry
  static ChangesetEntry make( ChangesetTable *t, OperationType o, std::vector<Value> oldV, std::vector<Value> newV )
  {
    ChangesetEntry e;
    e.op = o;
    e.oldValues = oldV;
    e.newValues = newV;
    e.table = t;
    return e;
  }
};

#endif // CHANGESET_H
