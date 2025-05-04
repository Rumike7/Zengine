#include <iostream>
#include <fstream>
#include <cmath>

#include "camera.h"
#include "scene.h"
#include <glad/glad.h> 


int main() {
    scene scene;
    camera cam;
    cam.render(scene);
}