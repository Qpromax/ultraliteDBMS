#include "ulDBMS.h"
#include <iostream>

int main() {
    using namespace ulDBMS;
    MiniDB db;
    db.create_table("users", {{"id", Type::Integer}, {"name", Type::Text}, {"age", Type::Integer}, {"active", Type::Bool}});
    db.insert("users", {Integer(1), Text("alice"), Integer(30), Bool(true)});
    db.insert("users", {Integer(2), Text("bob"), Integer(25), Bool(false)});
    db.insert("users", {Integer(3), Text("cathy"), Integer(20), Bool(true)});

    auto res = db.select("users", {"id", "name"}, {{"age", Op::GT, Integer(21)}});
    for (auto &r : res) {
        for (auto &v : r) std::cout << to_string(v) << "\t";
        std::cout << "\n";
    }

    db.update("users", {{"name", Op::EQ, Text("bob")}}, {{"active", Bool(true)}});
    db.remove("users", {{"age", Op::LT, Integer(21)}});

    db.save_to_disk("./");
    return 0;
}
