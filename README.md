# Sqlite module for node and backendjs

# Usage

```javascript
  var sqlite = require("bkjs-sqlite");

  var db = new sqlite.Database("test.db", sqlite.SQLITE_OPEN_CREATE, function(err) {
     if (err) console.log(err);

     this.runSync("PRAGMA cache_size=5000");
     this.runSync("CREATE TABLE test(a int, b text)")

     this.query("SELECT name FROM sqlite_master WHERE type=?", ["table"], function(err, tables) {
        console.log(err, tables);
     });
  });
```

## Database class
- `new Database(filename, options, callback)` - create new database object,
  the callback will be called with an Error if occured.
- Properties:
  - `open` - return 1 if the db is open
  - `affected_rows` - returns number of rows affected by the last operation
  - `inserted_oid` - last auto generated ID
- Methods:
  - `exec(sql[, callback])` - execute the SQL statememt in a worker thread
  - `run(sql, [values], [callback])` - execute a DDL statement in a worker thread, supports
     parameters in the statement
  - `runSync(sql, [values])` - execute a DDL statement synchronously
  - `query(sql, [values], [callback])` - execute any SQL statement in a worker thread, if a callback
     is given it will be passed an array with result if exists, otherwise empty array
  - `querySync(sql, [values])` - execute a SQL statement synchronously, returns array with result
  - `close([callback])` - close the database in a worker thread
  - `closeSync()` - close the database in the main thread
  - `copy(db2)` - copy currently open database into another, db2 can be an open db object or a file name

## Statement class
- `new Statement(db, sql, callback)` - create new SQL statement object for a database and SQL statement, a callback
   will be called with an error if occured, otherwise prepared statement is ready for execution
- Methods:
  - `prepare(sql, [callback])` - prepare another SQL statement in the existing statement object
  - `run([callback])` - execute prepared DDL statement in a worker thread
  - `runSync()` - execute prepared DDL statememnt in the main thread
  - `query([values], [callback])` - execute prepared statement with values for the parameters, if callback is given it will be passed the results
  - `querySync([values])` - execute prepared statement with values for the parameters in the main thread
  - `each([values],onrow, oncomplete)` - start iterating over the results for the prepared statement, onrow callback
    will be called for each row, to move to the next row `this.next()` must be called. Once all rows are processed and oncomplete
    callback is given it will be called. This is for going over huge amounts of records, each step will retrieve only one record
    as opposed to the `query` which will retrieve all records in the memory.
  - `finalize()` - close and free the statement, it cannot be used anymore and will be deleted eventually

# Author

Vlad Seryakov

