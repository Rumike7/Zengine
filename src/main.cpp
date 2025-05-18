#include <iostream>
#include <fstream>
#include <cmath>

#include "camera.h"

int main(int argc, char *argv[]) {
    scene scene;
    camera cam;
    cam.render(scene);

    return 0;
}