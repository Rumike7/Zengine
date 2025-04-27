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

struct gui_button {
    SDL_Rect rect;
    std::string label;
    bool (*action)(scene&); // Function to call on click
};

class camera {
public:
    double aspect_ratio = 16.0 / 9.0;          // Ratio of image width over height
    int render_width = 400;                    // Rendered image width
    int render_height = 225;                   // Rendered image height
    int window_width = 600;                    // Total window width (render + GUI)
    int window_height = 400;                    // Total window width (render + GUI)
    int samples_per_pixel = 1;                 // Count of random samples for each pixel
    int max_depth = 10;                        // Maximum number of ray bounces into scene
    color background = color(0.5, 0.7, 1.0);   // Scene background color
    double move_speed = 0.1;                   // Camera movement speed
    double mouse_sensitivity = 0.005;          // Mouse look sensitivity
    double vfov = 90;                          // Vertical field of view (degrees)
    point3 lookfrom = point3(0, 0, 0);         // Camera position
    point3 lookat = point3(0, 0, -1);          // Point camera is looking at
    vec3 vup = vec3(0, 1, 0);                  // Camera-relative "up" direction
    double yaw = 0.0, pitch = 0.0;             // Euler angles for mouse look
    double defocus_angle = 0;                  // Variation angle of rays through each pixel
    double focus_dist = 1.0;                   // Distance to plane of perfect focus

    camera() {
        if (!initialize()) {
            std::cerr << "Camera initialization failed\n";
            throw std::runtime_error("SDL initialization failed");
        }

        // Initialize GUI
        gui_buttons.push_back({
            {render_width + 20, 20, 160, 40}, // x, y, w, h
            "Add Sphere",
            [](scene& s) { s.add_sphere(); return true; }
        });

    }

    ~camera() {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool render(const scene& world) {
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, render_width, render_height);
        if (!texture) {
            std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
            return false;
        }

        std::vector<color> pixel_buffer(render_width * render_height);
        bool running = true;
        SDL_Event event;
        auto last_time = std::chrono::high_resolution_clock::now();
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            double delta_time = std::chrono::duration<double>(now - last_time).count();
            last_time = now;
            std::clog << "Frame time: " << delta_time * 1000 << " ms\n";

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (event.key.keysym.sym == SDLK_p) save_ppm(world);
                }
                if (event.type == SDL_MOUSEMOTION) {
                    yaw -= event.motion.xrel * mouse_sensitivity;
                    pitch -= event.motion.yrel * mouse_sensitivity;
                    if (pitch > 1.57) pitch = 1.57;
                    if (pitch < -1.57) pitch = -1.57;
                }
            }

            const Uint8* keys = SDL_GetKeyboardState(NULL);
            vec3 forward(cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw));
            vec3 right(-sin(yaw), 0, cos(yaw));
            if (keys[SDL_SCANCODE_W]) lookfrom = lookfrom + forward * move_speed;
            if (keys[SDL_SCANCODE_S]) lookfrom = lookfrom - forward * move_speed;
            if (keys[SDL_SCANCODE_A]) lookfrom = lookfrom - right * move_speed;
            if (keys[SDL_SCANCODE_D]) lookfrom = lookfrom + right * move_speed;

            update_camera();

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // Draw GUI
            draw_gui();

            // Multithreaded rendering
            const int num_threads = std::thread::hardware_concurrency();
            std::vector<std::vector<color>> pixel_buffers(num_threads);
            int rows_per_thread = render_height / num_threads + 1;
            
            std::vector<std::thread> threads;
            threads.reserve(num_threads);

            for (int t = 0; t < num_threads; ++t) {
                int start_row = t * rows_per_thread;
                int end_row = std::min(start_row + rows_per_thread, render_height);
                threads.emplace_back([this, &world, &pixel_buffer, start_row, end_row]() {
                    for (int j = start_row; j < end_row; ++j) {
                        for (int i = 0; i < render_width; ++i) {
                            color pixel_color(0, 0, 0);
                            for (int sample = 0; sample < samples_per_pixel; ++sample) {
                                ray r = get_ray(i, j);
                                pixel_color += ray_color(r, max_depth, world.get_world());
                            }
                            pixel_buffer[j * render_width + i] = pixel_color * pixel_samples_scale;
                        }
                    }
                });
            }

            for (auto& thread : threads) {
                thread.join();
            }

            // Update texture
            void* pixels;
            int pitch;
            SDL_LockTexture(texture, NULL, &pixels, &pitch);
            SDL_PixelFormat* format = SDL_AllocFormat(SDL_GetWindowPixelFormat(window));
            if (!format) {
                std::cerr << "Failed to allocate pixel format: " << SDL_GetError() << "\n";
                SDL_UnlockTexture(texture);
                SDL_DestroyTexture(texture);
                return false;
            }

            for (int j = 0; j < render_height; ++j) {
                for (int i = 0; i < render_width; ++i) {
                    int idx = j * render_width + i;
                    color c = pixel_buffer[idx] / samples_per_pixel;
                    c = color(sqrt(c.x()), sqrt(c.y()), sqrt(c.z())); // Gamma correction
                    ((Uint32*)pixels)[idx] = SDL_MapRGBA(
                        format,
                        Uint8(c.x() * 255.99), // Red (swapped)
                        Uint8(c.y() * 255.99), // Green
                        Uint8(c.z() * 255.99), // Blue (swapped)
                        255
                    );
                }
            }
            SDL_FreeFormat(format);
            SDL_UnlockTexture(texture);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            
            // Draw crosshair (white, 10-pixel lines)
            // SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White
            // int center_x = width / 2;
            // int center_y = height / 2;
            // int crosshair_size = 10;
            // SDL_RenderDrawLine(renderer, center_x - crosshair_size, center_y, center_x + crosshair_size, center_y); // Horizontal
            // SDL_RenderDrawLine(renderer, center_x, center_y - crosshair_size, center_x, center_y + crosshair_size); // Vertical
                
            SDL_RenderPresent(renderer);
        }
        
        SDL_DestroyTexture(texture);       
        return true;
    }

private:
    point3 pixel00_loc;                 // Location of pixel 0, 0
    vec3 pixel_delta_u;                 // Offset to pixel to the right
    vec3 pixel_delta_v;                 // Offset to pixel below
    double pixel_samples_scale;         // Color scale factor for pixel samples
    vec3 u, v, w;                       // Camera frame basis vectors
    vec3 defocus_disk_u;                // Defocus disk horizontal radius
    vec3 defocus_disk_v;                // Defocus disk vertical radius
    double viewport_width;              // Viewport width
    double viewport_height;             // Viewport height
    SDL_Window* window = nullptr;       // SDL window
    SDL_Renderer* renderer = nullptr;   // SDL renderer
    std::vector<gui_button> gui_buttons; // GUI elements

    bool initialize() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
            return false;
        }

        render_height = int(render_width / aspect_ratio);
        render_height = (render_height < 1) ? 1 : render_height;

        window = SDL_CreateWindow("Zengine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, 0);
        if (!window) {
            std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
            SDL_Quit();
            return false;
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }

        pixel_samples_scale = 1.0 / samples_per_pixel;
        update_camera();

        // SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetRelativeMouseMode(SDL_FALSE); // Disable for GUI interaction

        std::clog << "Camera initialized\n";
        return true;
    }

    void update_camera() {
        vec3 look_dir(cos(pitch) * cos(yaw), sin(pitch), cos(pitch) * sin(yaw));
        w = unit_vector(look_dir);
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
        std::clog << "PPM saved\n";
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

    ray get_ray(int i, int j) const {
        auto offset = sample_square();
        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);
        auto ray_origin = (defocus_angle <= 0) ? lookfrom : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_double();
        return ray(ray_origin, ray_direction, ray_time);
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();
        return lookfrom + (p.x() * defocus_disk_u) + (p.y() * defocus_disk_v);
    }

    vec3 sample_square() const {
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    void draw_gui() {
        // Draw GUI panel background (gray)
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect panel = {render_width, 0, window_width - render_width, window_height};
        SDL_RenderFillRect(renderer, &panel);

        // Draw buttons
        for (const auto& button : gui_buttons) {
            // Button background (blue)
            SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255);
            SDL_RenderFillRect(renderer, &button.rect);
            // Button border (white)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &button.rect);
        }
    }

    void handle_gui_click(int x, int y, scene& world) {
        for (auto& button : gui_buttons) {
            if (x >= button.rect.x && x <= button.rect.x + button.rect.w &&
                y >= button.rect.y && y <= button.rect.y + button.rect.h) {
                button.action(world);
                std::clog << "Button clicked: " << button.label << "\n";
            }
        }
    }
};

#endif