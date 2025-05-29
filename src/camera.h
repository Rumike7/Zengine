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
#include "gui.h"
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <iomanip>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <../external/tinyfiledialogs.h>
#include <filesystem>
namespace fs = std::filesystem;

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
                    notify_task_completion();
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
            total_tasks++;
        }
        condition.notify_one();
    }

    void wait_for_completion() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        completion_condition.wait(lock, [this] { return tasks_completed >= total_tasks; });
    }


    void notify_task_completion() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks_completed++;
        }
        completion_condition.notify_one();
    }

    void reset_completion() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks_completed = 0;
        total_tasks = 0;
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable completion_condition;
    bool stop;
    size_t tasks_completed;
    size_t total_tasks = 0;
};


class camera {
public:
    double aspect_ratio = 16.0 / 9.0;
    int render_width = 600;
    int window_width = 900;
    int gui_width = 300;
    int samples_per_pixel = 4;
    int max_depth = 2;
    color background = color(0.5, 0.7, 1.0);
    double move_speed = 0.1;
    double mouse_sensitivity = 0.005;
    float vfov = 20;
    point3 lookfrom = point3(10,1,0);
    point3 lookat = point3(0, 0, 0);
    vec3 vup = vec3(0, 1, 0);
    double yaw = 0.0, pitch = 0.0;
    double defocus_angle = 0.6;
    float focus_dist = 10.0;
    bool use_defocus = false;
    int button_width = 80;
    int button_height = 40;

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
        if (gui::window) SDL_DestroyWindow(gui::window);
        SDL_Quit();
    }

    bool valid(int x, int y) {
        return x >= 0 && y >= 0 && x < render_width && y < render_height;
    }


    bool render(scene& sc) {
        SDL_GL_MakeCurrent(gui::window, gl_context);
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
            gui::window);

        auto last_time = std::chrono::high_resolution_clock::now();
        const double target_frame_time = 1.0 / 60.0;
        std::atomic<int> completed_rows{0};
        int rows_per_task = std::max(1, int(render_height / (std::thread::hardware_concurrency() * 4)));
        
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
            gui::render_top_bar(sc, running, use_defocus, vfov, focus_dist, max_depth, samples_per_pixel, pixel_samples_scale, topbar_height);
            RenderObjectButtons(sc);

            completed_rows = 0;
            std::fill(pixel_buffer.begin(), pixel_buffer.end(), color(0, 0, 0));
            for (int start_row = 0; start_row < render_height; start_row += rows_per_task) {
                int end_row = std::min(start_row + rows_per_task, render_height);
                thread_pool.enqueue([this, &sc, start_row, end_row, &format, &completed_rows]() {
                    for (int j = start_row; j < end_row; ++j) {
                        for (int i = 0; i < render_width; ++i) {
                            int idx = j * render_width + i;
                            for (int _ = 0; _ < samples_per_pixel; ++_) {
                                ray r = get_ray(i, j);
                                this->pixel_buffer[idx] += ray_color(r, max_depth, sc);
                            }

                            color c = this->pixel_buffer[idx] * pixel_samples_scale;
                            c = color(sqrt(c.x), sqrt(c.y), sqrt(c.z));
                            this->pixel_data[idx] = SDL_MapRGBA(
                                format,
                                Uint8(clamp(c.x, 0.0, 1.0) * 255.99),
                                Uint8(clamp(c.y, 0.0, 1.0) * 255.99),
                                Uint8(clamp(c.z, 0.0, 1.0) * 255.99),
                                255
                            );

                        }
                        completed_rows++;
                    }
                });
            }

            while (completed_rows < render_height) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            thread_pool.wait_for_completion();
            thread_pool.reset_completion();

            auto time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - last_time).count();
            std::clog << "Compute time " << time*1000 << "ms\n";

            // Update OpenGL texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glBindTexture(GL_TEXTURE_2D, render_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data.data());
            glBindTexture(GL_TEXTURE_2D, 0);

            // Render image
            ImGui::SetNextWindowPos(ImVec2(0, topbar_height));
            ImGui::SetNextWindowSize(ImVec2(float(render_width), float(render_height)));
            ImGui::Begin("Render", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoCollapse);
            ImGui::Image((ImTextureID)(uintptr_t)render_texture, ImVec2(float(render_width), float(render_height)));
            
            ImGui::SetNextWindowPos(ImVec2(render_width - 130, topbar_height + render_height - 50));
            ImGui::SetNextWindowSize(ImVec2(120, 40));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));  // Remove all padding
            ImGui::Begin("##ResetCameraButton", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground 
            );

            // Button fills entire available space
            ImGui::Button("Reset camera", ImVec2(
                ImGui::GetContentRegionAvail().x,  // Fill width exactly
                ImGui::GetContentRegionAvail().y   // Fill height exactly
            ));
            if (ImGui::IsItemClicked()) {
                reset_camera();
            }

            ImGui::End();
            ImGui::PopStyleVar();  // Restore original padding


            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(gui::window);

            double frame_time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - now).count();
            if (frame_time < target_frame_time) {
                SDL_Delay(Uint32((target_frame_time - frame_time) * 1000));
            }
            sc.rebuild_bvh();
        }

        SDL_FreeFormat(format);
        return true;
    }

    void render(const hittable_list& sc) {
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
    double threshold = 0.001;
    std::vector<color> pixel_buffer;
    std::vector<Uint32> pixel_data;




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
        
        SDL_GetWindowSize(gui::window, &window_width, &window_height);

        render_width = std::max(600, window_width - gui_width);
        render_height = int(render_width / aspect_ratio);
        render_height = (render_height < 1) ? 1 : render_height;
        render_height = std::min(450, (render_height < 1) ? 1 : render_height);

        window_height = int(window_width / aspect_ratio);
        window_height = (window_height < 1) ? 1 : window_height;

        std::clog << "Render-screen window size: " << render_width << "x" << render_height << "\n";
        std::clog << "Full-screen window size: " << window_width << "x" << window_height << "\n";


        gui::window = SDL_CreateWindow("Untitled - ZEngine",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        gl_context = SDL_GL_CreateContext(gui::window);
        SDL_GL_MakeCurrent(gui::window, gl_context);
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
        io.Fonts->AddFontFromFileTTF("../assets/MaterialIcons-Regular.ttf", 16.0f, &icons_config, icons_ranges);
        io.Fonts->Build();

        glGenTextures(1, &render_texture);
        glBindTexture(GL_TEXTURE_2D, render_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render_width, render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        pixel_buffer.resize(render_width * render_height, color(0, 0, 0));
        pixel_data.resize(render_width * render_height);



        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(gui::window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 330");

        pixel_samples_scale = 1.0 / samples_per_pixel;
        vec3 look_dir = unit_vector(lookat - lookfrom);

        pitch = asin(look_dir.y); 

        float cos_pitch = sqrt(1.0 - look_dir.y * look_dir.y); 
        if (abs(cos_pitch) > 1e-6) { 
            yaw = atan2(look_dir.z, look_dir.x);//arctan(y/x)
        } else {
            yaw = 0.0; 
        }
        std::clog << look_dir << " " << yaw << " " << pitch << "\n";
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

    color ray_color(const ray& r, int depth, const scene& sc) const {
        if (depth <= 0)
            return color(0, 0, 0);

        const hittable& world = sc.get_world();
        hit_record rec;
        if (world.hit(r, interval(threshold, infinity), rec)) {
            ray scattered;
            color attenuation;
            color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);
            if (!rec.mat->scatter(r, rec, attenuation, scattered))
                return color_from_emission;
            return color_from_emission + attenuation * ray_color(scattered, depth - 1, sc);
        }

        if (sc.is_grid_shown() && std::abs(r.direction().y) > threshold) {
            double t = -r.origin().y / r.direction().y;
            if (t > threshold) {
                point3 intersection = r.at(t);
                color grid_color;
                if (sc.check_grid(intersection, grid_color)) {
                    return grid_color;
                }
            }
        }
        return background;
    }
    color ray_color(const ray& r, int depth, const hittable_list& sc) const {
        if (depth <= 0)
            return color(0, 0, 0);

        const hittable& world = sc;
        hit_record rec;
        if (world.hit(r, interval(threshold, infinity), rec)) {
            ray scattered;
            color attenuation;
            color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);
            if (!rec.mat->scatter(r, rec, attenuation, scattered))
                return color_from_emission;
            return color_from_emission + attenuation * ray_color(scattered, depth - 1, sc);
        }
        return background;
    }

    ray get_ray(int i, int j, bool precise = false) const {
        auto offset = precise ? vec3(0.5, 0.5, 0) : sample_square();
        auto pixel_sample = pixel00_loc
                        + ((i + offset.x) * pixel_delta_u)
                        + ((j + offset.y) * pixel_delta_v);
        auto ray_origin = (defocus_angle <= 0 || precise || !use_defocus) ? lookfrom : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = precise ? 0.0 : random_double();
        return ray(ray_origin, ray_direction, ray_time);
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();
        return lookfrom + (p.x * defocus_disk_u) + (p.y * defocus_disk_v);
    }

    vec3 sample_square() const {
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    double clamp(double x, double min, double max) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
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

            // Context menu with keyboard shortcuts
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Update", "Ctrl+U", false, can_update)) {
                    selected_object_id = id;
                    addObjectModal = true;      
                    std::clog << "Updating ... " << selected_object_id << " " << addObjectModal <<" \n";                  
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Update object properties");
                }

                if (ImGui::MenuItem("Delete", "Ctrl+D", false, can_delete)) {
                    confirmDeleteModal = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Permanently delete object");
                }
        

                if (ImGui::MenuItem("Duplicate", "Ctrl+T", false, can_duplicate)) {
                    sc.duplicate_object(id);
                    gui::isSaved = false;    
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Create a copy of the object");
                }

                ImGui::EndMenu();
            }

            if(confirmDeleteModal){
                ImGui::OpenPopup("Confirm the deletion");
            }

            // Delete confirmation dialog
            if (ImGui::BeginPopupModal("Confirm the deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to delete '%s'?", object->get_name().c_str());
                ImGui::Separator();
                
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    
                    sc.delete_object(id);
                    gui::isSaved = false;    
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            ImGui::PopID();  

            if (addObjectModal && !ImGui::IsPopupOpen("Add or Update Object")) {
                std::clog << "Can open, IsPopupOpen: " << ImGui::IsPopupOpen("Add or Update Object") << "\n";
                ImGui::OpenPopup("Add or Update Object");
            }
        } catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error rendering menu");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_U) && io.KeyCtrl && can_update) {
            ImGui::OpenPopup("Add or Update Object");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl && can_delete) {
            ImGui::OpenPopup("Confirm Delete");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T) && io.KeyCtrl && can_duplicate) {
            int new_id = sc.duplicate_object(id);
            gui::isSaved = false;    

        }
    }   
    
    void render_add_or_update_modal(scene& sc){
        if (ImGui::BeginPopupModal("Add or Update Object", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {            
            static float color1[3] = {0.8f, 0.3f, 0.3f}; // Initialize with defaults
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

            gui::render_objects_attribute(sc);

            int current_material_type = static_cast<int>(st.material_type);
            if (ImGui::Combo("Material", &current_material_type, material_names.data(), material_names.size())) {
                st.material_type = static_cast<MaterialType>(current_material_type);
                std::clog << current_material_type << "\n";
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
                            // Reconstruct full path (assuming the directory remains the same)
                            size_t last_slash = st.texture_file.find_last_of("/\\");
                            if (last_slash != std::string::npos) {
                                st.texture_file = st.texture_file.substr(0, last_slash + 1) + texture_file_buffer;
                            } else {
                                st.texture_file = texture_file_buffer; // No path, just filename
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
                            // Fallback to default if no file selected
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
                gui::isSaved = false;
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


    

    void RenderObjectButtons(scene& sc) {
        ImGui::SetNextWindowPos(ImVec2(float(render_width), topbar_height));
        ImGui::SetNextWindowSize(ImVec2(float(gui_width), float(window_height)-topbar_height));
        ImGui::Begin("Scene Objects", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4)); // Smaller padding
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));  // Spacing between buttons

        float button_size = 32.0f; 
        float padding = ImGui::GetStyle().ItemSpacing.x;
        float available_width = ImGui::GetContentRegionAvail().x;

        // Compute max number of columns that fit
        int columns = std::max(1, int((available_width + padding) / (button_size + padding)));

        bool addObjectModal = false;

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
                    st.name = object_type_map[st.object_type].first;
                    addObjectModal = true;
                    std::clog << "Button clicked\n";
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", button.tooltip.c_str()); // Use c_str() for ImGui
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }
            
            ImGui::EndTable();
        }

        ImGui::PopStyleVar(2);

        if(addObjectModal){
            ImGui::OpenPopup("Add or Update Object");
        }

        ImGui::Separator();

        if (ImGui::Button("\uf067 Add Object", ImVec2(160, 40))) {
            ImGui::OpenPopup("Add or Update Object");
        }
        
        ImGui::Separator();


        ImGui::Text("Objects in Scene:");
        ImGui::Text("");
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        auto objects = sc.get_objects();
        for(auto [id, object] : objects){
            render_object_menu(sc, object, id);

        }
        ImGui::PopStyleVar();

        render_add_or_update_modal(sc);


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
        ImGui::TextWrapped("To update, delete or duplicate an object, put the cursor on the object NAME and click on RIGHT mouse button");

        ImGui::Separator();

        ImGui::End();  


    }
    void reset_camera() {
        vfov = 20;
        lookfrom = point3(10,1,0);
        lookat = point3(0, 0, 0);
        vup = vec3(0, 1, 0);
        yaw = 0.0, pitch = 0.0;
        defocus_angle = 0.6;
        focus_dist = 10.0;
        use_defocus = false;
        vec3 look_dir = unit_vector(lookat - lookfrom);
        pitch = asin(look_dir.y); 

        float cos_pitch = sqrt(1.0 - look_dir.y * look_dir.y); 
        if (abs(cos_pitch) > 1e-6) { 
            yaw = atan2(look_dir.z, look_dir.x);//arctan(y/x)
        } else {
            yaw = 0.0; 
        }

        update_camera();
    }


    std::string ExtractFilename(const std::string& path) {
        size_t last_slash = path.find_last_of("/\\");
        return (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    }

    void handle_poll_event(SDL_Event& event, scene& sc) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(gui::window))) {
                running = false;
                std::clog << "Quit event\n";
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_width = event.window.data1;
                window_height = event.window.data2;
                update_render_dimensions();
                pixel_buffer.resize(render_width * render_height, color(0, 0, 0));
                pixel_data.resize(render_width * render_height);
            }
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
                        gui::isSaved = false;
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
            if (!io.WantCaptureKeyboard) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                    std::clog << "Escape key pressed\n";
                }

                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p) {
                    hittable_list world;
                    render(hittable_list(sc.get_world_ptr()));
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
    }


    
    void update_render_dimensions() {
        render_width = std::max(600, window_width - gui_width);
        render_height = int(render_width / aspect_ratio);
        render_height = std::min(450, (render_height < 1) ? 1 : render_height);


        // Update the OpenGL texture
        glBindTexture(GL_TEXTURE_2D, render_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render_width, render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Update camera parameters
        update_camera();
    }
    

};

#endif


