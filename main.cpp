#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <format>

#include "imgui.h"
#include "imgui_backends/imgui_impl_glfw.h"
#include "imgui_backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>


class List {
private:
    std::map<int, std::vector<std::string> > list_data_;
    std::string list_name_;
    int list_key_;
    std::vector<std::array<char, 256> > list_values_;

public:
    List() = default;

    template<typename T>
    List(T&& list_data, std::string list_name)
        : list_data_(std::forward<T>(list_data)), list_name_(std::move(list_name)), list_key_(0)
    {
        if (!list_data_.empty())
            list_values_ = std::vector<std::array<char, 256> >(list_data_.begin()->second.size());
    }

    List(const std::string& list_name, int key, const std::string& raw_data)
        : list_name_(list_name), list_key_(0)
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream ss(raw_data);

        while (std::getline(ss, token, ';'))
        {
            if (!token.empty())
            {
                tokens.push_back(token);
            }
        }

        list_data_[key] = std::move(tokens);
        list_values_ = std::vector<std::array<char, 256> >(list_data_.begin()->second.size(), std::array<char, 256>{});
    }

    ~List() = default;

    void list_show()
    {
        auto str_vec_size_ = list_data_.begin()->second.size();

        ImGui::Begin(list_name_.c_str());
        if (!list_data_.empty())
        {
            if (ImGui::BeginTable("table", 1 + str_vec_size_, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                // 表头
                ImGui::TableSetupColumn("KEY");
                for (auto i = 1; i < 1 + str_vec_size_; i++)
                {
                    std::string temp = std::format("VALUE{}", i);
                    ImGui::TableSetupColumn(temp.c_str());
                }
                ImGui::TableHeadersRow();

                // 数据
                auto temp_i_ = 0;
                for (const auto& [k, v]: list_data_)
                {
                    if (k == list_key_)
                    {
                        ImGui::TableNextRow();
                        temp_i_ = temp_i_ % v.size();
                        ImGui::TableSetColumnIndex(temp_i_);
                        ImVec4 highlightColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // 黄色
                        ImGui::TextColored(highlightColor, "%d", k);
                        for (const auto& _sub_value: v)
                        {
                            ++temp_i_;
                            ImGui::TableSetColumnIndex(temp_i_);
                            ImGui::TextColored(highlightColor, "%s", _sub_value.c_str());
                        }
                    } else
                    {
                        ImGui::TableNextRow();
                        temp_i_ = temp_i_ % v.size();
                        ImGui::TableSetColumnIndex(temp_i_);
                        ImGui::Text("%d", k);
                        for (const auto& _sub_value: v)
                        {
                            ++temp_i_;
                            ImGui::TableSetColumnIndex(temp_i_);
                            ImGui::Text("%s", _sub_value.c_str());
                        }
                    }
                }

                // 输入文本框
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::InputInt("key", &list_key_);
                for (auto j = 0; j < str_vec_size_; ++j)
                {
                    ImGui::TableSetColumnIndex(j + 1);
                    std::string temp = std::to_string(j + 1);
                    ImGui::InputText(temp.c_str(), list_values_[j].data(), list_values_[j].size() * sizeof(char));
                }

                ImGui::EndTable();
            }
        }
        // 控件
        ImGui::Columns(3, nullptr, false); // 创建3列
        if (ImGui::Button("submit"))
        {
            list_change(list_values_, list_key_);
        }
        ImGui::NextColumn();
        if (ImGui::Button("find"))
        {
            auto find_str = list_find(list_key_);
            if (find_str != nullptr)
            {
                std::string temp_str = {};
                for (auto& str: *find_str)
                {
                    temp_str += " " + str;
                }
                ImGui::Text("searching result: %d%s", list_key_, temp_str.c_str());
            }
        }
        ImGui::NextColumn();
        if (ImGui::Button("remove"))
        {
            list_remove(list_key_);
        }
        ImGui::Columns(1); // 恢复单列
        ImGui::End();
    }

    // std::vector<std::string> list_rawdata_train(std::vector<std::array<char, 256> >& input_vec_)
    // {
    //     std::vector<std::string> res{};
    //     for (const auto& i : input_vec_)
    //     {
    //         res.emplace_back(i.data());
    //     }
    //     return res;
    // }

    void list_change(const std::vector<std::array<char, 256> >& raw_string, int key) noexcept
    {
        std::vector<std::string> temp_{};
        temp_.reserve(raw_string.size());
        for (const auto& i: raw_string)
            temp_.emplace_back(i.data());
        list_data_[key] = std::move(temp_);
    }

    void list_remove(int key) noexcept
    {
        if (list_data_.contains(key))
            list_data_.erase(key);
    }

    const std::vector<std::string>* list_find(int key) const noexcept
    {
        if (list_data_.contains(key))
        {
            return &list_data_.at(key);
        }
        return nullptr;
    }

    const std::string list_get_name() const noexcept
    {
        return list_name_;
    }

    const auto list_get_volume() const noexcept
    {
        return list_data_.size();
    }

    const auto list_get_item_size() const noexcept
    {
        if (list_data_.empty())
        {
            return static_cast<size_t>(0);
        }
        return list_data_.begin()->second.size();
    }
};


// 主函数 ================================================================
int main()
{
    // GLFW初始化
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "ImGui Docking & Floating", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ImGui初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // 启用Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // 启用浮动窗口

    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __APPLE__
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 330";
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::map<int, std::vector<std::string> > data{
        {1, {"20", "asd"}},
        {2, {"45", "zxc"}},
        {0, {"12", "qwe"}},
        {21, {"54", "ojk"}}
    };
    std::vector<List> lists{{data, "exemple"}};
    static char name[256] = "";
    static int raw_key = 0;
    static char raw_data[256] = "";

    // 主循环 ----------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // DockSpace ----------------------------------------
        {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("DockSpace Demo", nullptr, window_flags);
            ImGui::PopStyleVar(2);

            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::End();
        }

        // 表格---------------------------------------
        ImGui::Begin("main");
        for (auto& l: lists)
        {
            l.list_show();
        }
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("input a list name", "please name a list", name, IM_ARRAYSIZE(name));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputInt("intput a key", &raw_key);
        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputTextWithHint("input your data", "please input data, seperate each elements by using ;", raw_data,
                                 IM_ARRAYSIZE(raw_data));
        if (ImGui::Button("create") && name[0] != '\0')
        {
            std::string name_temp_ = name;
            lists.emplace_back(name_temp_, raw_key, raw_data);
        }
        // std::cout << lists.size();
        if (ImGui::TreeNode("All Lists")) {
            for (int i = 0; i < lists.size(); ++i) {
                auto& list = lists[i];
                // 显示列表名称
                ImGui::Text("%s", list.list_get_name().c_str());

                ImGui::SameLine();
                if (ImGui::Button(("Delete##" + std::to_string(i)).c_str())) {
                    lists.erase(lists.begin() + i);
                    --i; // 删除后索引回退
                    continue;
                }

                // 可选：展开显示 list_show()
                if (ImGui::TreeNode(("Details##" + std::to_string(i)).c_str())) {
                    ImGui::Text("The volume of this list is : %d", list.list_get_volume());
                    ImGui::Text("The size of each item is : %d", list.list_get_item_size());
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::End();

        // 渲染---------------------------------------
        ImGui::Render();
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // 多视窗支持
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_context);
        }

        glfwSwapBuffers(window);
    }

    // 清理----------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
