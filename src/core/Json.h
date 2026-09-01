// Json.h —— 极简 JSON 读写(文档持久化用, 零依赖)
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <functional>

namespace cad::json {

class Value;
using ValuePtr = std::shared_ptr<Value>;
using Object = std::map<std::string, ValuePtr>;
using Array = std::vector<ValuePtr>;

struct Value {
    enum Type { Null, Bool, Number, String, Arr, Obj } type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::shared_ptr<Array> arr;
    std::shared_ptr<Object> obj;

    Value() { init(Type::Null); }
    explicit Value(Type t) { init(t); }

    void init(Type t) {
        type = t;
        if (t == Arr && !arr) arr = std::make_shared<Array>();
        if (t == Obj && !obj) obj = std::make_shared<Object>();
    }
    bool isNull() const { return type == Null; }
    bool isNum() const { return type == Number; }

    // ---- 取值快捷方式 ----
    double number(double def = 0) const { return type == Number ? num : def; }
    bool boolean(bool def = false) const { return type == Bool ? b : def; }
    bool getBool(const std::string& k, bool def = false) const {
        auto v = get(k);
        return v ? v->boolean(def) : def;
    }
    std::string string(const std::string& def = {}) const { return type == String ? str : def; }

    ValuePtr get(const std::string& k) const {
        if (type != Obj) return nullptr;
        auto it = obj->find(k);
        return it == obj->end() ? nullptr : it->second;
    }
    double getNum(const std::string& k, double def = 0) const {
        auto v = get(k); return v ? v->number(def) : def;
    }
    std::string getStr(const std::string& k, const std::string& def = {}) const {
        auto v = get(k); return v ? v->string(def) : def;
    }

    // ---- 构造快捷 ----
    static ValuePtr mkNum(double d) { auto v = std::make_shared<Value>(Number); v->num = d; return v; }
    static ValuePtr mkBool(bool b) { auto v = std::make_shared<Value>(Bool); v->b = b; return v; }
    static ValuePtr mkStr(const std::string& s) { auto v = std::make_shared<Value>(String); v->str = s; return v; }
    static ValuePtr mkArr() { return std::make_shared<Value>(Arr); }
    static ValuePtr mkObj() { return std::make_shared<Value>(Obj); }
};

inline void set(ValuePtr obj, const std::string& k, double v) { (*obj->obj)[k] = Value::mkNum(v); }
inline void set(ValuePtr obj, const std::string& k, int v) { (*obj->obj)[k] = Value::mkNum(v); }
inline void set(ValuePtr obj, const std::string& k, const std::string& v) { (*obj->obj)[k] = Value::mkStr(v); }
inline void set(ValuePtr obj, const std::string& k, const char* v) { (*obj->obj)[k] = Value::mkStr(v); }
inline void set(ValuePtr obj, const std::string& k, bool v) { (*obj->obj)[k] = Value::mkBool(v); }
inline void setV(ValuePtr obj, const std::string& k, ValuePtr v) { (*obj->obj)[k] = std::move(v); }
inline void push(ValuePtr arr, ValuePtr v) { arr->arr->push_back(std::move(v)); }

// ---------------- 写入 ----------------
inline std::string dump(const ValuePtr& v) {
    std::string out;
    out.reserve(256);
    std::function<void(const ValuePtr&)> wr = [&](const ValuePtr& n) {
        char buf[64];
        if (!n) {
            out += "null";
            return;
        }
        switch (n->type) {
            case Value::Null: out += "null"; break;
            case Value::Bool: out += n->b ? "true" : "false"; break;
            case Value::Number: {
                double d = n->num;
                if (d == (long long)d && std::fabs(d) < 9e15)
                    snprintf(buf, sizeof(buf), "%lld", (long long)d);
                else
                    snprintf(buf, sizeof(buf), "%.10g", d);
                out += buf;
                break;
            }
            case Value::String: {
                out += '"';
                for (char c : n->str) {
                    switch (c) {
                        case '"': out += "\\\""; break;
                        case '\\': out += "\\\\"; break;
                        case '\n': out += "\\n"; break;
                        case '\t': out += "\\t"; break;
                        case '\r': out += "\\r"; break;
                        default:
                            if ((unsigned char)c < 0x20) {
                                snprintf(buf, sizeof(buf), "\\u%04x", c);
                                out += buf;
                            } else out += c;
                    }
                }
                out += '"';
                break;
            }
            case Value::Arr: {
                out += '[';
                bool first = true;
                for (auto& e : *n->arr) {
                    if (!first) out += ',';
                    first = false;
                    wr(e);
                }
                out += ']';
                break;
            }
            case Value::Obj: {
                out += '{';
                bool first2 = true;
                for (auto& kv : *n->obj) {
                    if (!first2) out += ',';
                    first2 = false;
                    ValuePtr key = Value::mkStr(kv.first);
                    wr(key);
                    out += ':';
                    wr(kv.second);
                }
                out += '}';
                break;
            }
        }
    };
    wr(v);
    return out;
}

// ---------------- 解析 ----------------
class Parser {
public:
    static ValuePtr parse(const std::string& text) {
        Parser p(text);
        p.skipWs();
        auto v = p.parseValue();
        p.skipWs();
        return v;
    }

private:
    explicit Parser(std::string s) : s_(std::move(s)) {}
    std::string s_;   // 必须按值持有(引用会悬空指向已销毁的形参)
    size_t i_ = 0;

    void skipWs() { while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_; }
    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
    char next() { return i_ < s_.size() ? s_[i_++] : '\0'; }
    [[noreturn]] void fail(const char* msg) { throw std::runtime_error(std::string("JSON: ") + msg + " @ " + std::to_string(i_)); }

    ValuePtr parseValue() {
        skipWs();
        char c = peek();
        if (c == '{') return parseObj();
        if (c == '[') return parseArr();
        if (c == '"') return Value::mkStr(parseStr());
        if (c == 't') { expect("true"); return Value::mkBool(true); }
        if (c == 'f') { expect("false"); return Value::mkBool(false); }
        if (c == 'n') { expect("null"); return std::make_shared<Value>(); }
        return parseNum();
    }
    void expect(const char* lit) {
        for (const char* p = lit; *p; ++p)
            if (next() != *p) fail("literal");
    }
    ValuePtr parseObj() {
        auto v = Value::mkObj();
        next(); // {
        skipWs();
        if (peek() == '}') { next(); return v; }
        for (;;) {
            skipWs();
            if (peek() != '"') fail("key");
            std::string k = parseStr();
            skipWs();
            if (next() != ':') fail("colon");
            setV(v, k, parseValue());
            skipWs();
            char c = next();
            if (c == '}') break;
            if (c != ',') fail("object");
        }
        return v;
    }
    ValuePtr parseArr() {
        auto v = Value::mkArr();
        next(); // [
        skipWs();
        if (peek() == ']') { next(); return v; }
        for (;;) {
            push(v, parseValue());
            skipWs();
            char c = next();
            if (c == ']') break;
            if (c != ',') fail("array");
        }
        return v;
    }
    std::string parseStr() {
        std::string out;
        next(); // "
        while (i_ < s_.size()) {
            char c = next();
            if (c == '"') return out;
            if (c == '\\') {
                char e = next();
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'u': {
                        if (i_ + 4 > s_.size()) fail("unicode");
                        unsigned cp = std::stoul(s_.substr(i_, 4), nullptr, 16);
                        i_ += 4;
                        // 代理对
                        if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 6 <= s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                            unsigned lo = std::stoul(s_.substr(i_ + 2, 4), nullptr, 16);
                            i_ += 6;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        // UTF-8 编码
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 63));
                        } else if (cp < 0x10000) {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 63));
                            out += (char)(0x80 | (cp & 63));
                        } else {
                            out += (char)(0xF0 | (cp >> 18));
                            out += (char)(0x80 | ((cp >> 12) & 63));
                            out += (char)(0x80 | ((cp >> 6) & 63));
                            out += (char)(0x80 | (cp & 63));
                        }
                        break;
                    }
                    default: fail("escape");
                }
            } else out += c;
        }
        fail("string");
    }
    ValuePtr parseNum() {
        size_t b = i_;
        if (peek() == '-') ++i_;
        while (i_ < s_.size() && (isdigit((unsigned char)s_[i_]) || s_[i_] == '.' || s_[i_] == 'e' ||
                                  s_[i_] == 'E' || s_[i_] == '+' || s_[i_] == '-'))
            ++i_;
        if (b == i_) fail("number");
        return Value::mkNum(std::stod(s_.substr(b, i_ - b)));
    }
};

} // namespace cad::json
