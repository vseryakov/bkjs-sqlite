//
//  Author: Vlad Seryakov vseryakov@gmail.com
//  April 2007
//

#ifndef _BK_JS_H
#define _BK_JS_H

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
#include <node_version.h>
#include <v8.h>
#include <v8-profiler.h>
#include <uv.h>
#include <nan.h>

using namespace node;
using namespace v8;
using namespace std;

#define BKJS_VERSION "0.1.0"

#define NAN_RETURN(x) info.GetReturnValue().Set(x)

#define NAN_REQUIRE_ARGUMENT(i) if (info.Length() <= i || info[i]->IsUndefined()) {Nan::ThrowError("Argument " #i " is required");return;}
#define NAN_REQUIRE_ARGUMENT_STRING(i, var) if (info.Length() <= (i) || !info[i]->IsString()) {Nan::ThrowError("Argument " #i " must be a string"); return;} Nan::Utf8String var(info[i]->ToString());
#define NAN_REQUIRE_ARGUMENT_AS_STRING(i, var) if (info.Length() <= (i)) {Nan::ThrowError("Argument " #i " must be a string"); return;} Nan::Utf8String var(info[i]->ToString());
#define NAN_REQUIRE_ARGUMENT_OBJECT(i, var) if (info.Length() <= (i) || !info[i]->IsObject()) {Nan::ThrowError("Argument " #i " must be an object"); return;} Local<Object> var(info[i]->ToObject());
#define NAN_REQUIRE_ARGUMENT_INT(i, var) if (info.Length() <= (i)) {Nan::ThrowError("Argument " #i " must be an integer"); return;} int var = info[i]->Int32Value();
#define NAN_REQUIRE_ARGUMENT_INT64(i, var) if (info.Length() <= (i)) {Nan::ThrowError("Argument " #i " must be an integer"); return;} int64_t var = info[i]->NumberValue();
#define NAN_REQUIRE_ARGUMENT_BOOL(i, var) if (info.Length() <= (i)) {Nan::ThrowError("Argument " #i " must be a boolean"); return;} int var = info[i]->Int32Value();
#define NAN_REQUIRE_ARGUMENT_NUMBER(i, var) if (info.Length() <= (i)) {Nan::ThrowError("Argument " #i " must be a number"); return;} double var = info[i]->NumberValue();
#define NAN_REQUIRE_ARGUMENT_ARRAY(i, var) if (info.Length() <= (i) || !info[i]->IsArray()) {Nan::ThrowError("Argument " #i " must be an array"); return;} Local<Array> var = Local<Array>::Cast(info[i]);
#define NAN_REQUIRE_ARGUMENT_FUNCTION(i, var) if (info.Length() <= (i) || !info[i]->IsFunction()) {Nan::ThrowError("Argument " #i " must be a function"); return;} Local<Function> var = Local<Function>::Cast(info[i]);

#define NAN_EXPECT_ARGUMENT_FUNCTION(i, var) Local<Function> var; \
        if (info.Length() > 0 && info.Length() > (i) && !info[(i) >= 0 ? (i) : info.Length() - 1]->IsUndefined()) { \
            if (!info[(i) >= 0 ? (i) : info.Length() - 1]->IsFunction()) {Nan::ThrowError("Argument " #i " must be a function"); return;} \
            var = Local<Function>::Cast(info[(i) >= 0 ? (i) : info.Length() - 1]); }

#define NAN_OPTIONAL_ARGUMENT_FUNCTION(i, var) Local<Function> var; \
        if (info.Length() > 0 && info.Length() > (i) && info[(i) >= 0 ? (i) : info.Length() - 1]->IsFunction()) \
        var = Local<Function>::Cast(info[(i) >= 0 ? (i) : info.Length() - 1]);

#define NAN_OPTIONAL_ARGUMENT_INT(i, var) int var = (info.Length() > (i) && info[i]->IsInt32() ? info[i]->Int32Value() : 0);
#define NAN_OPTIONAL_ARGUMENT_UINT(i, var) unsigned int var = (info.Length() > (i) && info[i]->IsInt32() ? info[i]->Int32Value() : 0);
#define NAN_OPTIONAL_ARGUMENT_BOOL(i, var) int var = (info.Length() > (i) && info[i]->IsBoolean() ? info[i]->BooleanValue() : false);
#define NAN_OPTIONAL_ARGUMENT_BOOL2(i, var, dflt) int var = (info.Length() > (i) && info[i]->IsBoolean() ? info[i]->BooleanValue() : dflt);
#define NAN_OPTIONAL_ARGUMENT_INT2(i, var, dflt) int var = (info.Length() > (i) && info[i]->IsInt32() ? info[i]->Int32Value() : dflt);
#define NAN_OPTIONAL_ARGUMENT_UINT2(i, var, dflt) unsigned int var = (info.Length() > (i) && info[i]->IsInt32() ? info[i]->Int32Value() : dflt);
#define NAN_OPTIONAL_ARGUMENT_NUMBER(i, var) float var = (info.Length() > (i) && info[i]->IsNumber() ? info[i]->NumberValue() : 0);
#define NAN_OPTIONAL_ARGUMENT_STRING(i, var) Nan::Utf8String var(info.Length() > (i) && info[i]->IsString() ? info[i]->ToString() : Nan::EmptyString());
#define NAN_OPTIONAL_ARGUMENT_STRING2(i, var, dflt) Nan::Utf8String var(info.Length() > (i) && info[i]->IsString() ? info[i]->ToString() : dflt);
#define NAN_OPTIONAL_ARGUMENT_ARRAY(i, var) Local<Array> var(info.Length() > (i) && info[i]->IsArray() ? Local<Array>::Cast(info[i]) : Local<Array>::New(Array::New()));
#define NAN_OPTIONAL_ARGUMENT_OBJECT(i, var) Local<Object> var(info.Length() > (i) && info[i]->IsObject() ? Local<Object>::Cast(info[i]) : Local<Object>::New(Object::New()));
#define NAN_OPTIONAL_ARGUMENT_AS_STRING(i, var) Nan::Utf8String var(info.Length() > (i) ? info[i]->ToString() : Nan::EmptyString());
#define NAN_OPTIONAL_ARGUMENT_AS_BOOL(i, var) bool var = (info.Length() > (i) ? info[i]->BooleanValue() : false);
#define NAN_OPTIONAL_ARGUMENT_AS_INT(i, var) int var = (info.Length() > (i) ? info[i]->Int32Value() : 0);
#define NAN_OPTIONAL_ARGUMENT_AS_UINT(i, var) unsigned int var = (info.Length() > (i) ? info[i]->Int32Value() : 0);
#define NAN_OPTIONAL_ARGUMENT_AS_INT64(i, var) int64_t var = (info.Length() > (i) ? info[i]->NumberValue() : 0);
#define NAN_OPTIONAL_ARGUMENT_AS_UINT64(i, var) uint64_t var = (info.Length() > (i) ? info[i]->NumberValue() : 0);

#define NAN_GETOPTS_BOOL(obj,opts,name) if (!obj.IsEmpty()) { Local<String> name(String::New(#name)); if (obj->Has(name)) opts.name = obj->Get(name)->BooleanValue(); }
#define NAN_GETOPTS_INT(obj,opts,name) if (!obj.IsEmpty()) { Local<String> name(String::New(#name)); if (obj->Has(name)) opts.name = obj->Get(name)->ToInt32()->Value(); }
#define NAN_GETOPTS_INTVAL(obj,opts,name,expr) if (!obj.IsEmpty()) { Local<String> name(String::New(#name)); if (obj->Has(name)) { int val = obj->Get(name)->ToInt32()->Value(); opts.name = (expr); }}

#define NAN_TRY_CATCH_CALL(context, callback, argc, argv) { Nan::TryCatch try_catch; (callback)->Call((context), (argc), (argv)); if (try_catch.HasCaught()) FatalException(try_catch); }
#define NAN_TRY_CATCH_CALL_RETURN(context, callback, argc, argv, rc) { Nan::TryCatch try_catch; (callback)->Call((context), (argc), (argv)); if (try_catch.HasCaught()) { FatalException(try_catch); return rc; }}

#define NAN_DEFINE_CONSTANT_INTEGER(target, constant, name) Nan::ForceSet(target, Nan::New(#name).ToLocalChecked(), Nan::New(constant),static_cast<PropertyAttribute>(ReadOnly | DontDelete) );
#define NAN_DEFINE_CONSTANT_STRING(target, constant, name) Nan::ForceSet(target, Nan::New(#name).ToLocalChecked(), Nan::New(constant).ToLocalChecked(),static_cast<PropertyAttribute>(ReadOnly | DontDelete));

Local<Value> toArray(vector<string> &list, int numeric)
{
    Nan::EscapableHandleScope scope;
    Local<Array> rc = Nan::New<Array>(list.size());
    for (uint i = 0; i < list.size(); i++) {
        switch (numeric) {
        case 1:
            rc->Set(Nan::New(i), Nan::New(::atof(list[i].c_str())));
            break;

        case 2:
            rc->Set(Nan::New(i), Nan::New(::atof(list[i].c_str())));
            break;

        default:
            rc->Set(Nan::New(i), Nan::New(list[i].c_str()).ToLocalChecked());
        }
    }
    return scope.Escape(rc);
}

Local<Value> toArray(vector<pair<string,string> > &list)
{
    Nan::EscapableHandleScope scope;
    Local<Array> rc = Nan::New<Array>(list.size());
    for (uint i = 0; i < list.size(); i++) {
        Local<Object> obj = Nan::New<Object>();
        obj->Set(Nan::New("name").ToLocalChecked(), Nan::New(list[i].first.c_str()).ToLocalChecked());
        obj->Set(Nan::New("value").ToLocalChecked(), Nan::New(list[i].second.c_str()).ToLocalChecked());
        rc->Set(Nan::New(i), obj);
    }
    return scope.Escape(rc);
}

#endif

