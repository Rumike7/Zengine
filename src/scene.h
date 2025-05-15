#ifndef SCENE_H
#define SCENE_H

#include "hittable.h"
#include "sphere.h"
#include "quad.h"
#include "material.h"
#include "utility.h"
#include "bvh.h"
#include "texture.h"
#include "rtw_stb_image.h"
#include "constant_medium.h"


std::mt19937 rng(45218965);
int rm = 6;

class scene {
public:
    scene() {
        initialize();
        auto ground_material = std::make_shared<lambertian>(color(0.5, 0.5, 0.5));
        world.add(std::make_shared<sphere>(point3(0, -1000.5, -1), 1000, ground_material, next_id++));
        update_bvh();
    }

    void add_sphere() {
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        auto sphere0 = std::make_shared<sphere>(point3(2, 0, -1.5), 0.5, make_shared<lambertian>(color(0, 0, 0)), next_id++);
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


    void add_object(point3 pos, shared_ptr<material> mat, std::string obj_name, int object_type){
        std::shared_ptr<hittable> obj;

        switch (object_type) {
            case 0: { // Sphere
                obj = std::make_shared<sphere>(pos, 0.5, mat, next_id++);
                break;
            }
            case 1: { // Moving Sphere
                point3 center2(pos.x() + random_double(-0.2, 0.2), pos.y(), pos.z() + random_double(-0.2, 0.2));
                obj = std::make_shared<sphere>(pos, center2, 0.3, mat, next_id++);
                break;
            }
            case 2: { // Quad
                vec3 u(1, 0, 0), v(0, 1, 0);
                obj = std::make_shared<quad>(pos, u, v, mat, next_id++);
                break;
            }
            case 3: { // Box
                point3 a(pos.x() - 0.3, pos.y() - 0.3, pos.z() - 0.3);
                point3 b(pos.x() + 0.3, pos.y() + 0.3, pos.z() + 0.3);
                obj = std::make_shared<box>(a, b, mat, next_id++);
                break;
            }
            case 4: { // Triangle
                vec3 u(0.5, 0, 0), v(0, 0.5, 0);
                obj = std::make_shared<triangle>(pos, u, v, mat, next_id++);
                break;
            }
            case 5: { // Rectangle
                vec3 u(random_double(0.5, 1.5), 0, 0), v(0, random_double(0.5, 1.5), 0);
                obj = std::make_shared<rectangle>(pos, u, v, mat, next_id++);
                break;
            }
            case 6: { // Disk
                vec3 u(0.6, 0, 0), v(0, 0.6, 0);
                obj = std::make_shared<disk>(pos, u, v, mat, next_id++);
                break;
            }
            case 7: { // Ellipse
                vec3 u(0.8, 0, 0), v(0, 0.5, 0);
                obj = std::make_shared<ellipse>(pos, u, v, mat, next_id++);
                break;
            }
            case 8: { // Ring
                vec3 u(0.6, 0, 0), v(0, 0.6, 0);
                obj = std::make_shared<ring>(pos, u, v, mat, next_id++);
                break;
            }
        }

        world.add(obj);
        objects.push_back(obj);
        update_bvh();

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



private:
    hittable_list world;
    hittable_list bvh_world;
    std::vector<std::shared_ptr<material>> mats;
    std::vector<std::shared_ptr<hittable>> objects; // Track objects for selection
    int selected_object_id = -1; // Currently selected object
    int next_id = 0; // Incremental ID for objects

    void update_bvh() {
        bvh_world.clear();
        auto bvh = std::make_shared<bvh_node>(world);
        bvh_world.add(bvh);
    }
};

#endif