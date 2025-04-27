#ifndef SCENE_H
#define SCENE_H

#include "hittable.h"
#include "bvh.h"
#include "sphere.h"
#include "material.h"

class scene {
public:
    scene() {
        // Initial scene: ground and one sphere
        auto ground_material = std::make_shared<lambertian>(color(0.5, 0.5, 0.5));
        world.add(std::make_shared<sphere>(point3(0, -100.5, -1), 100, ground_material));
        
        auto sphere_material = std::make_shared<lambertian>(color(0.8, 0.3, 0.3));
        world.add(std::make_shared<sphere>(point3(0, 0, -1), 0.5, sphere_material));
        
        update_bvh();
    }

    void add_sphere() {
        // Add sphere with random x,z position, fixed y, radius, and random color
        double x = random_double(-2, 2);
        double z = random_double(-3, -1);
        color col(random_double(0.1, 0.9), random_double(0.1, 0.9), random_double(0.1, 0.9));
        auto material = std::make_shared<lambertian>(col);
        world.add(std::make_shared<sphere>(point3(x, 0, z), 0.3, material));
        update_bvh();
    }

    const hittable& get_world() const {
        return bvh_world;
    }

private:
    hittable_list world;
    hittable_list bvh_world;

    void update_bvh() {
        bvh_world.clear();
        auto bvh = std::make_shared<bvh_node>(world);
        bvh_world.add(bvh);
    }
};

#endif