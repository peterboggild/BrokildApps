#pragma once

// Minimal JSON for the BWFX state blob. Message thread only. Tolerant by
// design: unknown keys are simply never looked up, malformed input yields
// an empty value and the rack keeps its defaults.

#include <string>
#include <utility>
#include <vector>

namespace bwfx { namespace json
{

struct Value
{
    enum T { Null, Bool, Num, Str, Arr, Obj };
    T t = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj;

    bool isObj() const { return t == Obj; }
    bool isArr() const { return t == Arr; }
    const Value* find (const std::string& k) const
    {
        if (t != Obj) return nullptr;
        for (auto& kv : obj)
            if (kv.first == k) return &kv.second;
        return nullptr;
    }
    double asNum (double def) const { return t == Num ? num : (t == Bool ? (b ? 1.0 : 0.0) : def); }
    std::string asStr (const char* def) const { return t == Str ? str : std::string (def); }
};

// Returns false on malformed input (out is left Null).
bool parse (const std::string& s, Value& out);

// Escape a string for embedding in JSON output (adds no quotes).
std::string escape (const std::string& s);

}} // namespace bwfx::json
