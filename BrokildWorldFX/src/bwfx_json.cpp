#include "bwfx_json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace bwfx { namespace json
{

namespace
{
    struct Parser
    {
        const char* p;
        const char* end;
        int depth = 0;

        void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }

        bool lit (const char* s, size_t n)
        {
            if ((size_t) (end - p) < n) return false;
            for (size_t i = 0; i < n; ++i) if (p[i] != s[i]) return false;
            p += n; return true;
        }

        bool str (std::string& out)
        {
            if (p >= end || *p != '"') return false;
            ++p;
            out.clear();
            while (p < end && *p != '"')
            {
                if (*p == '\\')
                {
                    ++p;
                    if (p >= end) return false;
                    switch (*p)
                    {
                        case '"':  out += '"';  break;
                        case '\\': out += '\\'; break;
                        case '/':  out += '/';  break;
                        case 'b':  out += '\b'; break;
                        case 'f':  out += '\f'; break;
                        case 'n':  out += '\n'; break;
                        case 'r':  out += '\r'; break;
                        case 't':  out += '\t'; break;
                        case 'u':
                        {
                            if (end - p < 5) return false;
                            unsigned cp = 0;
                            for (int i = 1; i <= 4; ++i)
                            {
                                const char c = p[i];
                                cp <<= 4;
                                if (c >= '0' && c <= '9') cp |= (unsigned) (c - '0');
                                else if (c >= 'a' && c <= 'f') cp |= (unsigned) (c - 'a' + 10);
                                else if (c >= 'A' && c <= 'F') cp |= (unsigned) (c - 'A' + 10);
                                else return false;
                            }
                            p += 4;
                            // UTF-8 encode (surrogate pairs not recombined —
                            // state blobs never carry them)
                            if (cp < 0x80) out += (char) cp;
                            else if (cp < 0x800)
                            {
                                out += (char) (0xC0 | (cp >> 6));
                                out += (char) (0x80 | (cp & 0x3F));
                            }
                            else
                            {
                                out += (char) (0xE0 | (cp >> 12));
                                out += (char) (0x80 | ((cp >> 6) & 0x3F));
                                out += (char) (0x80 | (cp & 0x3F));
                            }
                            break;
                        }
                        default: return false;
                    }
                    ++p;
                }
                else out += *p++;
            }
            if (p >= end) return false;
            ++p;   // closing quote
            return true;
        }

        bool value (Value& v)
        {
            if (++depth > 32) return false;      // blobs are shallow; bail on abuse
            ws();
            if (p >= end) { --depth; return false; }
            bool ok = false;
            if (*p == '{')
            {
                ++p; v.t = Value::Obj;
                ws();
                if (p < end && *p == '}') { ++p; ok = true; }
                else while (p < end)
                {
                    std::string key;
                    ws();
                    if (! str (key)) break;
                    ws();
                    if (p >= end || *p != ':') break;
                    ++p;
                    Value child;
                    if (! value (child)) break;
                    v.obj.emplace_back (std::move (key), std::move (child));
                    ws();
                    if (p < end && *p == ',') { ++p; continue; }
                    if (p < end && *p == '}') { ++p; ok = true; }
                    break;
                }
            }
            else if (*p == '[')
            {
                ++p; v.t = Value::Arr;
                ws();
                if (p < end && *p == ']') { ++p; ok = true; }
                else while (p < end)
                {
                    Value child;
                    if (! value (child)) break;
                    v.arr.push_back (std::move (child));
                    ws();
                    if (p < end && *p == ',') { ++p; continue; }
                    if (p < end && *p == ']') { ++p; ok = true; }
                    break;
                }
            }
            else if (*p == '"') { v.t = Value::Str; ok = str (v.str); }
            else if (lit ("true", 4))  { v.t = Value::Bool; v.b = true;  ok = true; }
            else if (lit ("false", 5)) { v.t = Value::Bool; v.b = false; ok = true; }
            else if (lit ("null", 4))  { v.t = Value::Null; ok = true; }
            else
            {
                char* q = nullptr;
                const double d = std::strtod (p, &q);
                if (q != nullptr && q != p && q <= end && std::isfinite (d))
                {
                    v.t = Value::Num; v.num = d; p = q; ok = true;
                }
            }
            --depth;
            return ok;
        }
    };
}

bool parse (const std::string& s, Value& out)
{
    out = Value();
    Parser pr { s.c_str(), s.c_str() + s.size() };
    Value v;
    if (! pr.value (v)) { out = Value(); return false; }
    pr.ws();
    out = std::move (v);
    return true;
}

std::string escape (const std::string& s)
{
    std::string o;
    o.reserve (s.size() + 8);
    for (const char c : s)
    {
        switch (c)
        {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char) c < 0x20) { char buf[8]; std::snprintf (buf, sizeof (buf), "\\u%04x", c); o += buf; }
                else o += c;
        }
    }
    return o;
}

}} // namespace bwfx::json
