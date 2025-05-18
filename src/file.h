
#include <cmath>
#include <iostream>
#include <ostream>

// void save_to_file(scene& world, std::string filename) {
//     // Ensure the filename has the correct extension
//     if (filename.find(".ze") == std::string::npos) {
//         filename += ".ze";
//     }

//     std::ofstream file(filename);
//     if (!file.is_open()) {
//         std::cerr << "Failed to open file for saving: " << filename << "\n";
//         return;
//     }

//     // Rest of the save logic as previously defined
//     file << "ZENGINE_SCENE_V1\n";
//     file << "GRID " << (world.is_grid_shown() ? "1" : "0") << " " 
//          << world.grid_lines->get_size() << " " << grid_lines->get_spacing() << "\n";
//     file << "OBJECTS " << world.objects.size() << "\n";
//     for (const auto& obj : objects) {
//         // Save logic as before
//     }

//     file.close();
//     std::clog << "Scene saved to " << filename << "\n";
// }


// // Load the scene from a file
// void load_from_file(const std::string& filename) {
//     std::ifstream file(filename);
//     if (!file.is_open()) {
//         std::cerr << "Failed to open file for loading: " << filename << "\n";
//         return;
//     }

//     // Clear the current scene
//     world.clear();
//     bvh_world.clear();
//     objects.clear();
//     remove_grid();
//     next_id = 0;

//     std::string line;
//     while (std::getline(file, line)) {
//         std::istringstream iss(line);
//         std::string type;
//         iss >> type;

//         if (type == "GRID") {
//             int grid_visible, size;
//             double spacing;
//             iss >> grid_visible >> size >> spacing;
//             show_grid = (grid_visible == 1);
//             grid_lines = std::make_shared<grid>(size, spacing, color(0.7, 0.7, 0.7));
//             if (show_grid) add_grid();
//         }
//         else if (type == "SPHERE") {
//             int id;
//             double x, y, z;
//             iss >> id >> x >> y >> z;
//             auto sphere0 = std::make_shared<sphere>(point3(x, y, z), 0.5, mats[rng() % rm], id);
//             world.add(sphere0);
//             objects.push_back(sphere0);
//             if (id >= next_id) next_id = id + 1;
//         }
//         // Add similar blocks for other object types (QUAD, BOX, etc.)
//         else if (type == "QUAD") {
//             int id;
//             double x, y, z;
//             iss >> id >> x >> y >> z;
//             point3 Q(x, y, z);
//             vec3 u(1, 0, 0), v(0, 1, 0);
//             auto quad0 = std::make_shared<quad>(Q, u, v, mats[rng() % rm], id);
//             world.add(quad0);
//             objects.push_back(quad0);
//             if (id >= next_id) next_id = id + 1;
//         }
//         // Continue for other types (BOX, TRIANGLE, etc.) following the same pattern
//     }

//     file.close();
//     update_bvh();
//     std::clog << "Scene loaded from " << filename << "\n";
// }
// ZENGINE_SCENE_V1
// GRID 1 20 0.5
// OBJECTS 2
// SPHERE 0 0.0 0.0 0.0 0 0.5
// SPHERE 1 -1.5 0.0 -2.0 1 0.5

// ZENGINE_SCENE_V1
// GRID <visible> <size> <spacing>
// OBJECTS <count>
// <OBJ_TYPE> <id> <x> <y> <z> <material_index> <param1> <param2> ...
// ...


