//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

// #include "rtweekend.h"
#include "bvh.h"
#include "camera.h"
#include "hittable.h"
#include "material.h"
#include "quad.h"
#include "sphere.h"

void bouncing_spheres() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));
    auto ground_sphere = make_shared<sphere>(point3(0,-1000,0), 1000);
    ground_sphere->set_material(make_shared<lambertian>(checker));
    world.add(ground_sphere);

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = randomVec3() * randomVec3();
                    sphere_material = make_shared<lambertian>(make_shared<solid_color>(albedo.x, albedo.y, albedo.z));
                    auto center2 = center + vec3(0, random_double(0,.5), 0);
                    auto _sphere = make_shared<sphere>(center, center2, 0.2);
                    _sphere->set_material(sphere_material);
                    world.add(_sphere);
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = randomVec3(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(make_shared<solid_color>(albedo.x, albedo.y, albedo.z), fuzz);
                    auto _sphere = make_shared<sphere>(center, 0.2);
                    _sphere->set_material(sphere_material);
                    world.add(_sphere);
                } else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    auto _sphere = make_shared<sphere>(center, 0.2);
                    _sphere->set_material(sphere_material);
                    world.add(_sphere);
                }
            }
        }
    }

    auto sphere1 = make_shared<sphere>(point3(0, 1, 0), 1.0);
    sphere1->set_material(make_shared<dielectric>(1.5));
    world.add(sphere1);

    auto sphere2 = make_shared<sphere>(point3(-4, 1, 0), 1.0);
    sphere2->set_material(make_shared<lambertian>(make_shared<solid_color>(0.4, 0.2, 0.1)));
    world.add(sphere2);

    auto sphere3 = make_shared<sphere>(point3(4, 1, 0), 1.0);
    sphere3->set_material(make_shared<metal>(make_shared<solid_color>(0.7, 0.6, 0.5), 0.0));
    world.add(sphere3);

    world = hittable_list(make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.render_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0.70, 0.80, 1.00);

    cam.vfov     = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0.6;
    cam.focus_dist    = 10.0;

    cam.render(world);
}

void checkered_spheres() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));

    auto sphere1 = make_shared<sphere>(point3(0,-10, 0), 10);
    sphere1->set_material(make_shared<lambertian>(checker));
    world.add(sphere1);

    auto sphere2 = make_shared<sphere>(point3(0, 10, 0), 10);
    sphere2->set_material(make_shared<lambertian>(checker));
    world.add(sphere2);

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.render_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0.70, 0.80, 1.00);

    cam.vfov     = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void earth() {
    auto earth_texture = make_shared<image_texture>("earthmap.jpg");
    auto earth_sphere = make_shared<sphere>(point3(0,0,0), 2);
    earth_sphere->set_material(make_shared<lambertian>(earth_texture));
    hittable_list world;
    world.add(earth_sphere);

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.render_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0.70, 0.80, 1.00);

    cam.vfov     = 20;
    cam.lookfrom = point3(0,0,12);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void perlin_spheres() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    auto ground_sphere = make_shared<sphere>(point3(0,-1000,0), 1000);
    ground_sphere->set_material(make_shared<lambertian>(pertext));
    world.add(ground_sphere);

    auto _sphere = make_shared<sphere>(point3(0,2,0), 2);
    _sphere->set_material(make_shared<lambertian>(pertext));
    world.add(_sphere);

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.render_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0.70, 0.80, 1.00);

    cam.vfov     = 20;
    cam.lookfrom = point3(13,2,3);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void quads() {
    hittable_list world;

    // Materials with solid color textures
    auto left_red     = make_shared<lambertian>(make_shared<solid_color>(1.0, 0.2, 0.2));
    auto back_green   = make_shared<lambertian>(make_shared<solid_color>(0.2, 1.0, 0.2));
    auto right_blue   = make_shared<lambertian>(make_shared<solid_color>(0.2, 0.2, 1.0));
    auto upper_orange = make_shared<lambertian>(make_shared<solid_color>(1.0, 0.5, 0.0));
    auto lower_teal   = make_shared<lambertian>(make_shared<solid_color>(0.2, 0.8, 0.8));

    // Quads
    auto quad1 = make_shared<quad>(point3(-3,-2, 5), vec3(0, 0,-4), vec3(0, 4, 0));
    quad1->set_material(left_red);
    world.add(quad1);

    auto quad2 = make_shared<quad>(point3(-2,-2, 0), vec3(4, 0, 0), vec3(0, 4, 0));
    quad2->set_material(back_green);
    world.add(quad2);

    auto quad3 = make_shared<quad>(point3(3,-2, 1), vec3(0, 0, 4), vec3(0, 4, 0));
    quad3->set_material(right_blue);
    world.add(quad3);

    auto quad4 = make_shared<quad>(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4));
    quad4->set_material(upper_orange);
    world.add(quad4);

    auto quad5 = make_shared<quad>(point3(-2,-3, 5), vec3(4, 0, 0), vec3(0, 0,-4));
    quad5->set_material(lower_teal);
    world.add(quad5);

    camera cam;

    cam.aspect_ratio      = 1.0;
    cam.render_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0.70, 0.80, 1.00);

    cam.vfov     = 80;
    cam.lookfrom = point3(0,0,9);
    cam.lookat   = point3(0,0,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void simple_light() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    auto ground_sphere = make_shared<sphere>(point3(0,-1000,0), 1000);
    ground_sphere->set_material(make_shared<lambertian>(pertext));
    world.add(ground_sphere);

    auto _sphere = make_shared<sphere>(point3(0,2,0), 2);
    _sphere->set_material(make_shared<lambertian>(pertext));
    world.add(_sphere);

    auto difflight = make_shared<diffuse_light>(make_shared<solid_color>(4,4,4));
    auto light_sphere = make_shared<sphere>(point3(0,7,0), 2);
    light_sphere->set_material(difflight);
    world.add(light_sphere);

    auto light_quad = make_shared<quad>(point3(3,1,-2), vec3(2,0,0), vec3(0,2,0));
    light_quad->set_material(difflight);
    world.add(light_quad);

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.render_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0,0,0);

    cam.vfov     = 20;
    cam.lookfrom = point3(26,3,6);
    cam.lookat   = point3(0,2,0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void cornell_box() {
    hittable_list world;

    auto red   = make_shared<lambertian>(make_shared<solid_color>(.65, .05, .05));
    auto white = make_shared<lambertian>(make_shared<solid_color>(.73, .73, .73));
    auto green = make_shared<lambertian>(make_shared<solid_color>(.12, .45, .15));
    auto light = make_shared<diffuse_light>(make_shared<solid_color>(15, 15, 15));

    auto quad1 = make_shared<quad>(point3(555,0,0), vec3(0,555,0), vec3(0,0,555));
    quad1->set_material(green);
    world.add(quad1);

    auto quad2 = make_shared<quad>(point3(0,0,0), vec3(0,555,0), vec3(0,0,555));
    quad2->set_material(red);
    world.add(quad2);

    auto quad3 = make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105));
    quad3->set_material(light);
    world.add(quad3);

    auto quad4 = make_shared<quad>(point3(0,0,0), vec3(555,0,0), vec3(0,0,555));
    quad4->set_material(white);
    world.add(quad4);

    auto quad5 = make_shared<quad>(point3(555,555,555), vec3(-555,0,0), vec3(0,0,-555));
    quad5->set_material(white);
    world.add(quad5);

    auto quad6 = make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,555,0));
    quad6->set_material(white);
    world.add(quad6);

    shared_ptr<hittable> box1 = make_shared<box>(point3(0,0,0), point3(165,330,165));
    box1->set_material(white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265,0,295));
    world.add(box1);

    shared_ptr<hittable> box2 = make_shared<box>(point3(0,0,0), point3(165,165,165));
    box2->set_material(white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130,0,65));
    world.add(box2);

    camera cam;

    cam.aspect_ratio      = 1.0;
    cam.render_width       = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth         = 50;
    cam.background        = color(0,0,0);

    cam.vfov     = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat   = point3(278, 278, 0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void cornell_smoke() {
    hittable_list world;

    auto red   = make_shared<lambertian>(make_shared<solid_color>(.65, .05, .05));
    auto white = make_shared<lambertian>(make_shared<solid_color>(.73, .73, .73));
    auto green = make_shared<lambertian>(make_shared<solid_color>(.12, .45, .15));
    auto light = make_shared<diffuse_light>(make_shared<solid_color>(7, 7, 7));

    auto quad1 = make_shared<quad>(point3(555,0,0), vec3(0,555,0), vec3(0,0,555));
    quad1->set_material(green);
    world.add(quad1);

    auto quad2 = make_shared<quad>(point3(0,0,0), vec3(0,555,0), vec3(0,0,555));
    quad2->set_material(red);
    world.add(quad2);

    auto quad3 = make_shared<quad>(point3(113,554,127), vec3(330,0,0), vec3(0,0,305));
    quad3->set_material(light);
    world.add(quad3);

    auto quad4 = make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555));
    quad4->set_material(white);
    world.add(quad4);

    auto quad5 = make_shared<quad>(point3(0,0,0), vec3(555,0,0), vec3(0,0,555));
    quad5->set_material(white);
    world.add(quad5);

    auto quad6 = make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,555,0));
    quad6->set_material(white);
    world.add(quad6);

    shared_ptr<hittable> box1 = make_shared<box>(point3(0,0,0), point3(165,330,165));
    box1->set_material(white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265,0,295));

    shared_ptr<hittable> box2 = make_shared<box>(point3(0,0,0), point3(165,165,165));
    box2->set_material(white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130,0,65));

    world.add(make_shared<constant_medium>(box1, 0.01, color(0,0,0)));
    world.add(make_shared<constant_medium>(box2, 0.01, color(1,1,1)));

    camera cam;

    cam.aspect_ratio      = 1.0;
    cam.render_width       = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth         = 50;
    cam.background        = color(0,0,0);

    cam.vfov     = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat   = point3(278, 278, 0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void final_scene(int render_width, int samples_per_pixel, int max_depth) {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(make_shared<solid_color>(0.48, 0.83, 0.53));

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i*w;
            auto z0 = -1000.0 + j*w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1,101);
            auto z1 = z0 + w;

            auto box_obj = make_shared<box>(point3(x0,y0,z0), point3(x1,y1,z1));
            box_obj->set_material(ground);
            boxes1.add(box_obj);
        }
    }

    hittable_list world;

    world.add(make_shared<bvh_node>(boxes1));

    auto light = make_shared<diffuse_light>(make_shared<solid_color>(7, 7, 7));
    auto light_quad = make_shared<quad>(point3(123,554,147), vec3(300,0,0), vec3(0,0,265));
    light_quad->set_material(light);
    world.add(light_quad);

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30,0,0);
    auto sphere_material = make_shared<lambertian>(make_shared<solid_color>(0.7, 0.3, 0.1));
    auto moving_sphere = make_shared<sphere>(center1, center2, 50);
    moving_sphere->set_material(sphere_material);
    world.add(moving_sphere);

    auto glass_sphere = make_shared<sphere>(point3(260, 150, 45), 50);
    glass_sphere->set_material(make_shared<dielectric>(1.5));
    world.add(glass_sphere);

    auto metal_sphere = make_shared<sphere>(point3(0, 150, 145), 50);
    metal_sphere->set_material(make_shared<metal>(make_shared<solid_color>(0.8, 0.8, 0.9), 1.0));
    world.add(metal_sphere);

    auto boundary = make_shared<sphere>(point3(360,150,145), 70);
    boundary->set_material(make_shared<dielectric>(1.5));
    world.add(boundary);
    world.add(make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));

    auto large_boundary = make_shared<sphere>(point3(0,0,0), 5000);
    large_boundary->set_material(make_shared<dielectric>(1.5));
    world.add(large_boundary);
    world.add(make_shared<constant_medium>(large_boundary, .0001, color(1,1,1)));

    auto earth_texture = make_shared<image_texture>("earthmap.jpg");
    auto earth_sphere = make_shared<sphere>(point3(400,200,400), 100);
    earth_sphere->set_material(make_shared<lambertian>(earth_texture));
    world.add(earth_sphere);

    auto pertext = make_shared<noise_texture>(0.2);
    auto perlin_sphere = make_shared<sphere>(point3(220,280,300), 80);
    perlin_sphere->set_material(make_shared<lambertian>(pertext));
    world.add(perlin_sphere);

    hittable_list boxes2;
    auto white = make_shared<lambertian>(make_shared<solid_color>(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        auto small_sphere = make_shared<sphere>(randomVec3(0,165), 10);
        small_sphere->set_material(white);
        boxes2.add(small_sphere);
    }

    world.add(make_shared<translate>(
        make_shared<rotate_y>(
            make_shared<bvh_node>(boxes2), 15),
            vec3(-100,270,395)
        )
    );

    camera cam;

    cam.aspect_ratio      = 1.0;
    cam.render_width       = render_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth         = max_depth;
    cam.background        = color(0,0,0);

    cam.vfov     = 40;
    cam.lookfrom = point3(478, 278, -600);
    cam.lookat   = point3(278, 278, 0);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(world);


}


void render_scene(){
    scene scene;
    camera cam;
    cam.render(scene);
}

int main() {
    switch (0) {
        case 1:  bouncing_spheres();          break;
        case 2:  checkered_spheres();         break;
        case 3:  earth();                     break;
        case 4:  perlin_spheres();            break;
        case 5:  quads();                     break;
        case 6:  simple_light();              break;
        case 7:  cornell_box();               break;
        case 8:  cornell_smoke();             break;
        case 9:  final_scene(800, 10000, 40); break;
        default: render_scene(); break;
    }
}