#include <iostream>
#include <fstream>
#include <cmath>
#include "hittable_list.h"
#include "utility.h"
#include "sphere.h"
#include "hittable.h"
#include "camera.h"


// Main function
int main() {
    std::cerr << "Starting ray tracer...\n";
    
    
    hittable_list world;

    world.add(make_shared<sphere>(point3(0,0,-1), 0.5));
    world.add(make_shared<sphere>(point3(0,-100.5,-1), 100));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width  = 400;

    cam.render(world);
    return 0;
}