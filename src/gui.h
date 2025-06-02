#include <SDL2/SDL.h>
#include <chrono>
#include <fstream>
#include <iostream>
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
    int selected_object_id = -1;
    int open_menu_id = -1;
    bool is_updating = false;
    bool should_open_modal = false;
    bool should_open_delete = false;



    std::string ExtractFilename(const std::string& path) {
        size_t last_slash = path.find_last_of("/\\");
        return (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    }
    
    void updateWindowTitle(const std::string& file_name) {
        std::string name = ExtractFilename(file_name);
        if(window == nullptr)return;
        std::string title = (file_name.empty() ? "Untitled" : file_name)+ (isSaved ? "" :"*") + " - ZEngine";
        SDL_SetWindowTitle(window, title.c_str());
    }

    void render_objects_attribute(scene& sc, state& st) {
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
    


    void render_add_or_update_modal(scene& sc, state& st){
        if (ImGui::BeginPopupModal("Add or Update Object", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {            
            static float color1[3] = {0.8f, 0.3f, 0.3f}; 
            static float color2[3] = {0.8f, 0.3f, 0.3f};
            static bool initialized = false;
            if(selected_object_id != -1 && !is_updating){
                if (auto ptr = sc.get_state(selected_object_id)) {
                    st = *ptr; 
                }
                is_updating = true;
                color1[0] = st.color_values.x; color1[1] = st.color_values.y; color1[2] = st.color_values.z;
                color2[0] = st.color_values0.x; color2[1] = st.color_values0.y; color2[2] = st.color_values0.z;
                initialized = true;
            }else if(!initialized){
                color1[0] = st.color_values.x; color1[1] = st.color_values.y; color1[2] = st.color_values.z;
                color2[0] = st.color_values0.x; color2[1] = st.color_values0.y; color2[2] = st.color_values0.z;
                initialized = true;
            }

            
            static std::vector<std::string> object_names;
            static std::vector<const char*> object_names_cstr;
            if (object_names.empty()) {
                for (const auto& [type, pair] : object_type_map) {
                    if(type == ObjectType::Count)break;
                    object_names.push_back(pair.first);
                }
                object_names_cstr.resize(object_names.size());
                for (size_t i = 0; i < object_names.size(); ++i) {
                    object_names_cstr[i] = object_names[i].c_str();
                }
            }

            int current_object_type = static_cast<int>(st.object_type);
            if (ImGui::Combo("Type", &current_object_type, object_names_cstr.data(), object_names_cstr.size())) {
                st.object_type = static_cast<ObjectType>(current_object_type);
                st.name = object_type_map[st.object_type].first;
                initialized = false;
            }

            render_objects_attribute(sc, st);

            int current_material_type = static_cast<int>(st.material_type);
            if (ImGui::Combo("Material", &current_material_type, material_names.data(), material_names.size())) {
                st.material_type = static_cast<MaterialType>(current_material_type);
            }

            float pos[3] = { static_cast<float>(st.position.x), static_cast<float>(st.position.y), static_cast<float>(st.position.z) };
            if (ImGui::InputFloat3("Position (x, y, z)", pos)) {
                st.position = point3(pos[0], pos[1], pos[2]);
            }
            
            // float rot[3] = { static_cast<float>(st.position.x), static_cast<float>(st.rotation.y), static_cast<float>(st.rotation.z) };
            // if (ImGui::InputFloat3("ROtation (x, y, z)", rot)) {
            //     st.rotation = point3(rot[0], rot[1], rot[2]);
            // }
            

            if (st.material_type != MaterialType::Dielectric) {
                if(st.material_type == MaterialType::DiffuseLight){
                    st.color_values = color(1.0f, 1.0f, 1.0f);
                }else{
                    st.color_values = color(0.8f, 0.3f, 0.3f);
                }
                int current_texture_type = static_cast<int>(st.texture_type);
                if (ImGui::Combo("Texture", &current_texture_type, texture_types.data(), texture_types.size())) {
                    st.texture_type = static_cast<TextureType>(current_texture_type);
                }
    
                float color_values[3] = { static_cast<float>(st.color_values.x), static_cast<float>(st.color_values.y), static_cast<float>(st.color_values.z) };
                float color_values0[3] = { static_cast<float>(st.color_values0.x), static_cast<float>(st.color_values0.y), static_cast<float>(st.color_values0.z) };
                if (st.texture_type == TextureType::Checker) { 
                    bool color1_changed = ImGui::ColorEdit3("Checker Color 1", color1, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
                    if (color1_changed) {
                        st.color_values = color(color1[0], color1[1], color1[2]);
                    }

                    bool color2_changed = ImGui::ColorEdit3("Checker Color 2", color2, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
                    if (color2_changed) {
                        st.color_values0 = color(color2[0], color2[1], color2[2]);
                    }
                } 
                else if (st.texture_type == TextureType::Image) {
                    char texture_file_buffer[256];
                    strncpy(texture_file_buffer, ExtractFilename(st.texture_file).c_str(), sizeof(texture_file_buffer) - 1);
                    texture_file_buffer[sizeof(texture_file_buffer) - 1] = '\0';

                    if (ImGui::InputText("Image File", texture_file_buffer, sizeof(texture_file_buffer))) {
                        if (texture_file_buffer[0] != '\0') {
                            size_t last_slash = st.texture_file.find_last_of("/\\");
                            if (last_slash != std::string::npos) {
                                st.texture_file = st.texture_file.substr(0, last_slash + 1) + texture_file_buffer;
                            } else {
                                st.texture_file = texture_file_buffer;
                            }
                        } else {
                            st.texture_file.clear();
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Browse")) {
                        const char* filters[] = { "*.jpg", "*.png", "*.bmp", "*.tga" }; // Supported image formats
                        const char* path = tinyfd_openFileDialog(
                            "Select Image",
                            "", // Default path
                            4,                       // Number of filters
                            filters,                 // Filter patterns
                            "Image Files (*.jpg, *.png, *.bmp, *.tga)", // Filter description
                            0                        // Single file selection
                        );
                        if (path) {
                            st.texture_file = path;
                            std::clog << "Selected image: " << path << "\n";
                        } else {
                            st.texture_file = "../assets/earthmap.jpg";
                            std::clog << "No file selected, using default: ../assets/earthmap.jpg\n";
                        }
                    }
                }
                else if (st.texture_type == TextureType::Noise) {
                    ImGui::SliderFloat("Noise Scale", &st.noise_scale, 0.1f, 10.0f);
                }else{ 
                    bool color1_changed = ImGui::ColorEdit3("Color", color1, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
                    if (color1_changed) {
                        st.color_values = color(color1[0], color1[1], color1[2]);
                    }
                }
            }else{
                ImGui::InputDouble("Refraction Index", &st.refraction_index, 0.01, 0.1, "%.3f");
                
                ImGui::SameLine();
                if (ImGui::Button("Presets")) {
                    ImGui::OpenPopup("RefractionPresets");
                }
                
                if (ImGui::BeginPopup("RefractionPresets")) {
                    ImGui::Text("Common Materials:");
                    if (ImGui::Selectable("Air (1.000)")) st.refraction_index = 1.000;
                    if (ImGui::Selectable("Water (1.333)")) st.refraction_index = 1.333;
                    if (ImGui::Selectable("Glass (1.500)")) st.refraction_index = 1.500;
                    if (ImGui::Selectable("Diamond (2.417)")) st.refraction_index = 2.417;
                    ImGui::EndPopup();
                }
                
                ImGui::SameLine();
                ImGui::
                TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Refraction index determines how light bends through the material.");
                    ImGui::Text("Common values: Air (1.000), Water (1.333), Glass (1.500), Diamond (2.417)");
                    ImGui::EndTooltip();
                }
            }

            if(st.material_type == MaterialType::Metal){
                ImGui::SliderFloat("Fuzz ", &st.fuzz, 0.01f, 1.0f, "%.2f");
            }

            char name_buf[128]; 
            strncpy(name_buf, st.name.c_str(), sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0'; 
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                st.name = name_buf;
            }

            
            if (ImGui::Button(selected_object_id == -1 ?"Add" : "Update")) {
                std::clog << st.color_values << "\n";
                st.color_values = color(color1[0], color1[1], color1[2]);
                st.color_values0 = color(color2[0], color2[1], color2[2]);

                sc.add_or_update_object(st, selected_object_id);

                selected_object_id = -1;
                is_updating = false;
                initialized = false;
                st.reset();
                ImGui::CloseCurrentPopup();
                isSaved = false;
                updateWindowTitle(sc.getName());
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                selected_object_id = -1;
                is_updating = false;
                initialized = false; 
                ImGui::CloseCurrentPopup();
                st.reset();
            }

            ImGui::EndPopup();
        }

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
            recent_files.erase(it); 
        }
        recent_files.insert(recent_files.begin(), file_path);
        if (recent_files.size() > 5) {
            recent_files.resize(5); 
        }
    }

    void _openFile(scene& sc, bool isNew){

        if(isNew){
            sc = scene(); 
            std::clog << "Menu action: New file\n";
            isSaved = true;
            updateWindowTitle("Untitled");
            return;
        }
        const char* filters[] = { "*.zsc" };
        const char* path = tinyfd_openFileDialog(
            "Load Scene",
            "",
            1,
            filters,
            "ZScene Files (*.zsc)",
            0 
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
        std::clog << "Menu action:" << sc.getName() << "\n";
        if (saveAs && !sc.getName().empty() && fs::exists(sc.getName())) {
            sc.save_to_file(sc.getName().c_str());
            std::clog << "Menu action: Save file: " << sc.getName() << "\n";
            isSaved = true;
            updateWindowTitle(sc.getName());
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
            sc.setName(path);
            sc.save_to_file(path);
            addToRecentFiles(path);
            std::clog << "Menu action: Save file: " << path << "\n";
            isSaved = true;
            updateWindowTitle(sc.getName());
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
        ImGuiIO& io = ImGui::GetIO(); 

        if (ImGui::BeginMainMenuBar()) {
            topbar_height = ImGui::GetFrameHeight();
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    _openFile(sc, true);
                }
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    _openFile(sc, false);
                }

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
                    _saveFile(sc, false);

                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    _saveFile(sc, true);
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
                    should_open_modal = true;
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
                    static int current_preset = 2; // Default to Normal
                    
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


    void render_object_menu(scene& sc,  int id) {
        ImGuiIO io = ImGui::GetIO();
        shared_ptr<hittable> object = sc.get_object(id);
        if(object == nullptr) {
            std::clog << "Object with ID " << id << " not found.\n";
            return;
        }
        try {

            ImGui::PushID(id); 
            std::string display_label = object->get_icon() + " " + object->get_name();
            bool item_clicked = ImGui::Selectable(display_label.c_str(), false);

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Update", "Ctrl+U", false, true)) {
                    selected_object_id = id;
                    should_open_modal = true;      
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Update object properties");
                }

                if (ImGui::MenuItem("Delete", "Ctrl+D", false, true)) {
                    should_open_delete = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Permanently delete object");
                }

                if (ImGui::MenuItem("Duplicate", "Ctrl+T", false, true)) {
                    sc.duplicate_object(id);
                    isSaved = false;    
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Create a copy of the object");
                }
                open_menu_id = -1;
                ImGui::EndMenu();
            }
            ImGui::PopID();  

            if(should_open_delete) {
                ImGui::OpenPopup("Confirm the deletion");
                should_open_delete = false; 
            }

            if (ImGui::BeginPopupModal("Confirm the deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to delete '%s'?", object->get_name().c_str());
                ImGui::Separator();
                
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    
                    sc.delete_object(id);
                    isSaved = false;    
                    updateWindowTitle(sc.getName());
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }


        } catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error rendering menu");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_U) && io.KeyCtrl) {
            should_open_modal = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl) {
            ImGui::OpenPopup("Confirm Delete");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T) && io.KeyCtrl) {
            int new_id = sc.duplicate_object(id);
            isSaved = false;   
            updateWindowTitle(sc.getName()); 

        }
    }   
    

    void render_object_buttons(scene& sc, int render_width, int topbar_height, int gui_width, int window_height, 
                        state& st, vec3& lookfrom, float yaw, float pitch, int render_height, int controls_height) {
        ImGui::SetNextWindowPos(ImVec2(float(render_width), topbar_height));
        ImGui::SetNextWindowSize(ImVec2(float(gui_width), float(window_height)-topbar_height));
        ImGui::Begin("Scene Objects", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4)); // Smaller padding
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));  // Spacing between buttons

        float button_size = 32.0f; 
        float padding = ImGui::GetStyle().ItemSpacing.x;
        float available_width = ImGui::GetContentRegionAvail().x;

        int columns = std::max(1, int((available_width + padding) / (button_size + padding)));


        if (ImGui::BeginTable("ObjectGrid", columns, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX)) {
            struct ObjectButton {
                std::string icon;   
                std::string tooltip;
                ObjectType type;
            };

            std::vector<ObjectButton> buttons;
            for (const auto& [type, pair] : object_type_map) {
                if(type == ObjectType::Count)break;
                std::string tooltip = std::string("Add ") + pair.first;
                buttons.push_back({pair.second, tooltip, type});
            }

            for (const auto& button : buttons) {
                ImGui::TableNextColumn();
                ImGui::PushID(&button);

                if (ImGui::Button(button.icon.c_str(), ImVec2(button_size, button_size))) {
                    st.reset();
                    st.object_type = button.type;
                    st.name = object_type_map.at(st.object_type).first;
                    should_open_modal = true;
                    std::clog << "Button clicked\n";
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", button.tooltip.c_str()); 
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }
            
            ImGui::EndTable();
        }

        ImGui::PopStyleVar(2);

        
        ImGui::Separator();

        if (ImGui::Button("\ue145 Add Object", ImVec2(160, 40))) {
            should_open_modal = true;
        }


        
        ImGui::Separator();


        ImGui::Text("Objects in Scene:");
        ImGui::Text("");
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        auto objects = sc.get_objects();
        for(auto [id, object] : objects){
            render_object_menu(sc, id);
        }
        if(open_menu_id != -1) {
            ImGui::OpenPopup("ObjectContextMenu");
        }
        if (ImGui::BeginPopup("ObjectContextMenu")) {
            shared_ptr<hittable> object = sc.get_object(open_menu_id);
            if (ImGui::MenuItem("Update", "Ctrl+U", false, true)) {
                selected_object_id = open_menu_id;
                should_open_modal = true;  
                open_menu_id = -1;    
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Update object properties");
            }

            if (ImGui::MenuItem("Delete", "Ctrl+D", false, true)) {
                should_open_delete = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Permanently delete object");
            }

            if (ImGui::MenuItem("Duplicate", "Ctrl+T", false, true)) {
                sc.duplicate_object(open_menu_id);
                isSaved = false;    
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Create a copy of the object");
            }
            
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();


        ImGui::Separator();
        
        ImGui::End();


        ImGui::SetNextWindowPos(ImVec2(0.0f, topbar_height+render_height));
        ImGui::SetNextWindowSize(ImVec2(render_width, window_height-topbar_height-render_height));
        ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", lookfrom.x, lookfrom.y, lookfrom.z);
        ImGui::Text("Yaw: %.2f, Pitch: %.2f", yaw, pitch);
        if(sc.is_grid_shown())ImGui::Text("The X-axis is RED, The Z-axis is BLUE, The Y-axis is the third ones");
        ImGui::Separator();
        
        ImGui::TextWrapped("Hold RIGHT mouse button and move to rotate camera");
        ImGui::TextWrapped("WASD or arrow keys to move camera");
        ImGui::TextWrapped("Hold LEFT mouse button to select an object and move it");
        ImGui::TextWrapped("Use crtl + P to save the image in ppm format");

        ImGui::Separator();

        ImGui::Text("Number of Objects: %d", sc.get_objects().size());        
        ImGui::TextWrapped("To update, delete or duplicate an object, put the cursor on the object or on it name and click on RIGHT mouse button");

        ImGui::Separator();

        ImGui::End();  


    }


    void renderSaveConfirmationPopup(scene& sc) {
        if (ImGui::BeginPopupModal("Save Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes.");
            ImGui::Text("Do you want to save before creating a new file?");
            ImGui::Separator();
            
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                _saveFile(sc, true);
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            
            if (ImGui::Button("Don't Save", ImVec2(120, 0))) {

                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }


         


}