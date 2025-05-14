#ifndef SCENE_H
#define SCENE_H

#include "hittable.h"
#include "sphere.h"
#include "quad.h"
#include "material.h"
#include "bvh.h"
#include "texture.h"

std::mt19937 rng(45218965);
int rm = 6;

class scene {
public:
    scene() {
        initialize();
        grid_lines = std::make_shared<grid>(20, 0.5, color(0.7, 0.7, 0.7));
        
        // Add grid lines to the scene
        if (show_grid) {
            add_grid();
        }
        update_bvh();
    }

    void add_sphere() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        auto sphere0 = std::make_shared<sphere>(point3(0, 0, 0), 0.5, make_shared<lambertian>(color(0, 0, 0)), next_id++);
        world.add(sphere0);
        objects.push_back(sphere0);
        update_bvh();
        std::clog << world << "\n";
    }
    void add_sphere_mov() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 center1(x, 0, z);
        point3 center2(x + random_double(-0.2, 0.2), 0, z + random_double(-0.2, 0.2));
        auto sphere0 = std::make_shared<sphere>(center1, center2, 0.3, mats[rng() % rm], next_id++);
        world.add(sphere0);
        objects.push_back(sphere0);
        update_bvh();
    }


    void add_quad() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 Q(x, 0, z);
        vec3 u(1, 0, 0);
        vec3 v(0, 1, 0);
        auto quad0 = std::make_shared<quad>(Q, u, v, mats[rng() % rm], next_id++);
        world.add(quad0);
        objects.push_back(quad0);
        update_bvh();
    }

    void add_box() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 a(x - 0.3, -0.3, z - 0.3);
        point3 b(x + 0.3, 0.3, z + 0.3);
        auto box0 = std::make_shared<box>(a, b, mats[rng() % rm], next_id++);
        world.add(box0);
        objects.push_back(box0);
        update_bvh();
    }

    void add_triangle() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 Q(x, 0, z);
        vec3 u(0.5, 0, 0);
        vec3 v(0, 0.5, 0);
        auto triangle0 = std::make_shared<triangle>(Q, u, v, mats[rng() % rm], next_id++);
        world.add(triangle0);
        objects.push_back(triangle0);
        update_bvh();
    }

    void add_rectangle() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 Q(x, 0, z);
        vec3 u(random_double(0.5, 1.5), 0, 0);
        vec3 v(0, random_double(0.5, 1.5), 0);
        auto rectangle0 = std::make_shared<rectangle>(Q, u, v, mats[rng() % rm], next_id++);
        world.add(rectangle0);
        objects.push_back(rectangle0);
        update_bvh();
    }

    void add_disk() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 Q(x, 0, z);
        vec3 u(0.6, 0, 0);
        vec3 v(0, 0.6, 0);
        auto disk0 = std::make_shared<disk>(Q, u, v, mats[rng() % rm], next_id++);
        world.add(disk0);
        objects.push_back(disk0);
        update_bvh();
    }

    void add_ellipse() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 Q(x, 0, z);
        vec3 u(0.8, 0, 0);
        vec3 v(0, 0.5, 0);
        auto ellipse0 = std::make_shared<ellipse>(Q, u, v, mats[rng() % rm], next_id++);
        world.add(ellipse0);
        objects.push_back(ellipse0);
        update_bvh();
    }

    void add_ring() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        point3 Q(x, 0, z);
        vec3 u(0.6, 0, 0);
        vec3 v(0, 0.6, 0);
        auto ring0 = std::make_shared<ring>(Q, u, v, mats[rng() % rm], next_id++);
        world.add(ring0);
        objects.push_back(ring0);
        update_bvh();
    }

    bool select_object(const ray& r, double max_t) {
        hit_record rec;
        double closest_t = max_t;
        int selected_id = -1;

        for (const auto& obj : objects) {
            if (obj->hit(r, interval(0.001, closest_t), rec)) {
                closest_t = rec.t;
                selected_id = obj->get_id();
                std::clog <<"Hit " <<(*obj) << "\n";
            }
        }

        if (selected_id != -1) {
            selected_object_id = selected_id;
            return true;
        }
        selected_object_id = -1;
        return false;
    }

    point3 get_selected_position(){
        if(selected_object_id == -1)
        for (auto& obj : objects) {
            if (obj->get_id() == selected_object_id) {
                aabb bbox = obj->bounding_box();
                point3 center = (bbox.min() + bbox.max()) * 0.5;
                return center;
            }
        }
        return point3(0, 0, 0);
    }

    void move_selected(const point3& new_pos) {
        if (selected_object_id == -1) return;

        for (auto& obj : objects) {
            if (obj->get_id() == selected_object_id) {
                aabb bbox = obj->bounding_box();
                point3 center = (bbox.min() + bbox.max()) * 0.5;
                vec3 offset = new_pos - center;
                // Wrap the object in a translate instance
                obj->move_by(offset);
                // std::clog << (*obj) << "\n";
                update_bvh();

                break;
            }
        }
    }

    const hittable& get_world() const {
        return bvh_world;
    }

    void initialize() {
        mats.clear();
        mats.push_back(std::make_shared<lambertian>(color(0.8, 0.3, 0.3)));
        mats.push_back(std::make_shared<lambertian>(color(0.2, 0.8, 0.2)));
        mats.push_back(std::make_shared<metal>(color(0.8, 0.8, 0.8), 0.1));
        mats.push_back(std::make_shared<dielectric>(1.5));
        mats.push_back(std::make_shared<diffuse_light>(color(4.0, 4.0, 4.0)));
        mats.push_back(std::make_shared<isotropic>(color(0.5, 0.5, 0.5)));
    }
    bool is_grid_shown() const {
        return show_grid;
    }

    void toggle_grid() {
        show_grid = !show_grid;
        
        // Remove old grid lines if they exist
        remove_grid();
        
        // Add new grid lines if enabled
        if (show_grid) {
            add_grid();
        }
        
        update_bvh();
    }
    
    void set_grid_size(int size, double spacing) {
        // Remove old grid
        remove_grid();
        
        // Create new grid with specified size
        grid_lines = std::make_shared<grid>(size, spacing, color(0.7, 0.7, 0.7));
        
        // Add new grid if enabled
        if (show_grid) {
            add_grid();
        }
        
        update_bvh();
    }



private:
    hittable_list world;
    hittable_list bvh_world;
    std::vector<std::shared_ptr<material>> mats;
    std::vector<std::shared_ptr<hittable>> objects; // Track objects for selection
    int selected_object_id = -1; // Currently selected object
    int next_id = 0; // Incremental ID for objects
    std::vector<std::shared_ptr<hittable>> grid_objects; // Track grid objects separately

    void update_bvh() {
        if(world.objects.size()){
            bvh_world.clear();
            auto bvh = std::make_shared<bvh_node>(world);
            bvh_world.add(bvh);

        }
    }

        std::shared_ptr<grid> grid_lines;
    bool show_grid = true; // Grid visibility toggle
    
    // Add grid lines to the scene
    void add_grid() {
        if (!grid_lines) return;
        
        // Get the grid lines
        auto lines = grid_lines->get_lines();
        
        // Add them to the world and track them
        for (const auto& line : lines) {
            world.add(line);
            grid_objects.push_back(line);
        }
    }
    
    // Remove grid lines from the scene
    void remove_grid() {
        for (const auto& obj : grid_objects) {
            world.remove(obj);
        }
        grid_objects.clear();
    }

};

#endif