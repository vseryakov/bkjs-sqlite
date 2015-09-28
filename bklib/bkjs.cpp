//
//  Author: Vlad Seryakov vseryakov@gmail.com
//  April 2013
//

#include "bkjs.h"

string stringifyJSON(Local<Value> obj)
{
    Nan::EscapableHandleScope scope;
    Local<Value> argv[1] = { obj };
    Handle<Object> JSON = Context::GetCurrent()->Global()->Get(Nan::New("JSON").ToLocalChecked())->ToObject();
    Handle<Function> JSON_stringify = Handle<Function>::Cast(JSON->Get(Nan::New("stringify").ToLocalChecked()));
    Local<Value> val = Local<Value>::New(JSON_stringify->Call(JSON, 1, argv));
    Nan::Utf8String json(val);
    return *json;
}

Handle<Value> parseJSON(const char* str)
{
    Nan::EscapableHandleScope scope;
    Local<Value> argv[1] = { Nan::New(str).ToLocalChecked() };
    Handle<Object> JSON = Context::GetCurrent()->Global()->Get(Nan::New("JSON").ToLocalChecked())->ToObject();
    Handle<Function> JSON_parse = Handle<Function>::Cast(JSON->Get(Nan::New("parse").ToLocalChecked()));
    Local<Value> val;
    {
        Nan::TryCatch try_catch;
        val = Local<Value>::New(JSON_parse->Call(JSON, 1, argv));
        if (try_catch.HasCaught()) val = Local<Value>::New(Null());
    }
    return scope.Escape(val);
}

Local<Value> toArray(vector<string> &list, int numeric)
{
    Nan::EscapableHandleScope scope;
    Local<Array> rc = Local<Array>::New(Array::New(list.size()));
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
    Local<Array> rc = Local<Array>::New(Array::New(list.size()));
    for (uint i = 0; i < list.size(); i++) {
        Local<Object> obj = Local<Object>::New(Object::New());
        obj->Set(Nan::New("name").ToLocalChecked(), Nan::New(list[i].first.c_str()).ToLocalChecked());
        obj->Set(Nan::New("value").ToLocalChecked(), Nan::New(list[i].second.c_str()).ToLocalChecked());
        rc->Set(Nan::New(i), obj);
    }
    return scope.Escape(rc);
}
