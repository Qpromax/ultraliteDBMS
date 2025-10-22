//
// Created by Qpromax on 2025/9/26.
//
#pragma once

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <cstdint>

namespace ulDBMS {
    // 支持的数据类型
    using Integer = std::int64_t;
    using Real = double;
    using Text = std::string;
    using Bool = bool;
    using Null = std::monostate; // 用于表示 null

    using Value = std::variant<Null, Integer, Real, Text, Bool>;

    enum class Type { Null, Integer, Real, Text, Bool };

    enum class Op { EQ, NEQ, LT, LTE, GT, GTE };

    struct Column {
        std::string name;
        Type type;
    };

    struct Condition {
        std::string column;
        Op op;
        Value value;
    };

    using Row = std::vector<Value>;

    struct Table {
        std::string name;
        std::vector<Column> columns;
        std::vector<Row> rows;
        std::unordered_map<std::string, size_t> col_index;

        void build_index();
    };

    class MiniDB {
    public:
        MiniDB() = default;

        // 表操作
        bool create_table(const std::string& name, const std::vector<Column>& cols);

        bool drop_table(const std::string& name);

        // 数据操作
        bool insert(const std::string& table, const Row& values);

        size_t remove(const std::string& table, const std::vector<Condition>& conds);

        size_t update(const std::string& table, const std::vector<Condition>& conds,
                      const std::unordered_map<std::string, Value>& col_updates);

        std::vector<Row> select(const std::string& table, const std::vector<std::string>& proj_cols,
                                const std::vector<Condition>& conds);

        // 列信息
        std::vector<Column> columns(const std::string& table) const;

        // 持久化
        bool save_to_disk(const std::string& dir = "./") const;

        bool load_from_disk(const std::string& dir = "./"); // 预留接口
        bool load_table(const std::string& name, const std::string& dir = "./");

    private:
        std::unordered_map<std::string, Table> tables_;

        // 辅助函数
        static std::string serialize_value(const Value& v);

        static Value deserialize_value(const std::string& s);

        static bool compare_value(const Value& a, Op op, const Value& b);

        static bool compare_double(double a, Op op, double b);

        static bool matches(const Table& t, const Row& r, const std::vector<Condition>& conds);
    };

    // 小工具：打印 Value
    std::string to_string(const Value& v);

    // 类型工具
    Type value_type(const Value& v);

    std::string type_to_string(Type t);
} // namespace ulDBMS
