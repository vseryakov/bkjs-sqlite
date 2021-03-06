//
//  Author: Vlad Seryakov vseryakov@gmail.com
//  April 2013
//

#include "bkjs.h"
#include "bksqlite.h"

#define EXCEPTION(msg, errno, name) \
        Local<Value> name = Exception::Error(Nan::New(bkFmtStr("%d: %s", errno, msg)).ToLocalChecked()); \
        Local<Object> name ##_obj = Nan::To<Object>(name).ToLocalChecked(); \
        Nan::Set(name ##_obj, Nan::New("errno").ToLocalChecked(), Nan::New(errno)); \
        Nan::Set(name ##_obj, Nan::New("code").ToLocalChecked(), Nan::New(sqlite_code_string(errno)).ToLocalChecked());

struct SQLiteField {
    inline SQLiteField(unsigned short _index, unsigned short _type = SQLITE_NULL, double n = 0, string s = string()): type(_type), index(_index), nvalue(n), svalue(s) {}
    inline SQLiteField(const char *_name, unsigned short _type = SQLITE_NULL, double n = 0, string s = string()): type(_type), index(0), name(_name), nvalue(n), svalue(s) {}
    unsigned short type;
    unsigned short index;
    string name;
    double nvalue;
    string svalue;
};

typedef vector<SQLiteField> Row;
class SQLiteStatement;

static map<SQLiteStatement*,bool> _stmts;

class SQLiteDatabase: public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(NewDB);
        tpl->SetClassName(Nan::New("SQLiteDatabase").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("open").ToLocalChecked(), OpenGetter);
        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("inserted_oid").ToLocalChecked(), InsertedOidGetter);
        Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("affected_rows").ToLocalChecked(), AffectedRowsGetter);

        Nan::SetPrototypeMethod(tpl, "close", Close);
        Nan::SetPrototypeMethod(tpl, "closeSync", CloseSync);
        Nan::SetPrototypeMethod(tpl, "exec", Exec);
        Nan::SetPrototypeMethod(tpl, "run", Run);
        Nan::SetPrototypeMethod(tpl, "runSync", RunSync);
        Nan::SetPrototypeMethod(tpl, "query", Query);
        Nan::SetPrototypeMethod(tpl, "querySync", QuerySync);
        Nan::SetPrototypeMethod(tpl, "copy", Copy);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Database").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }

    static inline Nan::Persistent<Function> & constructor() {
        static Nan::Persistent<Function> _constructor;
        return _constructor;
    }

    struct Baton {
        uv_work_t request;
        SQLiteDatabase* db;
        Nan::Persistent<Function> callback;
        int status;
        string message;
        string sparam;
        int iparam;
        sqlite3_int64 inserted_id;
        int changes;

        Baton(SQLiteDatabase* db_, Local<Function> cb_, string s = "", int i = 0): db(db_), status(SQLITE_OK), sparam(s), iparam(i) {
            db->Ref();
            request.data = this;
            if (!cb_.IsEmpty()) callback.Reset(cb_);
        }
        virtual ~Baton() {
            db->Unref();
            callback.Reset();
        }
    };

    friend class SQLiteStatement;

    SQLiteDatabase() : Nan::ObjectWrap(), _handle(NULL), timeout(500), retries(2) {}
    virtual ~SQLiteDatabase() { sqlite3_close_v2(_handle); }

    static NAN_METHOD(NewDB);
    static NAN_GETTER(OpenGetter);
    static NAN_GETTER(InsertedOidGetter);
    static NAN_GETTER(AffectedRowsGetter);

    static void Work_Open(uv_work_t* req);
    static void Work_AfterOpen(uv_work_t* req);

    static NAN_METHOD(QuerySync);
    static NAN_METHOD(Query);
    static NAN_METHOD(RunSync);
    static NAN_METHOD(Run);
    static NAN_METHOD(Exec);
    static void Work_Exec(uv_work_t* req);
    static void Work_AfterExec(uv_work_t* req);

    static NAN_METHOD(CloseSync);
    static NAN_METHOD(Close);
    static void Work_Close(uv_work_t* req);
    static void Work_AfterClose(uv_work_t* req);
    static NAN_METHOD(Copy);

    sqlite3* _handle;
    int timeout;
    int retries;
};

static Nan::Persistent<ObjectTemplate> _tpl;

class SQLiteStatement: public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(NewStmt);
        tpl->SetClassName(Nan::New("SQLiteStatement").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "prepare", Prepare);
        Nan::SetPrototypeMethod(tpl, "run", Run);
        Nan::SetPrototypeMethod(tpl, "runSync", RunSync);
        Nan::SetPrototypeMethod(tpl, "query", Query);
        Nan::SetPrototypeMethod(tpl, "querySync", QuerySync);
        Nan::SetPrototypeMethod(tpl, "finalize", Finalize);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Statement").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());

        Local<ObjectTemplate> t = Nan::New<ObjectTemplate>();
        t->SetInternalFieldCount(1);
        _tpl.Reset(t);
    }

    static inline Nan::Persistent<Function> & constructor() {
        static Nan::Persistent<Function> _constructor;
        return _constructor;
    }
    static NAN_METHOD(NewStmt);

    static Local<Object> Create(SQLiteDatabase *db, string sql = string()) {
        Nan::EscapableHandleScope scope;
        Local<ObjectTemplate> t = Nan::New(_tpl);
        Local<Object> obj = Nan::NewInstance(t).ToLocalChecked();
        SQLiteStatement* stmt = new SQLiteStatement(db, sql);
        obj->SetInternalField(0, Nan::New(stmt));
        stmt->Wrap(obj);
        return scope.Escape(obj);
    }

    struct Baton {
        uv_work_t request;
        SQLiteStatement* stmt;
        Nan::Persistent<Function> callback;
        Row params;
        vector<Row> rows;
        sqlite3_int64 inserted_id;
        int changes;
        string sql;

        Baton(SQLiteStatement* stmt_, Local<Function> cb_): stmt(stmt_), inserted_id(0), changes(0), sql(stmt->sql)  {
            stmt->Ref();
            request.data = this;
            if (!cb_.IsEmpty()) callback.Reset(cb_);
        }
        virtual ~Baton() {
            stmt->Unref();
            callback.Reset();
        }
    };

    SQLiteStatement(SQLiteDatabase* db_, string sql_ = string()): Nan::ObjectWrap(), db(db_), _handle(NULL), sql(sql_), status(SQLITE_OK), each(NULL) {
        db->Ref();
        _stmts[this] = 0;
    }

    virtual ~SQLiteStatement() {
        Finalize();
        db->Unref();
        _stmts.erase(this);
    }

    void Finalize(void) {
        LogDev("%s", sql.c_str());
        if (_handle) sqlite3_finalize(_handle);
        _handle = NULL;
    }

    bool Prepare() {
        _handle = NULL;
        status = bkSqlitePrepare(db->_handle, &_handle, sql, db->retries, db->timeout);
        if (status != SQLITE_OK) {
            message = string(sqlite3_errmsg(db->_handle));
            if (_handle) sqlite3_finalize(_handle);
            _handle = NULL;
            return false;
        }
        return true;
    }

    static NAN_METHOD(Finalize);

    static NAN_METHOD(Prepare);
    static void Work_Prepare(uv_work_t* req);
    static void Work_AfterPrepare(uv_work_t* req);

    static NAN_METHOD(RunSync);
    static NAN_METHOD(Run);
    static void Work_Run(uv_work_t* req);
    static void Work_RunPrepare(uv_work_t* req);
    static void Work_AfterRun(uv_work_t* req);

    static NAN_METHOD(QuerySync);
    static NAN_METHOD(Query);
    static void Work_Query(uv_work_t* req);
    static void Work_QueryPrepare(uv_work_t* req);
    static void Work_AfterQuery(uv_work_t* req);

    SQLiteDatabase* db;
    sqlite3_stmt* _handle;
    string sql;
    string op;
    int status;
    string message;
    Baton *each;
};

NAN_METHOD(stats)
{
    Nan::HandleScope scope;
    Local<Array> keys = Nan::New<Array>();
    map<SQLiteStatement*,bool>::const_iterator it = _stmts.begin();
    int i = 0;
    while (it != _stmts.end()) {
        Local<Object> obj = Nan::New<Object>();
        Nan::Set(obj, Nan::New("op").ToLocalChecked(), Nan::New(it->first->op.c_str()).ToLocalChecked());
        Nan::Set(obj, Nan::New("sql").ToLocalChecked(), Nan::New(it->first->sql.c_str()).ToLocalChecked());
        Nan::Set(obj, Nan::New("prepared").ToLocalChecked(), Nan::New(it->first->_handle != NULL));
        Nan::Set(keys, Nan::New(i), obj);
        it++;
        i++;
    }
    NAN_RETURN(keys);
}

NAN_MODULE_INIT(SqliteInit)
{
    Nan::HandleScope scope;

    NAN_EXPORT(target, stats);

    sqlite3_initialize();
    SQLiteDatabase::Init(target);
    SQLiteStatement::Init(target);

    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_READONLY, OPEN_READONLY);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_READWRITE, OPEN_READWRITE);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_CREATE, OPEN_CREATE);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_NOMUTEX, OPEN_NOMUTEX);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_FULLMUTEX, OPEN_FULMUTEX);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_SHAREDCACHE, OPEN_SHAREDCACHE);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_PRIVATECACHE, OPEN_PRIVATECACHE);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OPEN_URI, OPEN_URI);

    NAN_DEFINE_CONSTANT_STRING(target, SQLITE_VERSION, SQLITE_VERSION);
    NAN_DEFINE_CONSTANT_STRING(target, SQLITE_SOURCE_ID, SQLITE_SOURCE_ID);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_VERSION_NUMBER, SQLITE_VERSION_NUMBER);

    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_OK, OK);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_ERROR, ERROR);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_INTERNAL, INTERNAL);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_PERM, PERM);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_ABORT, ABORT);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_BUSY, BUSY);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_LOCKED, LOCKED);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_NOMEM, NOMEM);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_READONLY, READONLY);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_INTERRUPT, INTERRUPT);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_IOERR, IOERR);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_CORRUPT, CORRUPT);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_NOTFOUND, NOTFOUND);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_FULL, FULL);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_CANTOPEN, CANTOPEN);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_PROTOCOL, PROTOCOL);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_EMPTY, EMPTY);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_SCHEMA, SCHEMA);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_TOOBIG, TOOBIG);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_CONSTRAINT, CONSTRAINT);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_MISMATCH, MISMATCH);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_MISUSE, MISUSE);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_NOLFS, NOLFS);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_AUTH, AUTH);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_FORMAT, FORMAT);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_RANGE, RANGE);
    NAN_DEFINE_CONSTANT_INTEGER(target, SQLITE_NOTADB, NOTADB);
}

NODE_MODULE(binding, SqliteInit);

static bool BindParameters(Row &params, sqlite3_stmt *stmt)
{
    sqlite3_reset(stmt);
    if (!params.size()) return true;

    sqlite3_clear_bindings(stmt);
    for (uint i = 0; i < params.size(); i++) {
        SQLiteField &field = params[i];
        int status;
        switch (field.type) {
        case SQLITE_INTEGER:
            status = sqlite3_bind_int(stmt, field.index, field.nvalue);
            break;
        case SQLITE_FLOAT:
            status = sqlite3_bind_double(stmt, field.index, field.nvalue);
            break;
        case SQLITE_TEXT:
            status = sqlite3_bind_text(stmt, field.index, field.svalue.c_str(), field.svalue.size(), SQLITE_TRANSIENT);
            break;
        case SQLITE_BLOB:
            status = sqlite3_bind_blob(stmt, field.index, field.svalue.c_str(), field.svalue.size(), SQLITE_TRANSIENT);
            break;
        case SQLITE_NULL:
            status = sqlite3_bind_null(stmt, field.index);
            break;
        }
        if (status != SQLITE_OK) return false;
    }
    return true;
}

static bool ParseParameters(Row &params, const Nan::FunctionCallbackInfo<v8::Value>& args, int idx)
{
    Nan::HandleScope scope;
    if (idx >= args.Length() || !args[idx]->IsArray()) return false;

    Local<Array> array = Local<Array>::Cast(args[idx]);
    for (uint i = 0, pos = 1; i < array->Length(); i++, pos++) {
        Local<Value> source = Nan::Get(array, i).ToLocalChecked();

        if (source->IsString() || source->IsRegExp()) {
            Nan::Utf8String val(Nan::To<String>(source).ToLocalChecked());
            params.push_back(SQLiteField(pos, SQLITE_TEXT, 0, string(*val, val.length())));
        } else
        if (source->IsInt32()) {
            params.push_back(SQLiteField(pos, SQLITE_INTEGER, Nan::To<int32_t>(source).FromJust()));
        } else
        if (source->IsNumber()) {
            params.push_back(SQLiteField(pos, SQLITE_FLOAT, Nan::To<double>(source).FromJust()));
        } else
        if (source->IsBoolean()) {
            params.push_back(SQLiteField(pos, SQLITE_INTEGER, Nan::To<bool>(source).FromJust() ? 1 : 0));
        } else
        if (source->IsNull()) {
            params.push_back(SQLiteField(pos));
        } else
        if (Buffer::HasInstance(source)) {
            Local < Object > buffer = Nan::To<Object>(source).ToLocalChecked();
            params.push_back(SQLiteField(pos, SQLITE_BLOB, 0, string(Buffer::Data(buffer), Buffer::Length(buffer))));
        } else
        if (source->IsDate()) {
            params.push_back(SQLiteField(pos, SQLITE_FLOAT, Nan::To<double>(source).FromJust()));
        } else
        if (source->IsUndefined()) {
            params.push_back(SQLiteField(pos));
        }
    }
    return true;
}

static void GetRow(Row &row, sqlite3_stmt* stmt)
{
    row.clear();
    int cols = sqlite3_column_count(stmt);
    for (int i = 0; i < cols; i++) {
        int type = sqlite3_column_type(stmt, i);
        int length = sqlite3_column_bytes(stmt, i);
        const char* name = sqlite3_column_name(stmt, i);
        const char* dtype = sqlite3_column_decltype(stmt, i);
        const char* text;

        if (dtype && !strcasecmp(dtype, "json")) type = SQLITE_TEXT;
        switch (type) {
        case SQLITE_INTEGER:
            row.push_back(SQLiteField(name, type, sqlite3_column_int64(stmt, i)));
            break;
        case SQLITE_FLOAT:
            row.push_back(SQLiteField(name, type, sqlite3_column_double(stmt, i)));
            break;
        case SQLITE_TEXT:
            text = (const char*) sqlite3_column_text(stmt, i);
            row.push_back(SQLiteField(name, type, 0, string(text, length)));
            break;
        case SQLITE_BLOB:
            text = (const char*)sqlite3_column_blob(stmt, i);
            row.push_back(SQLiteField(name, type, 0, string(text, length)));
            break;
        case SQLITE_NULL:
            row.push_back(SQLiteField(name));
            break;
        }
    }
}

static Local<Object> GetRow(sqlite3_stmt *stmt)
{
    Nan::EscapableHandleScope scope;

    Local<Object> obj = Nan::New<Object>();
    int cols = sqlite3_column_count(stmt);
    for (int i = 0; i < cols; i++) {
        int type = sqlite3_column_type(stmt, i);
        int length = sqlite3_column_bytes(stmt, i);
        const char* name = sqlite3_column_name(stmt, i);
        const char* dtype = sqlite3_column_decltype(stmt, i);
        Local<Value> value;

        if (dtype && !strcasecmp(dtype, "json")) type = SQLITE_TEXT;
        switch (type) {
        case SQLITE_INTEGER:
            value = Nan::New((double)sqlite3_column_int64(stmt, i));
            break;
        case SQLITE_FLOAT:
            value = Nan::New(sqlite3_column_double(stmt, i));
            break;
        case SQLITE_TEXT:
            value = Nan::New((const char*)sqlite3_column_text(stmt, i)).ToLocalChecked();
            break;
        case SQLITE_BLOB:
            value = Nan::CopyBuffer((const char*)sqlite3_column_blob(stmt, i), length).ToLocalChecked();
            break;
        case SQLITE_NULL:
            value = Nan::Null();
            break;
        }
        Nan::Set(obj, Nan::New(name).ToLocalChecked(), value);
    }
    return scope.Escape(obj);
}

static Local<Object> RowToJS(Row &row)
{
    Nan::EscapableHandleScope scope;

    Local<Object> result = Nan::New<Object>();
    for (uint i = 0; i < row.size(); i++) {
        SQLiteField &field = row[i];
        Local<Value> value;
        switch (field.type) {
        case SQLITE_INTEGER:
            value = Nan::New(field.nvalue);
            break;
        case SQLITE_FLOAT:
            value = Nan::New(field.nvalue);
            break;
        case SQLITE_TEXT:
            value = Nan::New(field.svalue).ToLocalChecked();
            break;
        case SQLITE_BLOB:
            value = Nan::CopyBuffer((const char*)field.svalue.c_str(), field.svalue.size()).ToLocalChecked();
            break;
        case SQLITE_NULL:
            value = Nan::Null();
            break;
        }
        Nan::Set(result, Nan::New(field.name).ToLocalChecked(), value);
    }
    row.clear();
    return scope.Escape(result);
}

static const char* sqlite_code_string(int code)
{
    switch (code) {
    case SQLITE_OK:
        return "SQLITE_OK";
    case SQLITE_ERROR:
        return "SQLITE_ERROR";
    case SQLITE_INTERNAL:
        return "SQLITE_INTERNAL";
    case SQLITE_PERM:
        return "SQLITE_PERM";
    case SQLITE_ABORT:
        return "SQLITE_ABORT";
    case SQLITE_BUSY:
        return "SQLITE_BUSY";
    case SQLITE_LOCKED:
        return "SQLITE_LOCKED";
    case SQLITE_NOMEM:
        return "SQLITE_NOMEM";
    case SQLITE_READONLY:
        return "SQLITE_READONLY";
    case SQLITE_INTERRUPT:
        return "SQLITE_INTERRUPT";
    case SQLITE_IOERR:
        return "SQLITE_IOERR";
    case SQLITE_CORRUPT:
        return "SQLITE_CORRUPT";
    case SQLITE_NOTFOUND:
        return "SQLITE_NOTFOUND";
    case SQLITE_FULL:
        return "SQLITE_FULL";
    case SQLITE_CANTOPEN:
        return "SQLITE_CANTOPEN";
    case SQLITE_PROTOCOL:
        return "SQLITE_PROTOCOL";
    case SQLITE_EMPTY:
        return "SQLITE_EMPTY";
    case SQLITE_SCHEMA:
        return "SQLITE_SCHEMA";
    case SQLITE_TOOBIG:
        return "SQLITE_TOOBIG";
    case SQLITE_CONSTRAINT:
        return "SQLITE_CONSTRAINT";
    case SQLITE_MISMATCH:
        return "SQLITE_MISMATCH";
    case SQLITE_MISUSE:
        return "SQLITE_MISUSE";
    case SQLITE_NOLFS:
        return "SQLITE_NOLFS";
    case SQLITE_AUTH:
        return "SQLITE_AUTH";
    case SQLITE_FORMAT:
        return "SQLITE_FORMAT";
    case SQLITE_RANGE:
        return "SQLITE_RANGE";
    case SQLITE_NOTADB:
        return "SQLITE_NOTADB";
    case SQLITE_ROW:
        return "SQLITE_ROW";
    case SQLITE_DONE:
        return "SQLITE_DONE";
    default:
        return "UNKNOWN";
    }
}

NAN_GETTER(SQLiteDatabase::OpenGetter)
{
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.This());
    NAN_RETURN(Nan::New(db->_handle != NULL));
}

NAN_GETTER(SQLiteDatabase::InsertedOidGetter)
{
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.This());
    NAN_RETURN(Nan::New((double)sqlite3_last_insert_rowid(db->_handle)));
}

NAN_GETTER(SQLiteDatabase::AffectedRowsGetter)
{
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.This());
    NAN_RETURN(Nan::New(sqlite3_changes(db->_handle)));
}

NAN_METHOD(SQLiteDatabase::NewDB)
{
    Nan::HandleScope scope;

    if (!info.IsConstructCall()) Nan::ThrowError("Use the new operator to create new Database objects");

    NAN_REQUIRE_ARGUMENT_STRING(0, filename);
    int arg = 1, mode = 0;
    if (info.Length() >= arg && info[arg]->IsInt32()) mode = Nan::To<int32_t>(info[arg++]).FromJust();

    Local < Function > callback;
    if (info.Length() >= arg && info[arg]->IsFunction()) callback = Local < Function > ::Cast(info[arg]);

    // Default RW and create
    mode |= mode & SQLITE_OPEN_READONLY ? 0 : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    // No global mutex in read only
    mode |= mode & SQLITE_OPEN_NOMUTEX ? 0 : SQLITE_OPEN_FULLMUTEX;
    // Default is shared cache unless private is specified
    mode |= mode & SQLITE_OPEN_PRIVATECACHE ? 0 : SQLITE_OPEN_SHAREDCACHE;

    SQLiteDatabase* db = new SQLiteDatabase();
    db->Wrap(info.This());
    Nan::Set(info.This(), Nan::New("name").ToLocalChecked(), Nan::New(*filename).ToLocalChecked());
    Nan::Set(info.This(), Nan::New("mode").ToLocalChecked(), Nan::New(mode));

    if (!callback.IsEmpty()) {
        Baton* baton = new Baton(db, callback, *filename, mode);
        uv_queue_work(uv_default_loop(), &baton->request, Work_Open, (uv_after_work_cb)Work_AfterOpen);
    } else {
        int status = sqlite3_open_v2(*filename, &db->_handle, mode, NULL);
        if (status != SQLITE_OK) {
            sqlite3_close(db->_handle);
            db->_handle = NULL;
            Nan::ThrowError(sqlite3_errmsg(db->_handle));
        }
        bkSqliteInitDb(db->_handle, NULL);
    }
    NAN_RETURN(info.This());
}

void SQLiteDatabase::Work_Open(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    baton->status = sqlite3_open_v2(baton->sparam.c_str(), &baton->db->_handle, baton->iparam, NULL);
    if (baton->status != SQLITE_OK) {
        baton->message = string(sqlite3_errmsg(baton->db->_handle));
        sqlite3_close(baton->db->_handle);
        baton->db->_handle = NULL;
    } else {
        bkSqliteInitDb(baton->db->_handle, NULL);
    }
}

void SQLiteDatabase::Work_AfterOpen(uv_work_t* req)
{
    Nan::HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    if (!baton->callback.IsEmpty()) {
        Local < Value > argv[1];
        Local<Function> cb = Nan::New(baton->callback);
        if (baton->status != SQLITE_OK) {
            EXCEPTION(baton->message.c_str(), baton->status, exception);
            argv[0] = exception;
        } else {
            argv[0] = Nan::Null();
        }
        NAN_TRY_CATCH_CALL(baton->db->handle(), cb, 1, argv);
    } else
    if (baton->status != SQLITE_OK) {
        LogError("%s", baton->message.c_str());
    }
    delete baton;
}

NAN_METHOD(SQLiteDatabase::CloseSync)
{
    Nan::HandleScope scope;
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());
    NAN_EXPECT_ARGUMENT_FUNCTION(0, callback);

    int status = sqlite3_close(db->_handle);
    db->_handle = NULL;
    if (status != SQLITE_OK) {
        Nan::ThrowError(sqlite3_errmsg(db->_handle));
    }
    NAN_RETURN(info.Holder());
}

NAN_METHOD(SQLiteDatabase::Close)
{
    Nan::HandleScope scope;
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());
    NAN_EXPECT_ARGUMENT_FUNCTION(0, callback);

    Baton* baton = new Baton(db, callback);
    uv_queue_work(uv_default_loop(), &baton->request, Work_Close, (uv_after_work_cb)Work_AfterClose);

    NAN_RETURN(info.Holder());
}

void SQLiteDatabase::Work_Close(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    baton->status = sqlite3_close(baton->db->_handle);
    if (baton->status != SQLITE_OK) {
        baton->message = string(sqlite3_errmsg(baton->db->_handle));
    }
    baton->db->_handle = NULL;
}

void SQLiteDatabase::Work_AfterClose(uv_work_t* req)
{
    Nan::HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    if (!baton->callback.IsEmpty()) {
        Local < Value > argv[1];
        Local<Function> cb = Nan::New(baton->callback);
        if (baton->status != SQLITE_OK) {
            EXCEPTION(baton->message.c_str(), baton->status, exception);
            argv[0] = exception;
        } else {
            argv[0] = Nan::Null();
        }
        NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), cb, 1, argv);
    } else
    if (baton->status != SQLITE_OK) {
        LogError("%s", baton->message.c_str());
    }
    delete baton;
}

NAN_METHOD(SQLiteDatabase::QuerySync)
{
    Nan::EscapableHandleScope scope;
    SQLiteDatabase *db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());

    NAN_REQUIRE_ARGUMENT_STRING(0, text);

    Row params;
    sqlite3_stmt *stmt;
    ParseParameters(params, info, 1);
    int status = sqlite3_prepare_v2(db->_handle, *text, text.length(), &stmt, NULL);
    if (status != SQLITE_OK) {
        Nan::ThrowError(sqlite3_errmsg(db->_handle));
    }

    int n = 0;
    string message;
    Local<Array> result = Nan::New<Array>();
    if (BindParameters(params, stmt)) {
        while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {
            Local<Object> obj(GetRow(stmt));
            Nan::Set(result, Nan::New(n++), obj);
        }
        if (status != SQLITE_DONE) {
            message = string(sqlite3_errmsg(db->_handle));
        }
    } else {
        message = string(sqlite3_errmsg(db->_handle));
    }
    sqlite3_finalize(stmt);
    if (status != SQLITE_DONE) {
        Nan::ThrowError(message.c_str());
    }
    NAN_RETURN(result);
}

NAN_METHOD(SQLiteDatabase::RunSync)
{
    Nan::HandleScope scope;
    SQLiteDatabase *db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());

    NAN_REQUIRE_ARGUMENT_STRING(0, text);

    Row params;
    string message;
    sqlite3_stmt *stmt;
    ParseParameters(params, info, 1);
    int status = sqlite3_prepare_v2(db->_handle, *text, text.length(), &stmt, NULL);
    if (status != SQLITE_OK) {
        Nan::ThrowError(sqlite3_errmsg(db->_handle));
    }

    if (BindParameters(params, stmt)) {
        status = sqlite3_step(stmt);
        if (!(status == SQLITE_ROW || status == SQLITE_DONE)) {
            message = string(sqlite3_errmsg(db->_handle));
        } else {
            status = SQLITE_OK;
            Nan::Set(db->handle(), Nan::New("inserted_oid").ToLocalChecked(), Nan::New((double)sqlite3_last_insert_rowid(db->_handle)));
            Nan::Set(db->handle(), Nan::New("affected_rows").ToLocalChecked(), Nan::New(sqlite3_changes(db->_handle)));
        }
    } else {
        message = string(sqlite3_errmsg(db->_handle));
    }
    sqlite3_finalize(stmt);
    if (status != SQLITE_OK) {
        Nan::ThrowError(message.c_str());
    }
    NAN_RETURN(info.Holder());
}

NAN_METHOD(SQLiteDatabase::Run)
{
    Nan::HandleScope scope;
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());

    NAN_REQUIRE_ARGUMENT_STRING(0, sql);
    NAN_OPTIONAL_ARGUMENT_FUNCTION(-1, callback);

    Local<Object> obj = SQLiteStatement::Create(db, *sql);
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (obj);
    SQLiteStatement::Baton* baton = new SQLiteStatement::Baton(stmt, callback);
    ParseParameters(baton->params, info, 1);
    uv_queue_work(uv_default_loop(), &baton->request, SQLiteStatement::Work_RunPrepare, (uv_after_work_cb)SQLiteStatement::Work_AfterRun);

    NAN_RETURN(obj);
}

NAN_METHOD(SQLiteDatabase::Query)
{
    Nan::HandleScope scope;
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());

    NAN_REQUIRE_ARGUMENT_STRING(0, sql);
    NAN_OPTIONAL_ARGUMENT_FUNCTION(-1, callback);

    Local<Object> obj = SQLiteStatement::Create(db, *sql);
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (obj);
    SQLiteStatement::Baton* baton = new SQLiteStatement::Baton(stmt, callback);
    ParseParameters(baton->params, info, 1);
    uv_queue_work(uv_default_loop(), &baton->request, SQLiteStatement::Work_QueryPrepare, (uv_after_work_cb)SQLiteStatement::Work_AfterQuery);

    NAN_RETURN(obj);
}

NAN_METHOD(SQLiteDatabase::Exec)
{
    Nan::HandleScope scope;
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());

    NAN_REQUIRE_ARGUMENT_STRING(0, sql);
    NAN_EXPECT_ARGUMENT_FUNCTION(1, callback);

    Baton* baton = new Baton(db, callback, *sql);
    uv_queue_work(uv_default_loop(), &baton->request, Work_Exec, (uv_after_work_cb)Work_AfterExec);

    NAN_RETURN(info.Holder());
}

void SQLiteDatabase::Work_Exec(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    char* message = NULL;
    baton->status = sqlite3_exec(baton->db->_handle, baton->sparam.c_str(), NULL, NULL, &message);
    if (baton->status != SQLITE_OK) {
        baton->message = bkFmtStr("sqlite3 error %d: %s", baton->status, message ? message : sqlite3_errmsg(baton->db->_handle));
        sqlite3_free(message);
    } else {
        baton->inserted_id = sqlite3_last_insert_rowid(baton->db->_handle);
        baton->changes = sqlite3_changes(baton->db->_handle);
    }
}

void SQLiteDatabase::Work_AfterExec(uv_work_t* req)
{
    Nan::HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    Nan::Set(baton->db->handle(), Nan::New("inserted_oid").ToLocalChecked(), Nan::New((double)baton->inserted_id));
    Nan::Set(baton->db->handle(), Nan::New("affected_rows").ToLocalChecked(), Nan::New(baton->changes));

    if (!baton->callback.IsEmpty()) {
        Local < Value > argv[1];
        Local<Function> cb = Nan::New(baton->callback);
        if (baton->status != SQLITE_OK) {
            EXCEPTION(baton->message.c_str(), baton->status, exception);
            argv[0] = exception;
        } else {
            argv[0] = Nan::Null();
        }
        NAN_TRY_CATCH_CALL(baton->db->handle(), cb, 1, argv);
    } else
    if (baton->status != SQLITE_OK) {
        LogError("%s", baton->message.c_str());
    }
    delete baton;
}

NAN_METHOD(SQLiteDatabase::Copy)
{
    Nan::HandleScope scope;
    SQLiteDatabase* db = ObjectWrap::Unwrap < SQLiteDatabase > (info.Holder());
    string errmsg;
    sqlite3 *handle2 = 0;
    int rc = SQLITE_OK;

    if (info.Length() && info[0]->IsObject()) {
        SQLiteDatabase* sdb = Nan::ObjectWrap::Unwrap < SQLiteDatabase > (Nan::To<Object>(info[0]).ToLocalChecked());
        handle2 = sdb->_handle;
    } else
    if (info.Length() && info[0]->IsString()) {
        Nan::Utf8String filename(info[0]);
        rc = sqlite3_open_v2(*filename, &handle2, SQLITE_OPEN_READONLY, NULL);
        if (rc != SQLITE_OK) {
            errmsg = sqlite3_errmsg(handle2);
            sqlite3_close(handle2);
            Nan::ThrowError(errmsg.c_str());
        }
    } else {
        Nan::ThrowError("Database object or database file name expected");
    }

    sqlite3_backup *backup;
    backup = sqlite3_backup_init(db->_handle, "main", handle2, "main");
    if (backup) {
        sqlite3_backup_step(backup, -1);
        sqlite3_backup_finish(backup);
        rc = sqlite3_errcode(db->_handle);
        errmsg = sqlite3_errmsg(db->_handle);
    }

    if (info[0]->IsString()) {
        sqlite3_close(handle2);
    }

    if (rc != SQLITE_OK) {
        Nan::ThrowError(errmsg.c_str());
    }
    NAN_RETURN(info.Holder());
}

// { Database db, String sql, Function callback }
NAN_METHOD(SQLiteStatement::NewStmt)
{
    Nan::HandleScope scope;

    if (!info.IsConstructCall()) Nan::ThrowError("Use the new operator to create new Statement objects");

    if (info.Length() < 1 || !info[0]->IsObject()) return Nan::ThrowError("Database object expected");
    NAN_REQUIRE_ARGUMENT_STRING(1, sql);
    NAN_EXPECT_ARGUMENT_FUNCTION(2, callback);

    SQLiteDatabase* db = Nan::ObjectWrap::Unwrap < SQLiteDatabase > (Nan::To<Object>(info[0]).ToLocalChecked());
    SQLiteStatement* stmt = new SQLiteStatement(db, *sql);
    stmt->Wrap(info.This());
    Nan::Set(info.This(), Nan::New("sql").ToLocalChecked(), Nan::New(*sql).ToLocalChecked());
    stmt->op = "new";
    Baton* baton = new Baton(stmt, callback);
    uv_queue_work(uv_default_loop(), &baton->request, Work_Prepare, (uv_after_work_cb)Work_AfterPrepare);

    NAN_RETURN(info.Holder());
}

NAN_METHOD(SQLiteStatement::Prepare)
{
    Nan::HandleScope scope;
    SQLiteStatement* stmt = Nan::ObjectWrap::Unwrap < SQLiteStatement > (info.Holder());

    NAN_REQUIRE_ARGUMENT_STRING(0, sql);
    NAN_OPTIONAL_ARGUMENT_FUNCTION(-1, callback);

    stmt->op = "prepare";
    stmt->sql = *sql;
    Baton* baton = new Baton(stmt, callback);
    uv_queue_work(uv_default_loop(), &baton->request, Work_Prepare, (uv_after_work_cb)Work_AfterPrepare);

    NAN_RETURN(info.Holder());
}

void SQLiteStatement::Work_Prepare(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);
    baton->stmt->Prepare();
}

void SQLiteStatement::Work_AfterPrepare(uv_work_t* req)
{
    Nan::HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    if (!baton->callback.IsEmpty()) {
        Local < Value > argv[1];
        Local<Function> cb = Nan::New(baton->callback);
        if (baton->stmt->status != SQLITE_OK) {
            EXCEPTION(baton->stmt->message.c_str(), baton->stmt->status, exception);
            argv[0] = exception;
        } else {
            argv[0] = Nan::Null();
        }
        NAN_TRY_CATCH_CALL(baton->stmt->handle(), cb, 1, argv);
    } else
    if (baton->stmt->status != SQLITE_OK) {
        LogError("%s", baton->stmt->message.c_str());
    }
    delete baton;
}

NAN_METHOD(SQLiteStatement::Finalize)
{
    Nan::HandleScope scope;
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (info.Holder());

    stmt->Finalize();
    NAN_RETURN(info.Holder());
}

NAN_METHOD(SQLiteStatement::RunSync)
{
    Nan::HandleScope scope;
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (info.Holder());
    Row params;

    stmt->op = "runSync";
    ParseParameters(params, info, 0);
    if (BindParameters(params, stmt->_handle)) {
        stmt->status = sqlite3_step(stmt->_handle);

        if (!(stmt->status == SQLITE_ROW || stmt->status == SQLITE_DONE)) {
            stmt->message = string(sqlite3_errmsg(stmt->db->_handle));
        } else {
            Nan::Set(stmt->handle(), Nan::New("lastID").ToLocalChecked(), Nan::New((double)sqlite3_last_insert_rowid(stmt->db->_handle)));
            Nan::Set(stmt->handle(), Nan::New("changes").ToLocalChecked(), Nan::New((int)sqlite3_changes(stmt->db->_handle)));
            stmt->status = SQLITE_OK;
        }
    } else {
        stmt->message = string(sqlite3_errmsg(stmt->db->_handle));
    }

    if (stmt->status != SQLITE_OK) {
        Nan::ThrowError(stmt->message.c_str());
    }
    NAN_RETURN(info.Holder());
}

NAN_METHOD(SQLiteStatement::Run)
{
    Nan::HandleScope scope;
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (info.Holder());

    NAN_OPTIONAL_ARGUMENT_FUNCTION(-1, callback);

    stmt->op = "run";
    Baton* baton = new Baton(stmt, callback);
    ParseParameters(baton->params, info, 0);

    uv_queue_work(uv_default_loop(), &baton->request, Work_Run, (uv_after_work_cb)Work_AfterRun);
    NAN_RETURN(info.Holder());
}

void SQLiteStatement::Work_Run(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    if (BindParameters(baton->params, baton->stmt->_handle)) {
        baton->stmt->status = bkSqliteStep(baton->stmt->_handle, baton->stmt->db->retries, baton->stmt->db->timeout);

        if (!(baton->stmt->status == SQLITE_ROW || baton->stmt->status == SQLITE_DONE)) {
            baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
        } else {
            baton->inserted_id = sqlite3_last_insert_rowid(baton->stmt->db->_handle);
            baton->changes = sqlite3_changes(baton->stmt->db->_handle);
            baton->stmt->status = SQLITE_OK;
        }
    } else {
        baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
    }
}

void SQLiteStatement::Work_RunPrepare(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    if (!baton->stmt->Prepare()) return;

    if (BindParameters(baton->params, baton->stmt->_handle)) {
        baton->stmt->status = bkSqliteStep(baton->stmt->_handle, baton->stmt->db->retries, baton->stmt->db->timeout);

        if (!(baton->stmt->status == SQLITE_ROW || baton->stmt->status == SQLITE_DONE)) {
            baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
        } else {
            baton->inserted_id = sqlite3_last_insert_rowid(baton->stmt->db->_handle);
            baton->changes = sqlite3_changes(baton->stmt->db->_handle);
            baton->stmt->status = SQLITE_OK;
        }
    } else {
        baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
    }
    baton->stmt->Finalize();
}

void SQLiteStatement::Work_AfterRun(uv_work_t* req)
{
    Nan::HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    Nan::Set(baton->stmt->handle(), Nan::New("lastID").ToLocalChecked(), Nan::New((double)baton->inserted_id));
    Nan::Set(baton->stmt->handle(), Nan::New("changes").ToLocalChecked(), Nan::New(baton->changes));

    if (!baton->callback.IsEmpty()) {
        Local < Value > argv[1];
        Local<Function> cb = Nan::New(baton->callback);
        if (baton->stmt->status != SQLITE_OK) {
            EXCEPTION(baton->stmt->message.c_str(), baton->stmt->status, exception);
            argv[0] = exception;
        } else {
            argv[0] = Nan::Null();
        }
        NAN_TRY_CATCH_CALL(baton->stmt->handle(), cb, 1, argv);
    } else
    if (baton->stmt->status != SQLITE_OK) {
        LogError("%s", baton->stmt->message.c_str());
    }
    delete baton;
}

NAN_METHOD(SQLiteStatement::QuerySync)
{
    Nan::HandleScope scope;
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (info.Holder());

    NAN_OPTIONAL_ARGUMENT_FUNCTION(-1, callback);

    int n = 0;
    Row params;
    ParseParameters(params, info, 0);
    Local<Array> result = Nan::New<Array>();
    stmt->op = "querySync";

    if (BindParameters(params, stmt->_handle)) {
        while ((stmt->status = sqlite3_step(stmt->_handle)) == SQLITE_ROW) {
            Local<Object> obj(GetRow(stmt->_handle));
            Nan::Set(result, Nan::New(n++), obj);
        }
        if (stmt->status != SQLITE_DONE) {
            stmt->message = string(sqlite3_errmsg(stmt->db->_handle));
        }
    } else {
        stmt->message = string(sqlite3_errmsg(stmt->db->_handle));
    }
    if (stmt->status != SQLITE_DONE) {
        Nan::ThrowError(stmt->message.c_str());
    }
    NAN_RETURN(result);
}

NAN_METHOD(SQLiteStatement::Query)
{
    Nan::HandleScope scope;
    SQLiteStatement* stmt = ObjectWrap::Unwrap < SQLiteStatement > (info.Holder());

    NAN_OPTIONAL_ARGUMENT_FUNCTION(-1, callback);
    Baton* baton = new Baton(stmt, callback);
    ParseParameters(baton->params, info, 0);
    stmt->op = "query";
    uv_queue_work(uv_default_loop(), &baton->request, Work_Query, (uv_after_work_cb)Work_AfterQuery);
    NAN_RETURN(info.Holder());
}

void SQLiteStatement::Work_Query(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    if (BindParameters(baton->params, baton->stmt->_handle)) {
        while ((baton->stmt->status = bkSqliteStep(baton->stmt->_handle, baton->stmt->db->retries, baton->stmt->db->timeout)) == SQLITE_ROW) {
            Row row;
            GetRow(row, baton->stmt->_handle);
            baton->rows.push_back(row);
        }
        if (baton->stmt->status != SQLITE_DONE) {
            baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
        }
    } else {
        baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
    }
}

void SQLiteStatement::Work_QueryPrepare(uv_work_t* req)
{
    Baton* baton = static_cast<Baton*>(req->data);

    if (!baton->stmt->Prepare()) return;

    if (BindParameters(baton->params, baton->stmt->_handle)) {
        while ((baton->stmt->status = bkSqliteStep(baton->stmt->_handle, baton->stmt->db->retries, baton->stmt->db->timeout)) == SQLITE_ROW) {
            Row row;
            GetRow(row, baton->stmt->_handle);
            baton->rows.push_back(row);
        }
        if (baton->stmt->status != SQLITE_DONE) {
            baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
        }
    } else {
        baton->stmt->message = string(sqlite3_errmsg(baton->stmt->db->_handle));
    }
    baton->stmt->Finalize();
}

void SQLiteStatement::Work_AfterQuery(uv_work_t* req)
{
    Nan::HandleScope scope;
    Baton* baton = static_cast<Baton*>(req->data);

    Nan::Set(baton->stmt->handle(), Nan::New("lastID").ToLocalChecked(), Nan::New((double)baton->inserted_id));
    Nan::Set(baton->stmt->handle(), Nan::New("changes").ToLocalChecked(), Nan::New(baton->changes));

    if (!baton->callback.IsEmpty()) {
        Local<Function> cb = Nan::New(baton->callback);
        if (baton->stmt->status != SQLITE_DONE) {
            EXCEPTION(baton->stmt->message.c_str(), baton->stmt->status, exception);
            Local<Value> argv[] = { exception, Nan::New<Array>() };
            NAN_TRY_CATCH_CALL(baton->stmt->handle(), cb, 2, argv);
        } else
        if (baton->rows.size()) {
            Local<Array> result = Nan::New<Array>(baton->rows.size());
            for (uint i = 0; i < baton->rows.size(); i++) {
                Nan::Set(result, i, RowToJS(baton->rows[i]));
            }
            Local<Value> argv[] = { Nan::Null(), result };
            NAN_TRY_CATCH_CALL(baton->stmt->handle(), cb, 2, argv);
        } else {
            Local<Value> argv[] = { Nan::Null(), Nan::New<Array>() };
            NAN_TRY_CATCH_CALL(baton->stmt->handle(), cb, 2, argv);
        }
    } else
    if (baton->stmt->status != SQLITE_DONE) {
        LogError("%s", baton->stmt->message.c_str());
    }
    delete baton;
}

