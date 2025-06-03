#ifndef CAMERA_H
#define CAMERA_H

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


struct Tile {
    int x0, x1, y0, y1;
};
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
    int render_width = 800;
    int window_width = 1000;
    int gui_width = 300;
    int control_height = 220;
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

    camera() : thread_pool(std::thread::hardware_concurrency()-1){
        if (!initialize()) {
            std::clog << "Camera initialization failed\n";
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
            if(!gui::savingPPM)handle_poll_event(event, sc);
            if(!gui::savingPPM)update_camera();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            

            gui::render_top_bar(sc, running, use_defocus, vfov, focus_dist, max_depth, samples_per_pixel, pixel_samples_scale, topbar_height, background);
            gui::render_object_buttons(sc, render_width, topbar_height, gui_width, window_height, st, lookfrom, yaw, pitch, render_height, control_height);
            

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
            // std::clog << "Compute time " << time*1000 << "ms\n";

            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glBindTexture(GL_TEXTURE_2D, render_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data.data());
            glBindTexture(GL_TEXTURE_2D, 0);


            
            ImGui::SetNextWindowPos(ImVec2(0, topbar_height));
            ImGui::SetNextWindowSize(ImVec2(float(render_width), float(render_height)));
            ImGui::Begin("Render", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoCollapse);
                ImGui::Image((ImTextureID)(uintptr_t)render_texture, ImVec2(float(render_width), float(render_height)));
        
            if (gui::savingPPM && ImGui::IsItemVisible()) {
                ImVec2 image_pos = ImGui::GetItemRectMin();
                ImVec2 image_size = ImGui::GetItemRectSize();
                
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(
                    image_pos, 
                    ImVec2(image_pos.x + image_size.x, image_pos.y + image_size.y),
                    IM_COL32(0, 0, 0, 128)
                );
                
                const char* loading_text = "Saving PPM Image...";
                ImVec2 text_size = ImGui::CalcTextSize(loading_text);
                ImVec2 text_pos = ImVec2(
                    image_pos.x + (image_size.x - text_size.x) * 0.5f,
                    image_pos.y + (image_size.y - text_size.y) * 0.5f
                );
                
                draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), loading_text);
                
                // Optional: Add a progress indicator or spinner
                static float rotation = 0.0f;
                rotation += ImGui::GetIO().DeltaTime * 2.0f;
                
                ImVec2 center = ImVec2(
                    image_pos.x + image_size.x * 0.5f,
                    image_pos.y + image_size.y * 0.5f + 30
                );
                
                float radius = 15.0f;
                for (int i = 0; i < 8; i++) {
                    float angle = rotation + (i * 3.14159f / 4);
                    float alpha = 1.0f - (i / 8.0f);
                    ImVec2 pos = ImVec2(
                        center.x + cos(angle) * radius,
                        center.y + sin(angle) * radius
                    );
                    draw_list->AddCircleFilled(pos, 3.0f, IM_COL32(255, 255, 255, (int)(alpha * 255)));
                }
            }
            if (gui::should_open_modal) {
                ImGui::OpenPopup("Add or Update Object");
                gui::should_open_modal = false;
            }
            gui::render_add_or_update_modal(sc, st);
            gui::renderSaveConfirmationPopup(sc);

            ImGui::SetNextWindowPos(ImVec2(render_width - 130, topbar_height + render_height - 50));
            ImGui::SetNextWindowSize(ImVec2(120, 40));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("##ResetCameraButton", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground 
            );

            ImGui::Button("Reset camera", ImVec2(
                ImGui::GetContentRegionAvail().x, 
                ImGui::GetContentRegionAvail().y  
            ));
            if (ImGui::IsItemClicked()) {
                if(!gui::savingPPM)reset_camera();
            }

            ImGui::End();
            ImGui::PopStyleVar();
            


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

    void savePPM(const scene& sc) {
        if(gui::savingPPM) {
            std::cerr << "Render is already in progress, please wait...\n";
            return;
        }
        const char* filterPatterns[1] = {"*.ppm"};
        const char* savePath = tinyfd_saveFileDialog(
            "Save Rendered Image",           
            "scene.ppm",                    
            1,                              
            filterPatterns,                 
            "PPM Image Files (*.ppm)"      
        );
        
        if (!savePath) {
            std::cerr << "Save operation cancelled by user\n";
            return;
        }
        int local_render_width = render_width;
        int local_render_height = render_height;        

        std::thread render_thread([this, savePath, &sc, local_render_height, local_render_width]() {
            std::ofstream ofs(savePath);
            if (!ofs.is_open()) {
                std::cerr << "Failed to open file: " << savePath << "\n";
                return;
            }

            float current_max_depth = 50;
            float current_samples_per_pixel = 100;
            float current_pixel_samples_scale = 1.0 / current_samples_per_pixel;

            gui::savingPPM = true;

            ofs << "P3\n" << local_render_width << ' ' << local_render_height << "\n255\n";
            if (!ofs.good()) {
                std::cerr << "Failed to write PPM header to file: " << savePath << "\n";
                ofs.close();
                gui::savingPPM = false;
                return;
            }
            for (int j = 0; j < local_render_height; j++) {
                std::cerr << "\rScanlines remaining: " << (local_render_height - j) << ' ' << std::flush;
                for (int i = 0; i < local_render_width; ++i) {
                    color pixel_color(0, 0, 0);
                    for (int sample = 0; sample < current_samples_per_pixel; ++sample) {
                        ray r = get_ray(i, j);
                        pixel_color += ray_color(r, current_max_depth, sc);
                    }
                    write_color(ofs, current_pixel_samples_scale * pixel_color);
                }
            }
            bool success = ofs.good();
            ofs.close();

            gui::savingPPM = false;

            if (success) {
                std::cerr << "\nRender saved successfully to: " << savePath << "\n";
            } else {
                std::cerr << "\nRender failed to save completely to: " << savePath << "\n";
            }
        });

        render_thread.detach();

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
        ...
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

    ...
    

   
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
                        auto r = get_ray(i, j, true);
                        if (sc.select_object(r, 1000.0)) {
                            object_grabbed = true;
                            std::clog << "Object selected\n";
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {

                        mouse_grabbed = true;
                        std::clog << "Camera grabbed\n";
                        int i = event.button.x;
                        int j = event.button.y;
                        auto r = get_ray(i, j, true);
                        if(valid(event.button.x, event.button.y)){
                            if (sc.select_object(r, 1000.0)) {
                                std::clog << "render object menu\n";
                                gui::open_menu_id = sc.get_selected_object_id();
                            }
                            
                        }
                    }
                }

                if (event.type == SDL_MOUSEBUTTONUP) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (object_grabbed) std::clog << "Object released\n";
                        object_grabbed = false;
                        sc.move_selected(point3(0,0,0), 2);
                        gui::isSaved = false;
                        gui::updateWindowTitle(sc.getName());
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
                    savePPM(sc);
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


    
    ...

};

#endif


