//
// Created by Qpromax on 2025/9/26.
//
#include "ulDBMS.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <numeric>

namespace ulDBMS {
    // Table 方法
    void Table::build_index()
    {
        col_index.clear();
        for (size_t i = 0; i < columns.size(); ++i) col_index[columns[i].name] = i;
    }

    // MiniDB 方法
    bool MiniDB::create_table(const std::string& name, const std::vector<Column>& cols)
    {
        if (tables_.count(name)) return false;
        Table t;
        t.name = name;
        t.columns = cols;
        t.build_index();
        tables_.emplace(name, std::move(t));
        return true;
    }

    bool MiniDB::drop_table(const std::string& name)
    {
        return tables_.erase(name) > 0;
    }

    bool MiniDB::insert(const std::string& table, const Row& values)
    {
        auto it = tables_.find(table);
        if (it == tables_.end()) return false;
        Table& t = it->second;
        Row r = values;
        if (r.size() < t.columns.size()) r.resize(t.columns.size(), Value(Null{}));
        t.rows.push_back(std::move(r));
        return true;
    }

    size_t MiniDB::remove(const std::string& table, const std::vector<Condition>& conds)
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

    size_t MiniDB::update(const std::string& table, const std::vector<Condition>& conds,
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

    std::vector<Row> MiniDB::select(const std::string& table, const std::vector<std::string>& proj_cols,
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

    std::vector<Column> MiniDB::columns(const std::string& table) const
    {
        auto it = tables_.find(table);
        if (it == tables_.end()) return {};
        return it->second.columns;
    }

    bool MiniDB::save_to_disk(const std::string& dir) const
    {
        for (auto& p: tables_)
        {
            const Table& t = p.second;
            std::string fn = dir + t.name + ".tbl";
            std::ofstream ofs(fn, std::ios::trunc);
            if (!ofs) return false;
            for (size_t i = 0; i < t.columns.size(); ++i)
            {
                if (i) ofs << '\t';
                ofs << t.columns[i].name << ":" << type_to_string(t.columns[i].type);
            }
            ofs << '\n';
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

    bool MiniDB::load_from_disk(const std::string& dir)
    {
        return true; // 占位
    }

    bool MiniDB::load_table(const std::string& name, const std::string& dir)
    {
        std::string fn = dir + name + ".tbl";
        std::ifstream ifs(fn);
        if (!ifs) return false;
        Table t;
        t.name = name;
        std::string line;
        if (!std::getline(ifs, line)) return false;
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
            t.columns.push_back({col, ct});
        }
        while (std::getline(ifs, line))
        {
            std::istringstream ss(line);
            Row r;
            while (std::getline(ss, token, '\t')) r.push_back(deserialize_value(token));
            if (r.size() < t.columns.size()) r.resize(t.columns.size(), Value(Null{}));
            t.rows.push_back(std::move(r));
        }
        t.build_index();
        tables_[name] = std::move(t);
        return true;
    }

    // 辅助函数
    std::string MiniDB::serialize_value(const Value& v)
    {
        if (std::holds_alternative<Null>(v)) return "__NULL__";
        if (std::holds_alternative<Integer>(v)) return "I:" + std::to_string(std::get<Integer>(v));
        if (std::holds_alternative<Real>(v)) return "R:" + std::to_string(std::get<Real>(v));
        if (std::holds_alternative<Text>(v))
        {
            std::string s = std::get<Text>(v);
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

    Value MiniDB::deserialize_value(const std::string& s)
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

    bool MiniDB::compare_value(const Value& a, Op op, const Value& b)
    {
        if (std::holds_alternative<Null>(a) || std::holds_alternative<Null>(b))
        {
            if (op == Op::EQ) return a.index() == b.index();
            if (op == Op::NEQ) return a.index() != b.index();
            return false;
        }
        if (std::holds_alternative<Integer>(a) && std::holds_alternative<Real>(b))
            return compare_double(double(std::get<Integer>(a)), op, std::get<Real>(b));
        if (std::holds_alternative<Real>(a) && std::holds_alternative<Integer>(b))
            return compare_double(std::get<Real>(a), op, double(std::get<Integer>(b)));
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

    bool MiniDB::compare_double(double a, Op op, double b)
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

    bool MiniDB::matches(const Table& t, const Row& r, const std::vector<Condition>& conds)
    {
        for (auto& c: conds)
        {
            auto cit = t.col_index.find(c.column);
            if (cit == t.col_index.end()) return false;
            if (!compare_value(r[cit->second], c.op, c.value)) return false;
        }
        return true;
    }

    // 工具函数
    std::string to_string(const Value& v)
    {
        if (std::holds_alternative<Null>(v)) return "NULL";
        if (std::holds_alternative<Integer>(v)) return std::to_string(std::get<Integer>(v));
        if (std::holds_alternative<Real>(v)) return std::to_string(std::get<Real>(v));
        if (std::holds_alternative<Text>(v)) return std::get<Text>(v);
        if (std::holds_alternative<Bool>(v)) return std::get<Bool>(v) ? "true" : "false";
        return "?";
    }

    Type value_type(const Value& v)
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

    std::string type_to_string(Type t)
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
} // namespace ulDBMS
