#ifndef CAMERA_H
#define CAMERA_H

#include <SDL2/SDL.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include "hittable.h"
#include "utility.h"
#include "scene.h"
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

//TODO : Uncomment showInstructions
//TODO : Uncomment showDemo

class camera {
public:
    double aspect_ratio = 16.0 / 9.0;
    int render_width = 400;
    int window_width = 1280;
    int gui_width = 300;
    int samples_per_pixel = 1; // Reduced for real-time rendering
    int max_depth = 10;
    color background = color(0.5, 0.7, 1.0);
    double move_speed = 0.1;
    double mouse_sensitivity = 0.005;
    double vfov = 90;
    point3 lookfrom = point3(0, 1, 0);
    point3 lookat = point3(0, 0, -1);
    vec3 vup = vec3(0, 1, 0);
    double yaw = 0.0, pitch = 0.0;
    double defocus_angle = 0;
    double focus_dist = 1.0;

    camera() {
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
        // return true;
        return x >= 0 && y >= 0 && x < render_width && y < render_height;
    }

    void handle_poll_event(SDL_Event& event, scene& world) {

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))) {
                running = false;
                std::clog << "Quit event\n";
            }
            // Only process mouse and keyboard events if ImGui isn't capturing them
            if (!ImGui::GetIO().WantCaptureMouse) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT && valid(event.button.x, event.button.y)) {
                        int i = event.button.x;
                        int j = event.button.y;
                        auto r = get_ray(i, j);
                        if (world.select_object(r, 1000.0)) {
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
                        double t = -r.origin().y() / r.direction().y();
                        if (t > 0) {
                            point3 new_pos = r.at(t);
                            new_pos = point3(new_pos.x(), 0, new_pos.z());
                            world.move_selected(new_pos);
                        }
                    }
                }

            }



            if (!ImGui::GetIO().WantCaptureKeyboard) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                    std::clog << "Escape key pressed\n";
                }

                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p) {
                    save_ppm(world);
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

    bool render(scene& world) {
        SDL_GL_MakeCurrent(window, gl_context);

        std::vector<color> pixel_buffer(render_width * render_height, color(0, 0, 0));
        SDL_Event event;
        auto last_time = std::chrono::high_resolution_clock::now();
        const double target_frame_time = 1.0 / 60.0;


        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, 
            "Camera Controls", 
            "• Hold RIGHT MOUSE BUTTON and move to rotate camera\n"
            "• Use W/A/S/D or ARROW KEYS to move camera\n"
            "• LEFT MOUSE BUTTON to select objects\n"
            "• Press ESC to exit\n"
            "• Press P to save the scene as PPM", 
            window);

        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            double delta_time = std::chrono::duration<double>(now - last_time).count();
            last_time = now;

            std::clog << "Frame time : " << delta_time*1000 << " ms\n";  



            handle_poll_event(event, world);
            std::fill(pixel_buffer.begin(), pixel_buffer.end(), color(0, 0, 0));

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(float(render_width), 0));
            ImGui::SetNextWindowSize(ImVec2(float(gui_width), float(window_height)));
            ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            
            // Add camera debug info to UI
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", lookfrom.x(), lookfrom.y(), lookfrom.z());
            ImGui::Text("Yaw: %.2f, Pitch: %.2f", yaw, pitch);
            ImGui::Separator();
            
            // Camera controls instructions
            ImGui::TextWrapped("Hold right mouse button and move to rotate camera");
            ImGui::TextWrapped("WASD or arrow keys to move camera");
            ImGui::Separator();
            
            
            if (ImGui::Button("Add Sphere", ImVec2(160, 40))) {
                world.add_sphere();
                std::clog << "Button clicked: Add Sphere\n";
            }
            if (ImGui::Button("Add Rectangle", ImVec2(160, 40))) {
                world.add_rectangle();
                std::clog << "Button clicked: Add Rectangle\n";
            }
            if (ImGui::Button("Add Box", ImVec2(160, 40))) {
                world.add_box();
                std::clog << "Button clicked: Add Box\n";
            }
            if (ImGui::Button("Add Triangle", ImVec2(160, 40))) {
                world.add_triangle();
                std::clog << "Button clicked: Add Triangle\n";
            }
            if (ImGui::Button("Add Disk", ImVec2(160, 40))) {
                world.add_disk();
                std::clog << "Button clicked: Add Disk\n";
            }
            if (ImGui::Button("Add Ellipse", ImVec2(160, 40))) {
                world.add_ellipse();
                std::clog << "Button clicked: Add Ellipse\n";
            }
            if (ImGui::Button("Add Ring", ImVec2(160, 40))) {
                world.add_ring();
                std::clog << "Button clicked: Add Ring\n";
            }
            ImGui::End();

            ImGui::ShowDemoWindow();
            update_camera();


            const int num_threads = std::thread::hardware_concurrency();
            int rows_per_thread = render_height / num_threads + 1;
            std::vector<std::thread> threads;
            threads.reserve(num_threads);

            for (int t = 0; t < num_threads; ++t) {
                int start_row = t * rows_per_thread;
                int end_row = std::min(start_row + rows_per_thread, render_height);
                threads.emplace_back([this, &world, &pixel_buffer, start_row, end_row]() {
                    for (int j = start_row; j < end_row; ++j) {
                        for (int i = 0; i < render_width; ++i) {
                            int idx = j * render_width + i;
                            for(int _ = 0; _ < samples_per_pixel; _++){
                                ray r = get_ray(i, j);
                                pixel_buffer[idx] += ray_color(r, max_depth, world.get_world());
                            }
                        }
                    }
                });
            }

            for (auto& thread : threads) {
                thread.join();
            }

            std::vector<Uint32> pixel_data(render_width * render_height);
            SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
            if (!format) {
                std::cerr << "Failed to allocate pixel format: " << SDL_GetError() << "\n";
                running = false;
                break;
            }

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
            std::vector<Uint32> flipped_data(render_width * render_height);
            for (int j = 0; j < render_height; ++j) {
                memcpy(&flipped_data[j * render_width],
                    &pixel_data[(render_height - 1 - j) * render_width],
                    render_width * sizeof(Uint32));
            }



            glBindTexture(GL_TEXTURE_2D, render_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data.data());
            glBindTexture(GL_TEXTURE_2D, 0);


            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)render_width, (float)render_height));
            ImGui::Begin("Render", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoCollapse);
            ImGui::Image((ImTextureID)(uintptr_t)render_texture, ImVec2((float)render_width, (float)render_height));
            ImGui::End();



            // Render ImGui
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);

            // Cap frame rate
            double frame_time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - now).count();
            if (frame_time < target_frame_time) {
                SDL_Delay(Uint32((target_frame_time - frame_time) * 1000));
            }

        }

        return true;
    }
private:
    int window_height;
    int render_height;
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

    ImGuiIO io;
    bool running = true;
    bool mouse_grabbed = false;
    bool object_grabbed = false;

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

        window = SDL_CreateWindow("ImGui + SDL2 + OpenGL",
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
        update_camera();
        SDL_SetRelativeMouseMode(SDL_FALSE);
        std::clog << "Camera initialized\n";
        return true;
    }

    void update_camera() {
        vec3 look_dir = unit_vector(lookat - lookfrom);
        w = -look_dir;
        u = unit_vector(cross(vup, w));
        v = cross(w, u);
        look_dir = vec3(cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw));
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

    void save_ppm(const scene& world) {
        std::ofstream ofs("../output/scene.ppm");
        if (!ofs.is_open()) {
            std::cerr << "Failed to open file\n";
            return;
        }
        ofs << "P3\n" << render_width << ' ' << render_height << "\n255\n";
        for (int j = render_height - 1; j >= 0; --j) {
            std::cerr << "\rScanlines remaining: " << (render_height - j) << ' ' << std::flush;
            for (int i = 0; i < render_width; ++i) {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; ++sample) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, world.get_world());
                }
                write_color(ofs, pixel_samples_scale * pixel_color);
            }
        }
        ofs.close();
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        if (depth <= 0)
            return color(0, 0, 0);
        hit_record rec;
        if (!world.hit(r, interval(0.001, infinity), rec))
            return background;
        ray scattered;
        color attenuation;
        color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);
        if (!rec.mat->scatter(r, rec, attenuation, scattered))
            return color_from_emission;
        color color_from_scatter = attenuation * ray_color(scattered, depth - 1, world);
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