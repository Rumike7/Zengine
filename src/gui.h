#include <SDL2/SDL.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include "hittable.h"
#include "scene.h"
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <iomanip>
#include <../external/tinyfiledialogs.h>
#include <filesystem>
namespace fs = std::filesystem;


namespace gui {
    std::vector<std::string> recent_files;
    bool isSaved = true;
    SDL_Window* window = nullptr;

    void updateWindowTitle(const std::string& file_name) {
        if(window == nullptr)return;
        std::string title = (file_name.empty() ? "Untitled" : file_name)+ (isSaved ? "" :"*") + " - ZEngine";
        SDL_SetWindowTitle(window, title.c_str());
    }


    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
        return ss.str();
    }


    void addToRecentFiles(const std::string& file_path) {
        auto it = std::find(recent_files.begin(), recent_files.end(), file_path);
        if (it != recent_files.end()) {
            recent_files.erase(it); // Move to front if already exists
        }
        recent_files.insert(recent_files.begin(), file_path);
        if (recent_files.size() > 5) {
            recent_files.resize(5); 
        }
    }

    void _openFile(scene& sc, bool isNew){
        if(isNew){
            sc = scene(); 
            updateWindowTitle("Untitled");
            std::clog << "Menu action: New file\n";
            return;
        }
        const char* filters[] = { "*.zsc" };
        const char* path = tinyfd_openFileDialog(
            "Load Scene",
            "",
            1,
            filters,
            "ZScene Files (*.zsc)",
            0 // Single file selection
        );
        if (path) {
            fs::path fs_path(path);
            sc.setName(path);
            updateWindowTitle(fs_path.filename().string());
            sc.load_from_file(path);
            addToRecentFiles(path);
            std::clog << "Menu action: Open file: " << path << "\n";
        }
    }

    void _saveFile(scene& sc, bool saveAs){
        if (saveAs && !sc.getName().empty() && fs::exists(sc.getName())) {
            sc.save_to_file(sc.getName().c_str());
            std::clog << "Menu action: Save file: " << sc.getName() << "\n";
            isSaved = true;
            return ;
        } 

        const char* filters[] = { "*.zsc" };
        std::string title = saveAs ? "Save as scene" : "Save scene";
        const char* path = tinyfd_saveFileDialog(
            title.data(),
            "scene.zsc",
            1,
            filters,
            "ZScene Files (*.zsc)"
        );
        if (path) {
            fs::path fs_path(path);
            sc.setName(fs_path);
            updateWindowTitle(fs_path.filename().string());
            sc.save_to_file(path);
            addToRecentFiles(path);
            std::clog << "Menu action: Save file: " << path << "\n";
            isSaved = true;
        }
    }
    
    void HelpMarker(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }


    void render_top_bar(scene& sc, bool& running, bool& use_defocus, float& vfov, float& focus_dist, int& max_depth, int& samples_per_pixel
        , double& pixel_samples_scale, float& topbar_height){
        ImGuiIO& io = ImGui::GetIO(); // Ensure io is up-to-date

        if (ImGui::BeginMainMenuBar()) {
            topbar_height = ImGui::GetFrameHeight();
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    _openFile(sc, true);
                }
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    _openFile(sc, false);
                }

                // Open Recent submenu
                if (ImGui::BeginMenu("Open Recent")) {
                    if (recent_files.empty()) {
                        ImGui::MenuItem("No recent files", nullptr, false, false);
                    } else {
                        for (const auto& file_path : recent_files) {
                            fs::path fs_path(file_path);
                            if (ImGui::MenuItem(fs_path.filename().string().c_str())) {
                                if (fs::exists(file_path)) {
                                    sc.setName(fs_path.filename().string());
                                    updateWindowTitle(fs_path.filename().string());
                                    sc.load_from_file(file_path);
                                    addToRecentFiles(file_path); 
                                    std::clog << "Menu action: Open recent - " << file_path << "\n";
                                } else {
                                    std::clog << "Menu action: Recent file not found - " << file_path << "\n";
                                }
                            }
                        }
                    }
                    if (ImGui::MenuItem("Clear Recent Files")) {
                        recent_files.clear();
                        std::clog << "Menu action: Clear recent files\n";
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    _openFile(sc, false);

                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    _openFile(sc, true);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    running = false;
                    std::clog << "Menu action: Exit application\n";
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Add Object", "Ctrl+U")) {
                    ImGui::OpenPopup("Add or Update Object");
                }
                ImGui::Separator();
                
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                    sc.undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                    sc.redo();
                }
                
                ImGui::EndMenu();
            } 
            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("Grid")) {
                    bool grid_visible = sc.is_grid_shown();
                    if (ImGui::Checkbox("Show Grid", &grid_visible)) {
                        sc.toggle_grid();
                    }
                    static int grid_size = 10;
                    static float grid_spacing = 1.0f;
                    bool grid_changed = false;

                    grid_changed |= ImGui::SliderInt("Grid Size", &grid_size, 5, 50);
                    grid_changed |= ImGui::SliderFloat("Grid Spacing", &grid_spacing, 0.1f, 2.0f, "%.1f");

                    if (grid_changed && ImGui::IsItemDeactivatedAfterEdit()) {
                        sc.set_grid_size(grid_size, grid_spacing);
                    }
                    ImGui::EndMenu(); 
                }
                if (ImGui::BeginMenu("Camera params")) {
                    ImGui::Checkbox("Enable Defocus", &use_defocus);
                    
                    if (ImGui::SliderFloat("Field of View", &vfov, 20.0f, 120.0f, "%.1f deg"));
                    
                    if (ImGui::SliderFloat("Defocus Strength", &focus_dist, 0.0f, 1.0f));
                    
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Render Quality")) {
                    static int current_preset = 0; // Default to Fast
                    
                    if (ImGui::RadioButton("Very Fast (Preview)", &current_preset, 0)) {
                        max_depth = 4;
                        samples_per_pixel = 2;
                    }
                    ImGui::SameLine(); HelpMarker("Low quality, fast rendering");
                    if (ImGui::RadioButton("Fast ", &current_preset, 1)) {
                        max_depth = 8;
                        samples_per_pixel = 4;
                    }
                    ImGui::SameLine(); HelpMarker("Low quality, fast rendering");
                    if (ImGui::RadioButton("Normal", &current_preset, 2)) {
                        max_depth = 10;
                        samples_per_pixel = 10;
                    }
                    ImGui::SameLine(); HelpMarker("Balanced quality and speed");

                    if (ImGui::RadioButton("Good Quality", &current_preset, 3)) {
                        max_depth = 30;
                        samples_per_pixel = 60;            
                    }
                    ImGui::SameLine(); HelpMarker("Best quality, slower rendering");

                    if (ImGui::RadioButton("High Quality", &current_preset, 4)) {
                        max_depth = 50;
                        samples_per_pixel = 100;            
                    }
                    ImGui::SameLine(); HelpMarker("Best quality, slower rendering");

                    if (ImGui::TreeNode("Advanced Settings")) {
                        
                        if (ImGui::SliderInt("Max Ray Depth", &max_depth, 2, 50)) {
                            current_preset = -1; 
                        }
                        
                        if (ImGui::SliderInt("Samples Per Pixel", &samples_per_pixel, 2, 100)) {
                            current_preset = -1;
                        }
                        
                        ImGui::TreePop();
                    }


                    ImGui::EndMenu();
                }



                pixel_samples_scale = 1.0 / samples_per_pixel;

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                // Theme submenu
                if (ImGui::BeginMenu("Theme")) {
                    if (ImGui::MenuItem("Light")) {
                        ImGui::StyleColorsLight();
                        std::clog << "Menu action: Set light theme\n";
                    }
                    if (ImGui::MenuItem("Dark")) {
                        ImGui::StyleColorsDark();
                        std::clog << "Menu action: Set dark theme\n";
                    }
                    if (ImGui::MenuItem("Classic")) {
                        ImGui::StyleColorsClassic();
                        std::clog << "Menu action: Set classic theme\n";
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            
            
            ImGui::EndMainMenuBar();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_O) && io.KeyCtrl) {
            _openFile(sc, false);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_N) && io.KeyCtrl) {
            _openFile(sc, true);
        }


        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
            _saveFile(sc, false);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl && io.KeyShift) {
            _saveFile(sc, true);
        }


        if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl) {
            sc.undo();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl) {
            sc.redo();
        }

    }


    void render_objects_attribute(scene& sc) {
        switch (st.object_type) {
            case ObjectType::Sphere: {
                ImGui::InputFloat("Radius", &st.radius(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::Box: {
                ImGui::InputFloat3("Box Length (x, y, z)", st.box_length());
                break;
            }
            case ObjectType::Cube: {
                ImGui::InputFloat("Cube Size", &st.cube_size(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::Triangle:
            case ObjectType::Rectangle:
            case ObjectType::Disk:
            case ObjectType::Ellipse:
            case ObjectType::Ring: {
                ImGui::InputFloat3("Vector u (x, y, z)", st.u());
                ImGui::InputFloat3("Vector v (x, y, z)", st.v());
                if (st.object_type == ObjectType::Disk) {
                    ImGui::InputFloat("Radius", &st.radius(), 0.01f, 0.1f, "%.2f");
                }
                if (st.object_type == ObjectType::Ring) {
                    ImGui::InputFloat("Inner Radius", &st.inner_radius(), 0.01f, 0.1f, "%.2f");
                    ImGui::InputFloat("Outer Radius", &st.outer_radius(), 0.01f, 0.1f, "%.2f");
                }
                break;
            }
            case ObjectType::Cylinder:
            case ObjectType::Cone:
            case ObjectType::Frustum:
            case ObjectType::Prism: {
                ImGui::InputFloat3("Axis (x, y, z)", st.axis());
                if (st.object_type != ObjectType::Frustum) {
                    ImGui::InputFloat("Radius", &st.radius(), 0.01f, 0.1f, "%.2f");
                } else {
                    ImGui::InputFloat("Top Radius", &st.top_radius(), 0.01f, 0.1f, "%.2f");
                    ImGui::InputFloat("Bottom Radius", &st.bottom_radius(), 0.01f, 0.1f, "%.2f");
                }
                ImGui::InputFloat("Height", &st.height(), 0.01f, 0.1f, "%.2f");
                if (st.object_type == ObjectType::Prism) {
                    float vertices[3][3] = {
                        {0.5f, 0.0f, 0.5f},
                        {-0.5f, 0.0f, 0.5f},
                        {0.0f, 0.0f, -0.5f}
                    };
                    ImGui::InputFloat3("Vertex 1 (x, y, z)", vertices[0]);
                    ImGui::InputFloat3("Vertex 2 (x, y, z)", vertices[1]);
                    ImGui::InputFloat3("Vertex 3 (x, y, z)", vertices[2]);
                    st.set_vertices({
                        st.position + point3(vertices[0][0], vertices[0][1], vertices[0][2]),
                        st.position + point3(vertices[1][0], vertices[1][1], vertices[1][2]),
                        st.position + point3(vertices[2][0], vertices[2][1], vertices[2][2])
                    });
                }
                break;
            }
            case ObjectType::Torus: {
                ImGui::InputFloat("Major Radius", &st.major_radius(), 0.01f, 0.1f, "%.2f");
                ImGui::InputFloat("Minor Radius", &st.minor_radius(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::Plane: {
                ImGui::InputFloat3("Normal (x, y, z)", st.normal());
                break;
            }
            case ObjectType::Ellipsoid: {
                ImGui::InputFloat3("Vector a (x, y, z)", st.a());
                ImGui::InputFloat3("Vector b (x, y, z)", st.b());
                ImGui::InputFloat3("Vector c (x, y, z)", st.c());
                break;
            }
            case ObjectType::Capsule: {
                ImGui::InputFloat3("Second Point (x, y, z)", st.p2());
                ImGui::InputFloat("Radius", &st.radius(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::HollowCylinder: {
                ImGui::InputFloat3("Axis (x, y, z)", st.axis());
                ImGui::InputFloat("Inner Radius", &st.inner_radius(), 0.01f, 0.1f, "%.2f");
                ImGui::InputFloat("Outer Radius", &st.outer_radius(), 0.01f, 0.1f, "%.2f");
                ImGui::InputFloat("Height", &st.height(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::Hexagon: {
                ImGui::InputFloat3("Normal (x, y, z)", st.normal());
                ImGui::InputFloat("Size", &st.size(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::Polyhedron: {
                float vertices[4][3] = {
                    {0.5f, 0.0f, 0.5f},
                    {-0.5f, 0.0f, 0.5f},
                    {0.0f, 0.0f, -0.5f},
                    {0.0f, 1.0f, 0.0f}
                };
                ImGui::InputFloat3("Vertex 1 (x, y, z)", vertices[0]);
                ImGui::InputFloat3("Vertex 2 (x, y, z)", vertices[1]);
                ImGui::InputFloat3("Vertex 3 (x, y, z)", vertices[2]);
                ImGui::InputFloat3("Vertex 4 (x, y, z)", vertices[3]);
                st.set_vertices({
                    st.position + point3(vertices[0][0], vertices[0][1], vertices[0][2]),
                    st.position + point3(vertices[1][0], vertices[1][1], vertices[1][2]),
                    st.position + point3(vertices[2][0], vertices[2][1], vertices[2][2]),
                    st.position + point3(vertices[3][0], vertices[3][1], vertices[3][2])
                });
                break;
            }
            case ObjectType::Wedge: {
                ImGui::InputFloat3("Point 2 (x, y, z)", st.p2());
                ImGui::InputFloat3("Point 3 (x, y, z)", st.p3());
                ImGui::InputFloat("Height", &st.height(), 0.01f, 0.1f, "%.2f");
                break;
            }
            case ObjectType::Tetrahedron: {
                ImGui::InputFloat3("Point 2 (x, y, z)", st.p2());
                ImGui::InputFloat3("Point 3 (x, y, z)", st.p3());
                ImGui::InputFloat3("Point 4 (x, y, z)", st.p4());
                break;
            }
            case ObjectType::Octahedron: {
                ImGui::InputFloat("Size", &st.size(), 0.01f, 0.1f, "%.2f");
                break;
            }
        }
    }







}