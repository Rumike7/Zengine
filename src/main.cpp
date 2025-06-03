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
#ifdef _WIN32
#include <windows.h>
#endif


void render_scene(){
    scene scene;
    camera cam;
    cam.render(scene);
}

int main(int argc, char* argv[]) {
    
    #ifdef _WIN32
    AllocConsole(); 
    freopen("CONOUT$", "w", stdout); 
    freopen("CONOUT$", "w", stderr);
    std::clog << "Console initialized" << std::endl;
    #endif
    
    render_scene();
    return 0;
}