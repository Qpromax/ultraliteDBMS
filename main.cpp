// minisql.cpp
// 超轻量级 "类 SQL" 小型数据库 — 现代 C++（单文件、无外部依赖）
// 功能：
//  - 支持表的创建/删除
//  - 支持插入/更新/删除/查询（带简单的 WHERE 条件：=, !=, <, <=, >, >=）
//  - 列类型：INTEGER, REAL, TEXT, BOOL, NULL
//  - 内存为主，提供简单的磁盘序列化（每个表为一个文本文件）
//  - 以 API 调用为主（而非完整 SQL 字符串解析），易于扩展为解析器
// 编译要求：C++17 或更新

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <numeric>

namespace minisql {
    // 支持的数据类型
    using Integer = int64_t;
    using Real = double;
    using Text = std::string;
    using Bool = bool;
    using Null = std::monostate; // 用于表示 null

    using Value = std::variant<Null, Integer, Real, Text, Bool>;

    enum class Type { Null, Integer, Real, Text, Bool };

    static Type value_type(const Value& v)
    {
        switch (v.index())
        {
            case 0: return Type::Null;
            case 1: return Type::Integer;
            case 2: return Type::Real;
            case 3: return Type::Text;
            case 4: return Type::Bool;
            default: return Type::Null;
        }
    }

    static std::string type_to_string(Type t)
    {
        switch (t)
        {
            case Type::Null: return "NULL";
            case Type::Integer: return "INTEGER";
            case Type::Real: return "REAL";
            case Type::Text: return "TEXT";
            case Type::Bool: return "BOOL";
        }
        return "UNKNOWN";
    }

    // 行（列值向量）
    using Row = std::vector<Value>;

    struct Column {
        std::string name;
        Type type;
    };

    // 简单的比较操作符
    enum class Op { EQ, NEQ, LT, LTE, GT, GTE };

    struct Condition {
        std::string column;
        Op op;
        Value value;
    };

    struct Table {
        std::string name;
        std::vector<Column> columns;
        std::vector<Row> rows;

        // 列名 -> 索引
        std::unordered_map<std::string, size_t> col_index;

        void build_index()
        {
            col_index.clear();
            for (size_t i = 0; i < columns.size(); ++i) col_index[columns[i].name] = i;
        }
    };

    class MiniDB {
    public:
        MiniDB() = default;

        // 创建表
        bool create_table(const std::string& name, const std::vector<Column>& cols)
        {
            if (tables_.count(name)) return false; // 已存在
            Table t;
            t.name = name;
            t.columns = cols;
            t.build_index();
            tables_.emplace(name, std::move(t));
            return true;
        }

        // 删除表
        bool drop_table(const std::string& name)
        {
            return tables_.erase(name) > 0;
        }

        // 插入一行：values 要与列数一致，或可接受缺省（补 null）
        bool insert(const std::string& table, const Row& values)
        {
            auto it = tables_.find(table);
            if (it == tables_.end()) return false;
            Table& t = it->second;
            Row r = values;
            if (r.size() < t.columns.size()) r.resize(t.columns.size(), Value(Null{}));
            // simple type checks: allow numeric widening (int->real) not implemented; keep simple
            t.rows.push_back(std::move(r));
            return true;
        }

        // 简单删除：按条件删除，返回删除的行数
        size_t remove(const std::string& table, const std::vector<Condition>& conds)
        {
            auto it = tables_.find(table);
            if (it == tables_.end()) return 0;
            Table& t = it->second;
            auto pred = [&](const Row& r) { return matches(t, r, conds); };
            size_t before = t.rows.size();
            std::vector<Row> keep;
            keep.reserve(t.rows.size());
            for (auto& r: t.rows) if (!pred(r)) keep.push_back(r);
            t.rows.swap(keep);
            return before - t.rows.size();
        }

        // 更新：将满足条件的行中某些列修改为给定值；col_updates: 列名->Value
        size_t update(const std::string& table, const std::vector<Condition>& conds,
                      const std::unordered_map<std::string, Value>& col_updates)
        {
            auto it = tables_.find(table);
            if (it == tables_.end()) return 0;
            Table& t = it->second;
            size_t changed = 0;
            for (auto& r: t.rows)
            {
                if (matches(t, r, conds))
                {
                    for (auto& p: col_updates)
                    {
                        auto cit = t.col_index.find(p.first);
                        if (cit == t.col_index.end()) continue;
                        r[cit->second] = p.second;
                    }
                    ++changed;
                }
            }
            return changed;
        }

        // 查询：返回投影列（列名列表，空=>全部）以及满足条件的所有行
        std::vector<Row> select(const std::string& table, const std::vector<std::string>& proj_cols,
                                const std::vector<Condition>& conds)
        {
            std::vector<Row> out;
            auto it = tables_.find(table);
            if (it == tables_.end()) return out;
            Table& t = it->second;

            std::vector<size_t> proj_idx;
            if (proj_cols.empty())
            {
                proj_idx.resize(t.columns.size());
                std::iota(proj_idx.begin(), proj_idx.end(), 0);
            } else
            {
                for (auto& c: proj_cols)
                {
                    auto cit = t.col_index.find(c);
                    if (cit != t.col_index.end()) proj_idx.push_back(cit->second);
                }
            }

            for (auto& r: t.rows)
            {
                if (matches(t, r, conds))
                {
                    Row outrow;
                    outrow.reserve(proj_idx.size());
                    for (auto idx: proj_idx) outrow.push_back(r[idx]);
                    out.push_back(std::move(outrow));
                }
            }
            return out;
        }

        // 列信息
        std::vector<Column> columns(const std::string& table) const
        {
            auto it = tables_.find(table);
            if (it == tables_.end()) return {};
            return it->second.columns;
        }

        // 简单持久化：把每个表写为 <name>.tbl 文件
        bool save_to_disk(const std::string& dir = "./") const
        {
            for (auto& p: tables_)
            {
                const Table& t = p.second;
                std::string fn = dir + t.name + ".tbl";
                std::ofstream ofs(fn, std::ios::trunc);
                if (!ofs) return false;
                // header
                for (size_t i = 0; i < t.columns.size(); ++i)
                {
                    if (i) ofs << '\t';
                    ofs << t.columns[i].name << ":" << type_to_string(t.columns[i].type);
                }
                ofs << '\n';
                // rows: 转义 tab/newline
                for (auto& r: t.rows)
                {
                    for (size_t i = 0; i < r.size(); ++i)
                    {
                        if (i) ofs << '\t';
                        ofs << serialize_value(r[i]);
                    }
                    ofs << '\n';
                }
            }
            return true;
        }

        // 从磁盘加载简单表格，如果文件不存在则跳过
        bool load_from_disk(const std::string& dir = "./")
        {
            // naive: 读取当前目录中以 .tbl 结尾的文件——但标准 C++ 没有目录遍历（避免外部依赖），
            // 所以这里要求用户传入明确想 load 的表名列表或直接调用 load_table
            return true;
        }

        // 加载单个表（从 name.tbl）
        bool load_table(const std::string& name, const std::string& dir = "./")
        {
            std::string fn = dir + name + ".tbl";
            std::ifstream ifs(fn);
            if (!ifs) return false;
            Table t;
            t.name = name;
            std::string line;
            // header
            if (!std::getline(ifs, line)) return false; {
                std::istringstream ss(line);
                std::string token;
                while (std::getline(ss, token, '\t'))
                {
                    auto pos = token.find(':');
                    if (pos == std::string::npos) return false;
                    std::string col = token.substr(0, pos);
                    std::string typ = token.substr(pos + 1);
                    Type ct = Type::Text;
                    if (typ == "INTEGER") ct = Type::Integer;
                    else if (typ == "REAL") ct = Type::Real;
                    else if (typ == "TEXT") ct = Type::Text;
                    else if (typ == "BOOL") ct = Type::Bool;
                    else ct = Type::Text;
                    t.columns.push_back({col, ct});
                }
            }
            while (std::getline(ifs, line))
            {
                std::istringstream ss(line);
                std::string token;
                Row r;
                while (std::getline(ss, token, '\t')) r.push_back(deserialize_value(token));
                // pad
                if (r.size() < t.columns.size()) r.resize(t.columns.size(), Value(Null{}));
                t.rows.push_back(std::move(r));
            }
            t.build_index();
            tables_[name] = std::move(t);
            return true;
        }

    private:
        std::unordered_map<std::string, Table> tables_;

        static std::string serialize_value(const Value& v)
        {
            if (std::holds_alternative<Null>(v)) return "__NULL__";
            if (std::holds_alternative<Integer>(v)) return "I:" + std::to_string(std::get<Integer>(v));
            if (std::holds_alternative<Real>(v)) return "R:" + std::to_string(std::get<Real>(v));
            if (std::holds_alternative<Text>(v))
            {
                std::string s = std::get<Text>(v);
                // 简单转义 \ -> \\ , \n -> \n , \t -> \t
                std::string out;
                for (char c: s)
                {
                    if (c == '\\') out += "\\\\";
                    else if (c == '\n') out += "\\n";
                    else if (c == '\t') out += "\\t";
                    else out += c;
                }
                return "T:" + out;
            }
            if (std::holds_alternative<Bool>(v)) return std::string("B:") + (std::get<Bool>(v) ? "1" : "0");
            return "__NULL__";
        }

        static Value deserialize_value(const std::string& s)
        {
            if (s == "__NULL__") return Null{};
            if (s.size() >= 2 && s[1] == ':')
            {
                char t = s[0];
                std::string payload = s.substr(2);
                if (t == 'I') return Integer(std::stoll(payload));
                if (t == 'R') return Real(std::stod(payload));
                if (t == 'B') return Bool(payload == "1");
                if (t == 'T')
                {
                    std::string out;
                    for (size_t i = 0; i < payload.size(); ++i)
                    {
                        char c = payload[i];
                        if (c == '\\' && i + 1 < payload.size())
                        {
                            char n = payload[++i];
                            if (n == 'n') out += '\n';
                            else if (n == 't') out += '\t';
                            else if (n == '\\') out += '\\';
                            else out += n;
                        } else out += c;
                    }
                    return Text(out);
                }
            }
            return Null{};
        }

        // matches helper
        static bool compare_value(const Value& a, Op op, const Value& b)
        {
            // 只处理同类型或可比较的基本类型（int<->real 支持）
            if (std::holds_alternative<Null>(a) || std::holds_alternative<Null>(b))
            {
                if (op == Op::EQ) return a.index() == b.index();
                if (op == Op::NEQ) return a.index() != b.index();
                return false;
            }
            // Integer <-> Real
            if (std::holds_alternative<Integer>(a) && std::holds_alternative<Real>(b))
            {
                double da = double(std::get<Integer>(a));
                double db = std::get<Real>(b);
                return compare_double(da, op, db);
            }
            if (std::holds_alternative<Real>(a) && std::holds_alternative<Integer>(b))
            {
                double da = std::get<Real>(a);
                double db = double(std::get<Integer>(b));
                return compare_double(da, op, db);
            }
            if (std::holds_alternative<Integer>(a) && std::holds_alternative<Integer>(b))
                return compare_double(double(std::get<Integer>(a)), op, double(std::get<Integer>(b)));
            if (std::holds_alternative<Real>(a) && std::holds_alternative<Real>(b))
                return compare_double(std::get<Real>(a), op, std::get<Real>(b));
            if (std::holds_alternative<Text>(a) && std::holds_alternative<Text>(b))
            {
                const std::string& sa = std::get<Text>(a);
                const std::string& sb = std::get<Text>(b);
                switch (op)
                {
                    case Op::EQ: return sa == sb;
                    case Op::NEQ: return sa != sb;
                    case Op::LT: return sa < sb;
                    case Op::LTE: return sa <= sb;
                    case Op::GT: return sa > sb;
                    case Op::GTE: return sa >= sb;
                }
            }
            if (std::holds_alternative<Bool>(a) && std::holds_alternative<Bool>(b))
            {
                bool ba = std::get<Bool>(a), bb = std::get<Bool>(b);
                switch (op)
                {
                    case Op::EQ: return ba == bb;
                    case Op::NEQ: return ba != bb;
                    case Op::LT: return ba < bb;
                    case Op::LTE: return ba <= bb;
                    case Op::GT: return ba > bb;
                    case Op::GTE: return ba >= bb;
                }
            }
            return false;
        }

        static bool compare_double(double a, Op op, double b)
        {
            switch (op)
            {
                case Op::EQ: return a == b;
                case Op::NEQ: return a != b;
                case Op::LT: return a < b;
                case Op::LTE: return a <= b;
                case Op::GT: return a > b;
                case Op::GTE: return a >= b;
            }
            return false;
        }

        static bool matches(const Table& t, const Row& r, const std::vector<Condition>& conds)
        {
            for (auto& c: conds)
            {
                auto cit = t.col_index.find(c.column);
                if (cit == t.col_index.end()) return false; // 列不存在 => 不匹配
                const Value& v = r[cit->second];
                if (!compare_value(v, c.op, c.value)) return false;
            }
            return true;
        }
    };

    // 小工具：将 Value 打印为字符串（调试）
    static std::string to_string(const Value& v)
    {
        if (std::holds_alternative<Null>(v)) return "NULL";
        if (std::holds_alternative<Integer>(v)) return std::to_string(std::get<Integer>(v));
        if (std::holds_alternative<Real>(v)) return std::to_string(std::get<Real>(v));
        if (std::holds_alternative<Text>(v)) return std::get<Text>(v);
        if (std::holds_alternative<Bool>(v)) return std::get<Bool>(v) ? "true" : "false";
        return "?";
    }
} // namespace minisql

// 示例用法
#ifdef MINI_SQL_EXAMPLE
int main()
{
    using namespace minisql;
    MiniDB db;
    db.create_table("users", {
                        {"id", Type::Integer}, {"name", Type::Text}, {"age", Type::Integer}, {"active", Type::Bool}
                    });
    db.insert("users", {Integer(1), Text("alice"), Integer(30), Bool(true)});
    db.insert("users", {Integer(2), Text("bob"), Integer(25), Bool(false)});
    db.insert("users", {Integer(3), Text("cathy"), Integer(20), Bool(true)});

    auto res = db.select("users", {"id", "name"}, {{"age", Op::GT, Integer(21)}});
    for (auto& r: res)
    {
        for (auto& v: r) std::cout << to_string(v) << "\t";
        std::cout << "\n";
    }

    db.update("users", {{"name", Op::EQ, Text("bob")}}, {{"active", Bool(true)}});
    db.remove("users", {{"age", Op::LT, Integer(21)}});

    db.save_to_disk("./");
    return 0;
}
#endif
