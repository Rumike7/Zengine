#ifndef SCENE_H
#define SCENE_H

#include "hittable.h"
#include "sphere.h"
#include "quad.h"
#include "material.h"
#include "bvh.h"
#include "texture.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <regex>
#include <stack>
#include "file.h" // Assumed for file I/O
#include <array>

// Define object types for type-safe handling
enum class ObjectType {
    Sphere,
    MovingSphere,
    Box,
    Triangle,
    Rectangle,
    Disk,
    Ellipse,
    Ring,
    Count
};
enum class TextureType{
    SolidColor,
    Checker,
    Image, 
    Noise,
    Count
};
enum class MaterialType{
    Lambertian,
    Metal,
    Dielectric,
    DiffuseLight,
    Isotropic,
    Count
};
struct state {
    ObjectType object_type = ObjectType::Sphere;
    vec3 scale = vec3(1, 1, 1); 
    vec3 rotation = vec3(0, 0, 0);
    // Add other state properties as needed
    point3 position = {0.0f, 0.0f, -1.5f}; 
    TextureType texture_type = TextureType::SolidColor;
    MaterialType material_type = MaterialType::Lambertian;
    std::string name = "Object"; 
    color color_values = color(0.8f, 0.3f, 0.3f); 
    color color_values0 = color(0.8f, 0.3f, 0.3f); 
    double refraction_index = 1.0;
    bool color_picker_open = false;
    double texture_scale = 0.1;
    std::string texture_file = "../assets/earthmap.jpg";
    float noise_scale = 4.0f;
    float fuzz = 0.1f;

    static constexpr std::array<const char*, static_cast<size_t>(ObjectType::Count)> object_types = {
        "Sphere", "Moving Sphere", "Box", "Triangle", "Rectangle", "Disk", "Ellipse", "Ring"
    };

    static constexpr std::array<const char*, 5> material_names = {
        "Lambertian", "Metal", "Dielectric", "Diffuse Light", "Isotropic"
    };

    static constexpr std::array<const char*, 4> texture_types = {
        "Color", "Checker", "Image", "Noise"
    };

    void reset(){
        object_type = ObjectType::Sphere;
        scale = vec3(1, 1, 1); 
        rotation = vec3(0, 0, 0);
        // Add other state properties as needed
        position = {0.0f, 0.0f, -1.5f}; 
        texture_type = TextureType::SolidColor;
        material_type = MaterialType::Lambertian;
        name = "Object"; 
        color_values = {0.8f, 0.3f, 0.3f};
        color_values0 = {0.8f, 0.3f, 0.3f};
        refraction_index = 1.0;
        color_picker_open = false;
        texture_scale = 0.1;
        texture_file = "../assets/earthmap.jpg";
        noise_scale = 4.0f;
        fuzz = 0.1f;
    }
};

class scene {
private: 
    // Command pattern for undo/redo
    class Command {
    public:
        virtual ~Command() = default;
        virtual void execute(bool shouldMove = true) = 0;
        virtual void undo() = 0;
    };

    class AddCommand : public Command {
    public:
        AddCommand(scene* s, std::shared_ptr<hittable> obj, int id)
            : scene_(s), obj_(obj), id_(id), state_(s->states[id]) {}
        void execute(bool shouldMove = true) override {
            scene_->object_map[id_] = obj_;
            scene_->states[id_] = state_;
            scene_->bvh_world->insert(obj_);
        }
        void undo() override {
            scene_->object_map.erase(id_);
            scene_->states.erase(id_);
            scene_->bvh_world->remove(obj_);
        }
    private:
        scene* scene_;
        std::shared_ptr<hittable> obj_;
        int id_;
        state state_;
    };

    class DeleteCommand : public Command {
    public:
        DeleteCommand(scene* s, std::shared_ptr<hittable> obj, int id)
            : scene_(s), obj_(obj), id_(id), state_(s->states[id]) {}
        void execute(bool shouldMove = true) override {
            scene_->object_map.erase(id_);
            scene_->states.erase(id_);
            scene_->bvh_world->remove(obj_);
        }
        void undo() override {
            scene_->object_map[id_] = obj_;
            scene_->states[id_] = state_;
            scene_->bvh_world->insert(obj_);
        }
    private:
        scene* scene_;
        std::shared_ptr<hittable> obj_;
        int id_;
        state state_;
    };

    class MoveCommand : public Command {
    public:
        MoveCommand(scene* s, int id, const vec3& offset)
            : scene_(s), id_(id), offset_(offset) {}
        void execute(bool shouldMove = true) override {
            auto it = scene_->object_map.find(id_);
            if (it != scene_->object_map.end() && shouldMove) {
                it->second->move_by(offset_);
                scene_->bvh_world->update(it->second);
            }
            scene_->states[id_].position += offset_;
        }
        void undo() override {
            auto it = scene_->object_map.find(id_);
            if (it != scene_->object_map.end()) {
                it->second->move_by(-offset_);
                scene_->bvh_world->update(it->second);
                scene_->states[id_].position -= offset_;
            }
        }
    private:
        scene* scene_;
        int id_;
        vec3 offset_;
    };

public:
    scene() {
        object_map.reserve(100);
        states.reserve(100);
        initialize();
        grid_visualization = std::make_shared<grid>(20, 0.5);
    }

    // Select an object by casting a ray
    bool select_object(const ray& r, double max_t) {
        hit_record rec;
        double closest_t = max_t;
        int selected_id = -1;

        for (const auto& [id, obj] : object_map) {
            if (obj->hit(r, interval(0.001, closest_t), rec)) {
                closest_t = rec.t;
                selected_id = id;
                std::clog << "Hit " << (*obj) << "\n";
            }
        }

        selected_object_id = selected_id;
        return selected_id != -1;
    }

    // Get the position (center) of the selected object
    point3 get_selected_position() const {
        if (selected_object_id == -1) {
            return point3(0, 0, 0);
        }
        auto it = object_map.find(selected_object_id);
        if (it != object_map.end()) {
            aabb bbox = it->second->bounding_box();
            return (bbox.min() + bbox.max()) * 0.5;
        }
        return point3(0, 0, 0);
    }

    // Move the selected object to a new position
    //Ismoving 1
    //EndMoving 2
    //shouldMove 0
    void move_selected(const point3& new_pos, int shouldMove = 0) {
        if (selected_object_id == -1) return;

        auto it = object_map.find(selected_object_id);
        if (it != object_map.end()) {
            aabb bbox = it->second->bounding_box();
            point3 center = (bbox.min() + bbox.max()) * 0.5;
            vec3 offset = new_pos - center;
            if(shouldMove == 1){
                accumulated_offset += offset;
                it->second->move_by(offset);
                bvh_world->update(it->second);
            }else if(shouldMove == 2){
                execute_command(std::make_unique<MoveCommand>(this, selected_object_id, accumulated_offset), false);
                accumulated_offset = vec3(0, 0, 0);
            }else{
                execute_command(std::make_unique<MoveCommand>(this, selected_object_id, offset), true);
            }
        }
    }

    // Add or update an object in the scene
    void add_or_update_object(const state& st, int id_object = -1, bool shouldMove = true) {
        std::shared_ptr<texture> tex;
        switch(st.texture_type) {
            case TextureType::SolidColor:               
                tex = std::make_shared<solid_color>(st.color_values);
                break;
            case TextureType::Checker: 
                tex = std::make_shared<checker_texture>(
                    st.texture_scale,
                    st.color_values,
                    st.color_values0
                );
                break;
            case TextureType::Image: 
                tex = std::make_shared<image_texture>(st.texture_file.data());
                break;
            case TextureType::Noise:
                tex = std::make_shared<noise_texture>(st.noise_scale);
                break;
            default:
                tex = std::make_shared<solid_color>(st.color_values);
        }

        shared_ptr<material> mat;
        switch(st.material_type){
            case MaterialType::Lambertian:
                mat = std::make_shared<lambertian>(tex);
                break;
            case MaterialType::Metal:
                mat = std::make_shared<metal>(tex, st.fuzz);
                break;
            case MaterialType::Dielectric:
                mat = std::make_shared<dielectric>(st.refraction_index);
                break;
            case MaterialType::DiffuseLight:
                mat = std::make_shared<diffuse_light>(tex);
                break;
            case MaterialType::Isotropic:
                mat = std::make_shared<isotropic>(tex);
                break;
            default:
                mat = std::make_shared<lambertian>(tex);
        }
        if (id_object != -1 && shouldMove) {
            auto it = object_map.find(id_object);
            auto obj = it->second;
            obj->set_name(generate_unique_name(st.name));
            obj->set_material(mat);
            apply_transformations(obj, st);
            std::clog << st.color_values << "\n";
            object_map[id_object] = obj;
            states[id_object] = st;
            selected_object_id = id_object;
            move_selected(st.position);
            return;
        }
        shared_ptr<hittable> obj;
        switch (st.object_type) {
            case ObjectType::Sphere: {
                obj = std::make_shared<sphere>(st.position, 0.5);
                obj->set_icon("\uf111");
                break;
            }
            case ObjectType::MovingSphere: {
                point3 center2(st.position.x() + random_double(-0.2, 0.2), st.position.y(), st.position.z() + random_double(-0.2, 0.2));
                obj = std::make_shared<sphere>(st.position, center2, 0.3);
                obj->set_icon("\uf111");
                break;
            }
            case ObjectType::Box: {
                point3 a(st.position.x() - 0.3, st.position.y() - 0.3, st.position.z() - 0.3);
                point3 b(st.position.x() + 0.3, st.position.y() + 0.3, st.position.z() + 0.3);
                obj = std::make_shared<box>(a, b);
                obj->set_icon("\uf466");
                break;
            }
            case ObjectType::Triangle: {
                vec3 u(0.5, 0, 0), v(0, 0.5, 0);
                obj = std::make_shared<triangle>(st.position, u, v);
                obj->set_icon("\uf0de");
                break;
            }
            case ObjectType::Rectangle: {
                vec3 u(random_double(0.5, 1.5), 0, 0), v(0, random_double(0.5, 1.5), 0);
                obj = std::make_shared<rectangle>(st.position, u, v);
                obj->set_icon("\uf0c8");
                break;
            }
            case ObjectType::Disk: {
                vec3 u(0.6, 0, 0), v(0, 0.6, 0);
                obj = std::make_shared<disk>(st.position, u, v);
                obj->set_icon("\uf192");
                break;
            }
            case ObjectType::Ellipse: {
                vec3 u(0.8, 0, 0), v(0, 0.5, 0);
                obj = std::make_shared<ellipse>(st.position, u, v);
                obj->set_icon("\uf192");
                break;
            }
            case ObjectType::Ring: {
                vec3 u(0.6, 0, 0), v(0, 0.6, 0);
                obj = std::make_shared<ring>(st.position, u, v);
                obj->set_icon("\uf0a3");
                break;
            }
            default:
                obj = nullptr;
        }
        if (!obj) return; 


        obj->set_name(generate_unique_name(st.name));
        obj->set_id(next_id);
        obj->set_material(mat);
        apply_transformations(obj, st);


        if(id_object != -1){//init
            object_map[id_object] = obj;
            bvh_world->insert(obj);
            return;
        }
        states[next_id] = st;
        object_map[next_id] = obj;

        execute_command(std::make_unique<AddCommand>(this, obj, next_id));
        next_id++;
    }

    // Delete an object by ID
    void delete_object(int id) {
        auto it = object_map.find(id);
        if (it != object_map.end()) {
            auto obj = it->second;
            object_map.erase(it);
            states.erase(id);
            execute_command(std::make_unique<DeleteCommand>(this, obj, id));
        }
    }

    // Duplicate an object by ID
    int duplicate_object(int id) {
        auto it = object_map.find(id);
        if (it == object_map.end()) return -1;

        auto obj = it->second;
        aabb bbox = obj->bounding_box();
        point3 center = (bbox.min() + bbox.max()) * 0.5;
        auto mat = obj->get_material();
        std::string name = obj->get_name();
        state st = states[id];

        add_or_update_object(st);
        return next_id - 1;
    }

    const hittable& get_world() const {
        return *bvh_world;
    }

    // Initialize the scene with a default object
    void initialize() {
        bvh_world = make_shared<bvh_node>();
        state st;
        st.position = point3(0, 0, 0);        
        st.color_values = color(0.8f, 0.3f, 0.3f);
        add_or_update_object(st);
    }

    state get_state(int id){
        return states[id];
    }
    // Grid-related functions
    bool is_grid_shown() const { return show_grid; }

    void toggle_grid() {
        show_grid = !show_grid;
    }

    void set_grid_size(int size, double spacing) {
        grid_visualization = std::make_shared<grid>(size, spacing);
    }

    bool check_grid(const point3& point, color& grid_color) const {
        if (!show_grid || !grid_visualization) return false;
        return grid_visualization->get_color_at(point, grid_color);
    }


    std::unordered_map<int, std::shared_ptr<hittable>> get_objects() const {
        return object_map;
    }

    // Get/set selected object ID
    int get_selected_object_id() const { return selected_object_id; }
    void set_selected_object_id(int id) {
        if (id == -1 || object_map.find(id) != object_map.end()) {
            selected_object_id = id;
        }
    }

    // Undo/redo support
    void undo() {
        if (!undo_stack.empty()) {
            auto cmd = std::move(undo_stack.top());
            undo_stack.pop();
            cmd->undo();
            redo_stack.push(std::move(cmd));
        }
        std::clog << "Undo " <<undo_stack.size() << " " << redo_stack.size() << "\n";
    }

    void redo() {
        if (!redo_stack.empty()) {
            auto cmd = std::move(redo_stack.top());
            redo_stack.pop();
            cmd->execute();
            undo_stack.push(std::move(cmd));
        }
        std::clog << "Redo "<< undo_stack.size() << " " << redo_stack.size() << "\n";
    }

    void save_to_file(const std::string& filename) const;

    void load_from_file(const std::string& filename);

    void load_new(){
        // Clear current scene
        object_map.clear();
        states.clear();
        bvh_world = make_shared<bvh_node>();
        next_id = 0;
    }

private:
    std::unordered_map<int, std::shared_ptr<hittable>> object_map; // O(1) lookup by ID
    std::unordered_map<int, state> states; 
    shared_ptr<bvh_node> bvh_world; 
    std::shared_ptr<grid> grid_visualization;
    bool show_grid = true; 
    int selected_object_id = -1; 
    int next_id = 0;
    vec3 accumulated_offset;
    bool is_moving = false; 
    vec3 move_start_pos; 

    // Undo/redo stacks
    std::stack<std::unique_ptr<class Command>> undo_stack;
    std::stack<std::unique_ptr<class Command>> redo_stack;


    // Apply transformations (scale, rotation) to an object
    void apply_transformations(std::shared_ptr<hittable> obj, const state& st) {
        // Note: Requires hittable to support scale/rotate methods
        // Example implementation:
        // obj->scale(st.scale);
        // obj->rotate(st.rotation);
        // If not supported, wrap in a transformation instance
    }

    std::string generate_unique_name(const std::string& base_name) const {
        static std::unordered_map<std::string, int> name_counters;
        std::string name = base_name.empty() ? "object" : base_name;
        int& counter = name_counters[name];
        if (counter == 0) {
            for (const auto& [id, obj] : object_map) {
                if (obj->get_name() == name) {
                    counter = 1;
                    break;
                }
            }
        }
        if (counter == 0) {
            return name;
        }
        std::string unique_name = name + "_" + std::to_string(counter);
        counter++;
        return unique_name;
    }

    void execute_command(std::unique_ptr<Command> cmd, bool shouldMove = true) {
        cmd->execute(shouldMove);
        undo_stack.push(std::move(cmd));
        redo_stack = std::stack<std::unique_ptr<Command>>();
    }
};

#endif


void scene::save_to_file(const std::string& filename) const{
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Header
    const char magic[] = "ZSC";
    uint32_t version = 1; // Format version
    uint32_t state_count = static_cast<uint32_t>(states.size());
    uint32_t n_id = static_cast<uint32_t>(next_id);
    uint32_t sh_grid = static_cast<uint32_t>(show_grid);

    out.write(magic, 3);
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&state_count), sizeof(state_count));
    out.write(reinterpret_cast<const char*>(&n_id), sizeof(n_id));
    out.write(reinterpret_cast<const char*>(&sh_grid), sizeof(sh_grid));

    // Serialize each state
    for (const auto& [id, s] : states) {
        // Write state ID
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));

        // Write enum values
        uint32_t obj_type = static_cast<uint32_t>(s.object_type);
        uint32_t tex_type = static_cast<uint32_t>(s.texture_type);
        uint32_t mat_type = static_cast<uint32_t>(s.material_type);
        out.write(reinterpret_cast<const char*>(&obj_type), sizeof(obj_type));
        out.write(reinterpret_cast<const char*>(&tex_type), sizeof(tex_type));
        out.write(reinterpret_cast<const char*>(&mat_type), sizeof(mat_type));

        // Write vec3 and point3
        float3 scale = s.scale.to_float3();
        float3 rotation = s.rotation.to_float3();
        float3 position = s.position.to_float3();
        out.write(reinterpret_cast<const char*>(&scale), sizeof(scale));
        out.write(reinterpret_cast<const char*>(&rotation), sizeof(rotation));
        out.write(reinterpret_cast<const char*>(&position), sizeof(position));

        // Write name (length-prefixed string)
        uint32_t name_len = static_cast<uint32_t>(s.name.size());
        out.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        out.write(s.name.data(), name_len);

        // Write arrays
        float3 color_values = s.color_values.to_float3();
        float3 color_values0 = s.color_values0.to_float3();
        out.write(reinterpret_cast<const char*>(&color_values), sizeof(color_values));
        out.write(reinterpret_cast<const char*>(&color_values0), sizeof(color_values0));

        // Write scalars
        out.write(reinterpret_cast<const char*>(&s.refraction_index), sizeof(s.refraction_index));
        out.write(reinterpret_cast<const char*>(&s.color_picker_open), sizeof(s.color_picker_open));
        out.write(reinterpret_cast<const char*>(&s.texture_scale), sizeof(s.texture_scale));

        // Write texture_file (fixed-size char array)
        out.write(s.texture_file.data(), s.texture_file.size());

        // Write remaining floats
        out.write(reinterpret_cast<const char*>(&s.noise_scale), sizeof(s.noise_scale));
        out.write(reinterpret_cast<const char*>(&s.fuzz), sizeof(s.fuzz));
    }

    if (!out.good()) {
        throw std::runtime_error("Error occurred while writing to file: " + filename);
    }
}

void scene::load_from_file(const std::string& filename) {
    load_new();


    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }

    // Read and validate header
    char magic[3];
    uint32_t version;
    uint32_t state_count;
    uint32_t n_id;
    uint32_t sh_grid;

    in.read(magic, 3);
    if (magic[0] != 'Z' || magic[1] != 'S' || magic[2] != 'C') {
        throw std::runtime_error("Invalid .zsc file: incorrect magic number");
    }

    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        throw std::runtime_error("Unsupported .zsc file version: " + std::to_string(version));
    }
    in.read(reinterpret_cast<char*>(&state_count), sizeof(state_count));
    in.read(reinterpret_cast<char*>(&n_id), sizeof(n_id));
    in.read(reinterpret_cast<char*>(&sh_grid), sizeof(sh_grid));

    next_id = (int)n_id;
    show_grid = (bool)sh_grid;

    // Clear existing states
    states.clear();

    std::clog << "Number of object :" << state_count << "\n";
    // Read each state
    for (uint32_t i = 0; i < state_count; ++i) {
        state s;
        int id;

        // Read state ID
        in.read(reinterpret_cast<char*>(&id), sizeof(id));

        // Read enum values
        uint32_t obj_type, tex_type, mat_type;
        in.read(reinterpret_cast<char*>(&obj_type), sizeof(obj_type));
        in.read(reinterpret_cast<char*>(&tex_type), sizeof(tex_type));
        in.read(reinterpret_cast<char*>(&mat_type), sizeof(mat_type));

        // Validate enums
        if (obj_type >= static_cast<uint32_t>(ObjectType::Count) ||
            tex_type >= static_cast<uint32_t>(TextureType::Count) ||
            mat_type >= static_cast<uint32_t>(MaterialType::Count)) {
            throw std::runtime_error("Invalid enum value in file");
        }

        s.object_type = static_cast<ObjectType>(obj_type);
        s.texture_type = static_cast<TextureType>(tex_type);
        s.material_type = static_cast<MaterialType>(mat_type);

        // Read vec3 and point3
        float3 scale;
        float3 rotation;
        float3 position;
        in.read(reinterpret_cast<char*>(&scale), sizeof(scale));
        in.read(reinterpret_cast<char*>(&rotation), sizeof(rotation));
        in.read(reinterpret_cast<char*>(&position), sizeof(position));
        s.scale = vec3(scale.x, scale.y, scale.z);
        s.rotation = vec3(rotation.x, rotation.y, rotation.z);
        s.position = vec3(position.x, position.y, position.z);

        // Read name
        uint32_t name_len;
        in.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        if (name_len > 1024) { // Arbitrary limit to prevent excessive allocation
            throw std::runtime_error("Invalid name length in file");
        }
        s.name.resize(name_len);
        in.read(s.name.data(), name_len);

        // Read arrays
        float3 color_values;
        float3 color_values0;
        in.read(reinterpret_cast<char*>(&color_values), sizeof(color_values));
        in.read(reinterpret_cast<char*>(&color_values0), sizeof(color_values0));
        s.color_values = color(color_values.x, color_values.y, color_values.z);
        s.color_values0 = color(color_values0.x, color_values0.y, color_values0.z);
        // Read scalars
        in.read(reinterpret_cast<char*>(&s.refraction_index), sizeof(s.refraction_index));
        in.read(reinterpret_cast<char*>(&s.color_picker_open), sizeof(s.color_picker_open));
        in.read(reinterpret_cast<char*>(&s.texture_scale), sizeof(s.texture_scale));

        // Read texture_file
        in.read(s.texture_file.data(), s.texture_file.size());
        // s.texture_file[sizeof(s.texture_file) - 1] = '\0'; // Ensure null-termination

        // Read remaining floats
        in.read(reinterpret_cast<char*>(&s.noise_scale), sizeof(s.noise_scale));
        in.read(reinterpret_cast<char*>(&s.fuzz), sizeof(s.fuzz));

        // Insert into states
        states[id] = s;
    }


    for(auto [id, st] : states){
        add_or_update_object(st, id, false);
    }


    if (!in.good() && !in.eof()) {
        throw std::runtime_error("Error occurred while reading file: " + filename);
    }
}