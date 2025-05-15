#include <iostream>
#include <fstream>
#include <cmath>

#include "camera.h"
#include "scene.h"
#include <glad/glad.h> 
#include <SDL2/SDL.h>


int main(int argc, char *argv[]) {
    scene scene;
    camera cam;
    cam.render(scene);


    return 0;
}