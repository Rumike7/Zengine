#ifndef CAMERA_H
#define CAMERA_H

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
#include <../external/nfd.h>
#include <chrono>
#include <iomanip>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>


class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};


class camera {
public:
    double aspect_ratio = 16.0 / 9.0;
    int render_width = 800;
    int window_width = 1280;
    int gui_width = 300;
    int samples_per_pixel = 4;
    int max_depth = 10;
    color background = color(0.5, 0.7, 1.0);
    double move_speed = 0.1;
    double mouse_sensitivity = 0.005;
    double vfov = 90;
    point3 lookfrom = point3(3, 1, 0);
    point3 lookat = point3(0, 0, 0);
    vec3 vup = vec3(0, 1, 0);
    double yaw = 0.0, pitch = 0.0;
    double defocus_angle = 0;
    double focus_dist = 1.0;

    camera() : thread_pool(std::thread::hardware_concurrency()){
        if (!initialize()) {
            std::cerr << "Camera initialization failed\n";
            throw std::runtime_error("SDL initialization failed");
        }
    }

    ~camera() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool valid(int x, int y) {
        return x >= 0 && y >= 0 && x < render_width && y < render_height;
    }

    void handle_poll_event(SDL_Event& event, scene& sc) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))) {
                running = false;
                std::clog << "Quit event\n";
            }
            // Only process mouse and keyboard events if ImGui isn't capturing them
            if (!io.WantCaptureMouse) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT && valid(event.button.x, event.button.y)) {
                        int i = event.button.x;
                        int j = event.button.y;
                        auto r = get_ray(i, j);
                        if (sc.select_object(r, 1000.0)) {
                            object_grabbed = true;
                            std::clog << "Object selected\n";
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        mouse_grabbed = true;
                        std::clog << "Camera grabbed\n";
                    }
                }

                if (event.type == SDL_MOUSEBUTTONUP) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (object_grabbed) std::clog << "Object released\n";
                        object_grabbed = false;
                        sc.move_selected(point3(0,0,0), 2);
                    }
                    if (event.button.button == SDL_BUTTON_RIGHT) {
                        if (mouse_grabbed) std::clog << "Camera released\n";
                        mouse_grabbed = false;
                    }
                }

                if (event.type == SDL_MOUSEMOTION) {
                    if (mouse_grabbed && valid(event.motion.x, event.motion.y)) {
                        yaw -= event.motion.xrel * mouse_sensitivity;
                        pitch -= event.motion.yrel * mouse_sensitivity;
                        if (pitch > 1.57) pitch = 1.57;
                        if (pitch < -1.57) pitch = -1.57;
                    }

                    if (object_grabbed && valid(event.motion.x, event.motion.y)) {
                        int i = event.motion.x;
                        int j = event.motion.y;
                        ray r = get_ray(i, j, true);
                        // double t = -r.origin().y() / r.direction().y();
                        // if (t > 0) {
                        //     point3 new_pos = r.at(t);
                        //     new_pos = point3(new_pos.x(), 0, new_pos.z());
                        //     sc.move_selected(new_pos);
                        // }
                        vec3 camera_forward = unit_vector(vec3(
                            cos(yaw) * cos(pitch),
                            sin(pitch),
                            sin(yaw) * cos(pitch)
                        ));
                        
                        point3 selected_pos = sc.get_selected_position();
                        
                        double plane_distance = dot(selected_pos - lookfrom, camera_forward);
                        
                        double t = plane_distance / dot(r.direction(), camera_forward);
                        
                        if (t > 0) {
                            point3 new_pos = r.at(t);
                            sc.move_selected(new_pos, 1);
                        }
                    }
                }

            }

        }
        if (!io.WantCaptureKeyboard) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
                std::clog << "Escape key pressed\n";
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p) {
                save_ppm(sc);
                std::clog << "PPM saved\n";
            }
            const Uint8* keys = SDL_GetKeyboardState(NULL);
            vec3 forward(cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw));
            vec3 right(-sin(yaw), 0, cos(yaw));
            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) lookfrom = lookfrom + forward * move_speed;
            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) lookfrom = lookfrom - forward * move_speed;
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) lookfrom = lookfrom - right * move_speed;
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) lookfrom = lookfrom + right * move_speed;

        }
    }

    void RenderTopBar(scene& sc){
        ImGuiIO& io = ImGui::GetIO(); // Ensure io is up-to-date

        if (ImGui::BeginMainMenuBar()) {
            topbar_height = ImGui::GetFrameHeight();
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    // Handle New file action
                    std::clog << "Menu action: New file\n";
                }
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    sc.load_from_file("../output/file.zsc");
                    // Handle Open file action
                    std::clog << "Menu action: Open file\n";
                }
                
                // Open Recent submenu
                if (ImGui::BeginMenu("Open Recent")) {
                    // You can populate this with actual recent files
                    if (ImGui::MenuItem("file.zsc")) {
                        sc.load_from_file("../output/file.zsc");
                        std::clog << "Menu action: Open recent - example1.scene\n";
                    }
                    if (ImGui::MenuItem("Clear Recent Files")) {
                        std::clog << "Menu action: Clear recent files\n";
                    }
                    ImGui::EndMenu();
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    sc.save_to_file("../output/file.zsc");
                    std::clog << "Menu action: Save file\n";
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    sc.save_to_file("../output/file.zsc");
                    std::clog << "Menu action: Save file as\n";
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    // Handle exit
                    std::clog << "Menu action: Exit application\n";
                    // You might want to set a flag here to close the application
                    // e.g., window_should_close = true;
                }
                
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {

                if (ImGui::MenuItem("Add Object", "Ctrl+U")) {
                    ImGui::OpenPopup("AddObjectModal");
                }
                ImGui::Separator();
                
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                    sc.undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                    sc.redo();
                }
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

                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                    // sc.cut();
                }
                if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                    // sc.copy();
                }
                if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                    // sc.paste();
                }


                //TODO : cut,copy and paste                
                
                ImGui::EndMenu();
            }            
            if (ImGui::BeginMenu("Settings")) {
                // Theme submenu
                if (ImGui::BeginMenu("Theme")) {
                    if (ImGui::MenuItem("Light")) {
                        // Set light theme
                        ImGui::StyleColorsLight();
                        std::clog << "Menu action: Set light theme\n";
                    }
                    if (ImGui::MenuItem("Dark")) {
                        // Set dark theme
                        ImGui::StyleColorsDark();
                        std::clog << "Menu action: Set dark theme\n";
                    }
                    if (ImGui::MenuItem("Classic")) {
                        // Set classic theme
                        ImGui::StyleColorsClassic();
                        std::clog << "Menu action: Set classic theme\n";
                    }
                    ImGui::EndMenu();
                }
            
                
            }
            
            // You can add more menu items here (Edit, View, Help, etc.)
            
            ImGui::EndMainMenuBar();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_O) && io.KeyCtrl) {
            sc.load_from_file("../output/file.zsc");
        }

        if (ImGui::IsKeyPressed(ImGuiKey_N) && io.KeyCtrl) {
            sc.load_new();
        }


        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
            sc.save_to_file("../output/file.zsc");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl && io.KeyShift) {
            sc.save_to_file("../output/file.zsc");
        }


        if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl) {
            sc.undo();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl) {
            sc.redo();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_X) && io.KeyCtrl) {
            // sc.cut();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl) {
            // sc.copy();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl) {
            // sc.paste();
        }



    }

    // Helper function to get formatted timestamp
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
        return ss.str();
    }

    void render_object_menu(scene& sc, shared_ptr<hittable> object, int id) {
        ImGuiIO io = ImGui::GetIO();
        bool can_update = true;
        bool can_delete = true;
        bool can_duplicate = true;
        try {
            bool confirmDeleteModal = false;
            bool addObjectModal = false;

            ImGui::PushID(id); 
            std::string display_label = object->get_icon() + " " + object->get_name();
            bool item_clicked = ImGui::Selectable(display_label.c_str(), false);


            // Store current selection state
            bool is_selected = sc.get_selected_object_id() == static_cast<int>(id);
            // Context menu with keyboard shortcuts
            if (ImGui::BeginPopupContextItem()) {
                // Update action
                // bool can_update = object->is_updatable();
                if (ImGui::MenuItem("Update", "Ctrl+U", false, can_update)) {
                    selected_object_id = id;
                    addObjectModal = true;                        
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Update object properties");
                }

                // Delete action with confirmation
                // bool can_delete = object->is_deletable();
                if (ImGui::MenuItem("Delete", "Ctrl+D", false, can_delete)) {
                    confirmDeleteModal = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Permanently delete object");
                }
        

                // Duplicate action
                // bool can_duplicate = sc.can_duplicate_object(id);
                if (ImGui::MenuItem("Duplicate", "Ctrl+T", false, can_duplicate)) {
                    try {
                        int new_id = sc.duplicate_object(id);
                        std::clog << "[" << get_timestamp() << "] Duplicated object " 
                                << object->get_name() << " (New ID: " << new_id << ")\n";
                    } catch (const std::exception& e) {
                        std::clog << "[" << get_timestamp() << "] Error duplicating object " 
                                << object->get_name() << ": " << e.what() << "\n";
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Create a copy of the object");
                }

                ImGui::EndMenu();
            }

            if(confirmDeleteModal){
                ImGui::OpenPopup("ConfirmDeleteModal");
            }
            if(addObjectModal){
                ImGui::OpenPopup("AddObjectModal");
            }

            // Delete confirmation dialog
            if (ImGui::BeginPopupModal("ConfirmDeleteModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to delete '%s'?", object->get_name().c_str());
                ImGui::Separator();
                
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    sc.delete_object(id);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            render_add_or_update_modal(sc);


            // Keyboard shortcuts
            
            ImGui::PopID();  
        } catch (const std::exception& e) {
            std::clog << "[" << get_timestamp() << "] Error in object menu: " << e.what() << "\n";
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error rendering menu");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_U) && io.KeyCtrl && can_update) {
            ImGui::OpenPopup("AddObjectModal");

        }
        if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl && can_delete) {
            ImGui::OpenPopup("Confirm Delete");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T) && io.KeyCtrl && can_duplicate) {
            int new_id = sc.duplicate_object(id);
            std::clog << "[" << get_timestamp() << "] Duplicated object via shortcut " 
                    << object->get_name() << " (New ID: " << new_id << ")\n";
        }
    }   
    
    void render_add_or_update_modal(scene& sc){
        if (ImGui::BeginPopupModal("AddObjectModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {            
            static float color1[3] = {0.8f, 0.3f, 0.3f}; // Initialize with defaults
            static float color2[3] = {0.8f, 0.3f, 0.3f};
            static bool initialized = false;
            if(selected_object_id != -1 && !is_updating){
                st = sc.get_state(selected_object_id);
                is_updating = true;
                color1[0] = st.color_values.x(); color1[1] = st.color_values.y(); color1[2] = st.color_values.z();
                color2[0] = st.color_values0.x(); color2[1] = st.color_values0.y(); color2[2] = st.color_values0.z();
                initialized = true;
            }else if(!initialized){
                color1[0] = st.color_values.x(); color1[1] = st.color_values.y(); color1[2] = st.color_values.z();
                color2[0] = st.color_values0.x(); color2[1] = st.color_values0.y(); color2[2] = st.color_values0.z();
                initialized = true;
            }
            
            int current_object_type = static_cast<int>(st.object_type);
            if (ImGui::Combo("Type", &current_object_type, st.object_types.data(), st.object_types.size())) {
                st.object_type = static_cast<ObjectType>(current_object_type);
            }

            // Material type dropdown
            int current_material_type = static_cast<int>(st.material_type);
            if (ImGui::Combo("Material", &current_material_type, st.material_names.data(), st.material_names.size())) {
                st.material_type = static_cast<MaterialType>(current_material_type);
                std::clog << current_material_type << "\n";
            }

            float pos[3] = { static_cast<float>(st.position.x()), static_cast<float>(st.position.y()), static_cast<float>(st.position.z()) };
            if (ImGui::InputFloat3("Position (x, y, z)", pos)) {
                st.position = point3(pos[0], pos[1], pos[2]);
            }
            
            if (st.material_type != MaterialType::Dielectric) {
                if(st.material_type == MaterialType::DiffuseLight){
                    st.color_values = color(1.0f, 1.0f, 1.0f);
                }else{
                    st.color_values = color(0.8f, 0.3f, 0.3f);
                }
                int current_texture_type = static_cast<int>(st.texture_type);
                if (ImGui::Combo("Texture", &current_texture_type, st.texture_types.data(), st.texture_types.size())) {
                    st.texture_type = static_cast<TextureType>(current_texture_type);
                }
    
                float color_values[3] = { static_cast<float>(st.color_values.x()), static_cast<float>(st.color_values.y()), static_cast<float>(st.color_values.z()) };
                float color_values0[3] = { static_cast<float>(st.color_values0.x()), static_cast<float>(st.color_values0.y()), static_cast<float>(st.color_values0.z()) };
                if (st.texture_type == TextureType::Checker) { 
                    bool color1_changed = ImGui::ColorEdit3("Checker Color 1", color1, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
                    if (color1_changed) {
                        st.color_values = color(color1[0], color1[1], color1[2]);
                    }

                        //TODO : FIle proper browser (option native file dialog)  
                    // Color picker for Checker Color 2
                    bool color2_changed = ImGui::ColorEdit3("Checker Color 2", color2, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float);
                    if (color2_changed) {
                        st.color_values0 = color(color2[0], color2[1], color2[2]);
                    }

                } 
                else if (st.texture_type == TextureType::Image) {
                    ImGui::InputText("Image Path", st.texture_file.data(), st.texture_file.size());
                    ImGui::SameLine();
                    if (ImGui::Button("Browse")) {
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

            
            // Add button
            if (ImGui::Button(selected_object_id == -1 ?"Add" : "Update")) {
                std::clog << st.color_values << "\n";
                st.color_values = color(color1[0], color1[1], color1[2]);
                st.color_values0 = color(color2[0], color2[1], color2[2]);

                sc.add_or_update_object(st, selected_object_id);

                selected_object_id = -1;
                is_updating = false;
                initialized = false; // Allow reinitialization next time modal opens
                st.reset();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                selected_object_id = -1;
                is_updating = false;
                initialized = false; // Allow reinitialization next time modal opens
                ImGui::CloseCurrentPopup();
                st.reset();
            }

            ImGui::EndPopup();
        }

    } 

    void RenderObjectButtons(scene& sc) {
        // Camera controls instructions
        ImGui::SetNextWindowPos(ImVec2(float(render_width), topbar_height));
        ImGui::SetNextWindowSize(ImVec2(float(gui_width), float(window_height)-topbar_height));
        ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        
        // Add camera debug info to UI
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", lookfrom.x(), lookfrom.y(), lookfrom.z());
        ImGui::Text("Yaw: %.2f, Pitch: %.2f", yaw, pitch);
        ImGui::Separator();
        
        // Camera controls instructions
        ImGui::TextWrapped("Hold RIGHT mouse button and move to rotate camera");
        ImGui::TextWrapped("WASD or arrow keys to move camera");
        ImGui::TextWrapped("Hold LEFT mouse button to select an object and move it");

        ImGui::Separator();

        // Style adjustments for compact buttons
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4)); // Smaller padding
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));  // Spacing between buttons

        float button_size = 32.0f; 
        float padding = ImGui::GetStyle().ItemSpacing.x;
        float available_width = ImGui::GetContentRegionAvail().x;

        // Compute max number of columns that fit
        int columns = std::max(1, int((available_width + padding) / (button_size + padding)));

        // Create a 4-column table for icon buttons
        if (ImGui::BeginTable("ObjectGrid", columns, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX)) {
            // Define icons, tooltips, and functions
            struct ObjectButton {
                const char* icon;
                const char* tooltip;
                void (*add_function)(scene&);
            };
            ObjectButton buttons[] = {
                {"\uf111", "Add Sphere", [](scene& w) { 1+1; }},
                {"\uf0c8", "Add Rectangle", [](scene& w) { 1+1; }},
                {"\uf466", "Add Box", [](scene& w) { 1+1; }},
                {"\uf0de", "Add Triangle", [](scene& w) { 1+1; }},
                {"\uf192", "Add Disk", [](scene& w) {1+1; }},
                {"\uf192", "Add Ellipse", [](scene& w) { 1+1; }},
                {"\uf0a3", "Add Ring", [](scene& w) { 1+1; }}
            };

            // Iterate through buttons and place in grid
            for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
                ImGui::TableNextColumn();

                // Create a button with the icon
                char button_id[32];
                snprintf(button_id, sizeof(button_id), "%s##%d", buttons[i].icon, i); // Unique ID
                if (ImGui::Button(button_id, ImVec2(button_size, button_size))) {
                    ImGui::OpenPopup("AddObjectPopup");
                }

                // Add tooltip on hover
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                   ImGui::Text("%s", buttons[i].tooltip);
                    ImGui::EndTooltip();
                }
            }
            

            ImGui::EndTable();
        }

        // Restore style
        ImGui::PopStyleVar(2);

        ImGui::Separator();

        if (ImGui::Button("\uf067 Add Object", ImVec2(160, 40))) {
            ImGui::OpenPopup("AddObjectModal");
        }

        render_add_or_update_modal(sc);
        
        ImGui::Separator();


        ImGui::Text("Objects in Scene:");
        ImGui::Text("");
        
        // Add slight padding between items
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        auto objects = sc.get_objects();
        for(auto [id, object] : objects){
            render_object_menu(sc, object, id);

        }
        ImGui::PopStyleVar();

        ImGui::Separator();

        
        ImGui::End();
    }

    bool render(scene& sc) {
        SDL_GL_MakeCurrent(window, gl_context);
        std::vector<color> pixel_buffer(render_width * render_height, color(0, 0, 0));
        std::vector<Uint32> pixel_data(render_width * render_height);
        SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
        if (!format) {
            std::cerr << "Failed to allocate pixel format: " << SDL_GetError() << "\n";
            return false;
        }

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
            "Camera Controls",
            "• Hold RIGHT MOUSE BUTTON and move to rotate camera\n"
            "• Use W/A/S/D or ARROW KEYS to move camera\n"
            "• LEFT MOUSE BUTTON to select objects\n"
            "• Press ESC to exit\n"
            "• Press P to save the scene as PPM",
            window);

        auto last_time = std::chrono::high_resolution_clock::now();
        const double target_frame_time = 1.0 / 60.0;
        std::atomic<int> completed_rows{0};
        const int rows_per_task = 10; // Adjust for optimal task size

        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            double delta_time = std::chrono::duration<double>(now - last_time).count();
            last_time = now;
            // std::clog << "Frame time: " << delta_time * 1000 << " ms\n";

            SDL_Event event;
            handle_poll_event(event, sc);
            update_camera();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            RenderTopBar(sc);
            RenderObjectButtons(sc);

            completed_rows = 0;
            std::fill(pixel_buffer.begin(), pixel_buffer.end(), color(0, 0, 0));
            for (int start_row = 0; start_row < render_height; start_row += rows_per_task) {
                int end_row = std::min(start_row + rows_per_task, render_height);
                thread_pool.enqueue([this, &sc, &pixel_buffer, start_row, end_row, &completed_rows]() {
                    for (int j = start_row; j < end_row; ++j) {
                        for (int i = 0; i < render_width; ++i) {
                            int idx = j * render_width + i;
                            for (int _ = 0; _ < samples_per_pixel; ++_) {
                                ray r = get_ray(i, j);
                                pixel_buffer[idx] += ray_color(r, max_depth, sc);
                            }
                        }
                        completed_rows++;
                    }
                });
            }

            // Wait for rendering tasks to complete
            while (completed_rows < render_height) {
                SDL_Delay(1); // Avoid busy-waiting
            }

            // Update pixel data
            for (int j = 0; j < render_height; ++j) {
                for (int i = 0; i < render_width; ++i) {
                    int idx = j * render_width + i;
                    color c = pixel_buffer[idx] * pixel_samples_scale;
                    c = color(sqrt(c.x()), sqrt(c.y()), sqrt(c.z())); // Gamma correction
                    pixel_data[idx] = SDL_MapRGBA(
                        format,
                        Uint8(clamp(c.x(), 0.0, 1.0) * 255.99),
                        Uint8(clamp(c.y(), 0.0, 1.0) * 255.99),
                        Uint8(clamp(c.z(), 0.0, 1.0) * 255.99),
                        255
                    );
                }
            }

            // Update OpenGL texture
            glBindTexture(GL_TEXTURE_2D, render_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data.data());
            glBindTexture(GL_TEXTURE_2D, 0);

            // Render image
            ImGui::SetNextWindowPos(ImVec2(0, topbar_height));
            ImGui::SetNextWindowSize(ImVec2(float(render_width), float(render_height) - topbar_height));
            ImGui::Begin("Render", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoCollapse);
            ImGui::Image((ImTextureID)(uintptr_t)render_texture, ImVec2(float(render_width), float(render_height)));
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);

            double frame_time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - now).count();
            if (frame_time < target_frame_time) {
                SDL_Delay(Uint32((target_frame_time - frame_time) * 1000));
            }
        }

        SDL_FreeFormat(format);
        return true;
    }


private:
    int window_height;
    int render_height;
    float topbar_height;
    point3 pixel00_loc;
    vec3 pixel_delta_u;
    vec3 pixel_delta_v;
    double pixel_samples_scale;
    vec3 u, v, w;
    vec3 defocus_disk_u;
    vec3 defocus_disk_v;
    double viewport_width;
    double viewport_height;
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context;
    GLuint render_texture = 0;
    int selected_object_id = -1; 
    bool is_updating = false;
    ImGuiIO io;
    bool running = true;
    bool mouse_grabbed = false;
    bool object_grabbed = false;
    ThreadPool thread_pool;
    state st;


    bool initialize() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            SDL_Log("Error: %s\n", SDL_GetError());
            return false;
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        render_height = int(render_width / aspect_ratio);
        render_height = (render_height < 1) ? 1 : render_height;
        window_height = int(window_width / aspect_ratio);
        window_height = (window_height < 1) ? 1 : window_height;

        window = SDL_CreateWindow("ZEngine",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1);

        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            SDL_Log("Failed to initialize GLAD");
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        io.Fonts->AddFontDefault();
        // Load FontAwesome
        static const ImWchar icons_ranges[] = { 0xe800, 0xf8ff, 0 }; // FontAwesome range
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("../assets/fa-solid-900.ttf", 16.0f, &icons_config, icons_ranges);
        io.Fonts->Build();

        glGenTextures(1, &render_texture);
        glBindTexture(GL_TEXTURE_2D, render_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render_width, render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);



        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 330");

        pixel_samples_scale = 1.0 / samples_per_pixel;
        vec3 look_dir = -w;

        pitch = asin(look_dir.y()); 

        float cos_pitch = sqrt(1.0 - look_dir.y() * look_dir.y()); 
        if (abs(cos_pitch) > 1e-6) { 
            yaw = atan2(look_dir.z(), look_dir.x());//arctan(y/x)
        } else {
            yaw = 0.0; 
        }
        update_camera();
        SDL_SetRelativeMouseMode(SDL_FALSE);        
        std::clog << "Camera initialized\n";
        return true;
    }

    void update_camera() {
        vec3 look_dir = vec3(cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw));
        w = -unit_vector(look_dir);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta / 2);
        viewport_height = 2 * h * focus_dist;
        viewport_width = viewport_height * (double(render_width) / render_height);
        vec3 viewport_u = viewport_width * u;
        vec3 viewport_v = viewport_height * -v;
        pixel_delta_u = viewport_u / render_width;
        pixel_delta_v = viewport_v / render_height;
        auto viewport_upper_left = lookfrom - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    void save_ppm(const scene& sc) {
        std::ofstream ofs("../output/scene.ppm");
        if (!ofs.is_open()) {
            std::cerr << "Failed to open file\n";
            return;
        }
        ofs << "P3\n" << render_width << ' ' << render_height << "\n255\n";
        for (int j = 0; j < render_height; j++) {
            std::cerr << "\rScanlines remaining: " << (render_height - j) << ' ' << std::flush;
            for (int i = 0; i < render_width; ++i) {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; ++sample) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, sc);
                }
                write_color(ofs, pixel_samples_scale * pixel_color);
            }
        }
        ofs.close();
    }

    color ray_color(const ray& r, int depth, const scene& sc) const {
        const hittable& world = sc.get_world();
        if (depth <= 0)
            return color(0, 0, 0);
        hit_record rec;
        if (!world.hit(r, interval(0.001, infinity), rec)){
            if (sc.is_grid_shown() && std::abs(r.direction().y()) > 0.0001) {
                double t = -r.origin().y() / r.direction().y();
                
                // Only consider forward intersections
                if (t > 0.001) {
                    point3 intersection = r.at(t);
                    color grid_color;
                    if (sc.check_grid(intersection, grid_color)) {
                        return grid_color; 
                    }
                }
            }
            return background;
        }
        ray scattered;
        color attenuation;
        color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);
        if (!rec.mat->scatter(r, rec, attenuation, scattered))
            return color_from_emission;
        color color_from_scatter = attenuation * ray_color(scattered, depth - 1, sc);
        return color_from_emission + color_from_scatter;
    }

    ray get_ray(int i, int j, bool precise = false) const {
        auto offset = precise ? vec3(0.5, 0.5, 0) : sample_square();
        auto pixel_sample = pixel00_loc
                        + ((i + offset.x()) * pixel_delta_u)
                        + ((j + offset.y()) * pixel_delta_v);
        auto ray_origin = (defocus_angle <= 0 || precise) ? lookfrom : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = precise ? 0.0 : random_double();
        return ray(ray_origin, ray_direction, ray_time);
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();
        return lookfrom + (p.x() * defocus_disk_u) + (p.y() * defocus_disk_v);
    }

    vec3 sample_square() const {
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    double clamp(double x, double min, double max) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }
};

#endif

