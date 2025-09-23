#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>

#include "imgui.h"
#include "imgui_backends/imgui_impl_glfw.h"
#include "imgui_backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

void load_member(int key, char *name, std::unordered_map<int, std::vector<std::string> > &data)
{
    std::stringstream ss(name);
    std::string token;
    std::vector<std::string> result;

    while (std::getline(ss, token, ';'))
    {
        result.emplace_back(token);
    }

    if (!data.empty())
    {
        if (result.size() == data.begin()->second.size())
        {
            data[key] = result;
        }
    } else
    {
        data[key] = result;
    }
}

void show_find(int key, std::unordered_map<int, std::vector<std::string> > &data)
{
    ImGui::Begin("Find");
    if (ImGui::BeginTable("Data", data[key].size() + 1))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", key);
        for (int i = 0; i < data[key].size(); i++)
        {
            ImGui::TableSetColumnIndex(i + 1);
            ImGui::Text("%s", data[key][i].c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void show_list(std::unordered_map<int, std::vector<std::string> > &data)
{
    ImGui::Begin("List");
    if (ImGui::TreeNode("Tree", "Click to expand"))
    {
        if (!data.empty())
        {
            if (ImGui::BeginTable("Data", data.begin()->second.size() + 1))
            {
                for (const auto &[key, value]: data)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", key);
                    for (int i = 0; i < value.size(); i++)
                    {
                        ImGui::TableSetColumnIndex(i + 1);
                        ImGui::Text("%s", value[i].c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

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

    GLFWwindow *window = glfwCreateWindow(800, 600, "ImGui Docking & Floating", NULL, NULL);
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
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // 启用Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // 启用浮动窗口

    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __APPLE__
    const char *glsl_version = "#version 150";
#else
    const char *glsl_version = "#version 330";
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::unordered_map<int, std::vector<std::string> > data;
    bool list_statues = false, find_statues = false;
    int key_pressed = 0;

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
            ImGuiViewport *viewport = ImGui::GetMainViewport();
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

        // 输入
        ImGui::Begin("Input");
        static int key = 0;
        ImGui::InputInt("Key", &key, 1);
        static char name[256] = "";
        ImGui::InputTextWithHint("Contents", "use english; to separate item", name, sizeof(name));
        ImGui::End();

        // 命令
        ImGui::Begin("Command");
        if (ImGui::Button("Submit"))
        {
            load_member(key, name, data);
        }
        if (ImGui::Button("Remove"))
        {
            if (data.erase(key) == 0)
            {
                ImGui::Text("Error! No member with key %d", key);
            }
        }
        if (ImGui::Button("Find"))
        {
            if (data.find(key) != data.end())
            {
                find_statues = true;
                key_pressed = key;
            } else
            {
                find_statues = false;
            }
        }
        if (ImGui::Button("List"))
        {
            list_statues = !list_statues;
        }
        if (find_statues) show_find(key_pressed, data);
        ImGui::End();

        if (list_statues)
        {
            show_list(data);
        }

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
            GLFWwindow *backup_context = glfwGetCurrentContext();
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
