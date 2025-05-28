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
    int render_width = 800;
    int window_width = 1400;
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
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool valid(int x, int y) {
        return x >= 0 && y >= 0 && x < render_width && y < render_height;
    }


    bool render(scene& sc) {
        SDL_GL_MakeCurrent(window, gl_context);
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
        int rows_per_task = std::max(1, int(render_height / (std::thread::hardware_concurrency() * 4)));
        
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            double delta_time = std::chrono::duration<double>(now - last_time).count();
            last_time = now;
            std::clog << "Frame time: " << delta_time * 1000 << " ms\n";

            SDL_Event event;
            handle_poll_event(event, sc);
            update_camera();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            ...

            completed_rows = 0;
            std::fill(pixel_buffer.begin(), pixel_buffer.end(), color(0, 0, 0));
            for (int start_row = 0; start_row < render_height; start_row += rows_per_task) {
                int end_row = std::min(start_row + rows_per_task, render_height);
                thread_pool.enqueue([this, &sc, start_row, end_row, &completed_rows]() {
                    for (int j = start_row; j < end_row; ++j) {
                        for (int i = 0; i < render_width; ++i) {
                            int idx = j * render_width + i;
                            for (int _ = 0; _ < samples_per_pixel; ++_) {
                                ray r = get_ray(i, j);
                                this->pixel_buffer[idx] += ray_color(r, max_depth, sc);
                            }
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
            completed_rows = 0;
            for (int start_row = 0; start_row < render_height; start_row += rows_per_task) {
                int end_row = std::min(start_row + rows_per_task, render_height);
                thread_pool.enqueue([this, &format, start_row, end_row, &completed_rows]() {
                    for (int j = start_row; j < end_row; ++j) {
                        for (int i = 0; i < render_width; ++i) {
                            int idx = j * render_width + i;
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
                ImGuiWindowFlags_NoBackground  // Optional: removes window background
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
            SDL_GL_SwapWindow(window);

            double frame_time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - now).count();
            if (frame_time < target_frame_time) {
                SDL_Delay(Uint32((target_frame_time - frame_time) * 1000));
            }
            // sc.rebuild_bvh();
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
    std::vector<std::string> recent_files;
    double threshold = 0.001;
    std::vector<color> pixel_buffer;
    std::vector<Uint32> pixel_data;
    bool isSaved = true;




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
        
        SDL_GetWindowSize(window, &window_width, &window_height);

        render_width = std::max(600, window_width - gui_width);
        render_height = int(render_width / aspect_ratio);
        render_height = (render_height < 1) ? 1 : render_height;
        render_height = std::min(450, (render_height < 1) ? 1 : render_height);

        window_height = int(window_width / aspect_ratio);
        window_height = (window_height < 1) ? 1 : window_height;

        std::clog << "Render-screen window size: " << render_width << "x" << render_height << "\n";
        std::clog << "Full-screen window size: " << window_width << "x" << window_height << "\n";


        window = SDL_CreateWindow("Untitled - ZEngine",
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

        pixel_buffer.resize(render_width * render_height, color(0, 0, 0));
        pixel_data.resize(render_width * render_height);



        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
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


    color ray_color_adaptive(const ray& r, int depth, const scene& sc, int i, int j, std::vector<color>& pixel_buffer) const {
        color total_color(0, 0, 0);
        int samples = samples_per_pixel;
        std::vector<color> sample_colors;

        // Initial samples
        for (int s = 0; s < samples; ++s) {
            ray sample_ray = get_ray(i, j);
            color c = ray_color(sample_ray, depth, sc);
            total_color += c;
            sample_colors.push_back(c);
        }

        // Compute variance
        color mean = total_color / samples;
        double variance = 0.0;
        for (const auto& c : sample_colors) {
            double diff = (c - mean).length2();
            variance += diff;
        }
        variance /= samples;

        // Add more samples if variance is high
        if (variance > 0.01) { // Adjust threshold based on quality needs
            for (int s = 0; s < samples_per_pixel / 2; ++s) {
                ray sample_ray = get_ray(i, j);
                total_color += ray_color(sample_ray, depth, sc);
                samples++;
            }
        }

        return total_color / samples;
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


    ...

    void handle_poll_event(SDL_Event& event, scene& sc) {
        ...
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


