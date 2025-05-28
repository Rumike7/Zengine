#ifndef SCENE_H
#define SCENE_H

#include "hittable.h"
#include "sphere.h"
#include "quad.h"
#include "objects.h"
#include "material.h"
#include "bvh.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <regex>
#include <stack>
#include <array>
#include <unordered_set>


// It is responsible the position of the buttons
enum class ObjectType {
    Sphere,
    Box,
    Cube,
    Triangle,
    Rectangle,
    Disk,
    Ellipse,
    Ring,
    Ellipsoid,
    Capsule,
    Polyhedron,
    Cylinder,
    Prism,
    Cone,

    //Debugging
    HollowCylinder,
    Hexagon,



    Count,
    //Further
    Torus,
    Tetrahedron,
    Octahedron,
    Frustum,
    Wedge,
    SphericalShell,
    RoundedBox,
    Plane,
    InfiniteCylinder,
    Paraboloid,
    Hyperboloid,
};

enum class TextureType {
    SolidColor,
    Checker,
    Image,
    Noise,
    Count
};

enum class MaterialType {
    Lambertian,
    Metal,
    Dielectric,
    DiffuseLight,
    Isotropic,
    Count
};

std::map<ObjectType, std::pair<std::string, std::string>> object_type_map = {
    {ObjectType::Sphere, {"Sphere", "\uE061"}},           // circle (filled)
    {ObjectType::Box, {"Box", "\uE14F"}},                // square_3d_stack
    {ObjectType::Cube, {"Cube", "\uE4C9"}},              // cube
    {ObjectType::Triangle, {"Triangle", "\uE899"}},      // play_arrow (triangle-like)
    {ObjectType::Rectangle, {"Rectangle", "\ue835"}},    // rectangle
    {ObjectType::Disk, {"Disk", "\uE1A7"}},              // disc_full
    {ObjectType::Ellipse, {"Ellipse", "\uE8B2"}},        // oval_horizontal
    {ObjectType::Ring, {"Ring", "\uE3D6"}},              // panorama_fish_eye (thin circle)
    {ObjectType::Cylinder, {"Cylinder", "\uE1B0"}},      // cylinder
    {ObjectType::Cone, {"Cone", "\uE8EF"}},              // traffic_cone
    {ObjectType::Torus, {"Torus", "\uE1A6"}},            // donut_large
    {ObjectType::Ellipsoid, {"Ellipsoid", "\uE8B3"}},    // oval_vertical
    {ObjectType::Capsule, {"Capsule", "\uE8E2"}},        // pill
    {ObjectType::HollowCylinder, {"Hollow Cylinder", "\uE1A5"}}, // donut_small
    {ObjectType::Hexagon, {"Hexagon", "\uE2B2"}},        // hexagon
    {ObjectType::Prism, {"Prism", "\uE4C9"}},            // cube (fallback, as no prism icon)
    {ObjectType::Polyhedron, {"Polyhedron", "\uE8B4"}},  // polyline (facet-like)
    {ObjectType::Frustum, {"Frustum", "\uE1B1"}},        // conical_flask
    {ObjectType::Wedge, {"Wedge", "\uE8E5"}},            // pie_chart (wedge-like)
    {ObjectType::Tetrahedron, {"Tetrahedron", "\uE8B1"}}, // triangle (3D-like)
    {ObjectType::Octahedron, {"Octahedron", "\uE3C8"}},  // octagon
    {ObjectType::Count, {"End Stop", "\uE8CC"}},         // stop
    {ObjectType::Plane, {"Plane", "\uE7BA"}},            // square
    {ObjectType::SphericalShell, {"SphericalShell", "\uE062"}}, // circle_outlined
    {ObjectType::RoundedBox, {"RoundedBox", "\uE7BB"}},  // rounded_square
    {ObjectType::Paraboloid, {"Paraboloid", "\uE8F8"}},  // waveform_path
    {ObjectType::Hyperboloid, {"Hyperboloid", "\uE2C3"}}, // hourglass_empty
    {ObjectType::InfiniteCylinder, {"InfiniteCylinder", "\uE1B0"}} // cylinder
};

constexpr std::array<const char*, 5> material_names = {
    "Lambertian", "Metal", "Dielectric", "Diffuse Light", "Isotropic"
};

constexpr std::array<const char*, 4> texture_types = {
    "Color", "Checker", "Image", "Noise"
};

struct state {
    ObjectType object_type;
    vec3 scale;
    vec3 rotation;
    point3 position;
    TextureType texture_type;
    MaterialType material_type;
    std::string name;
    color color_values;
    color color_values0;
    double refraction_index;
    bool color_picker_open;
    double texture_scale;
    std::string texture_file;
    float noise_scale;
    float fuzz;

    std::array<float, 100> data;

    // Type-specific field accessors
    float& radius()             { return data[0]; }
    const float& radius() const { return data[0]; }
    float* box_length()         { return &data[1]; }  
    const float* box_length() const { return &data[1]; }
    float& cube_size()          { return data[4]; }
    const float& cube_size() const { return data[4]; }
    float* u()                  { return &data[5]; } 
    const float* u() const      { return &data[5]; }
    float* v()                  { return &data[8]; } 
    const float* v() const      { return &data[8]; }
    float* axis()               { return &data[11]; }
    const float* axis() const   { return &data[11]; }
    float& height()             { return data[14]; }
    const float& height() const { return data[14]; }
    float& major_radius()       { return data[15]; }
    const float& major_radius() const { return data[15]; }
    float& minor_radius()       { return data[16]; }
    const float& minor_radius() const { return data[16]; }
    float* a()                  { return &data[17]; } 
    const float* a() const      { return &data[17]; }
    float* b()                  { return &data[20]; } 
    const float* b() const      { return &data[20]; }
    float* c()                  { return &data[23]; } 
    const float* c() const      { return &data[23]; }
    float& inner_radius()       { return data[26]; }
    const float& inner_radius() const { return data[26]; }
    float& outer_radius()       { return data[27]; }
    const float& outer_radius() const { return data[27]; }
    float& size()               { return data[28]; }
    const float& size() const   { return data[28]; }
    float& top_radius()         { return data[29]; }
    const float& top_radius() const { return data[29]; }
    float& bottom_radius()      { return data[30]; }
    const float& bottom_radius() const { return data[30]; }
    float* p2()                 { return &data[31]; }
    const float* p2() const     { return &data[31]; }
    float* p3()                 { return &data[34]; } 
    const float* p3() const     { return &data[34]; }
    float* p4()                 { return &data[37]; }
    float* normal()             { return &data[40]; } 
    const float* p4() const     { return &data[37]; }
    const float* normal() const { return &data[40]; }
    float& vertices_count()     { return data[43]; }
    const float& vertices_count() const { return data[43]; }

    // Helper functions for vertices (unchanged)
    void set_vertex(size_t index, const point3& vertex) {
        if (index >= 16) return; // Max 16 vertices
        size_t offset = 44 + index * 3;
        data[offset] = static_cast<float>(vertex.x);
        data[offset + 1] = static_cast<float>(vertex.y);
        data[offset + 2] = static_cast<float>(vertex.z);
        vertices_count() = std::max(vertices_count(), static_cast<float>(index + 1));
    }

    point3 get_vertex(size_t index) const {
        if (index >= static_cast<size_t>(vertices_count()) || index >= 16) {
            return point3(0, 0, 0);
        }
        size_t offset = 44 + index * 3;
        return point3(data[offset], data[offset + 1], data[offset + 2]);
    }

    std::vector<point3> get_vertices() const {
        std::vector<point3> vertices;
        size_t count = static_cast<size_t>(std::floor(vertices_count()));
        for (size_t i = 0; i < count && i < 16; ++i) {
            vertices.push_back(get_vertex(i));
        }
        return vertices;
    }

    void set_vertices(const std::vector<point3>& vertices) {
        size_t count = std::min(vertices.size(), size_t(16));
        vertices_count() = static_cast<float>(count);
        for (size_t i = 0; i < count; ++i) {
            set_vertex(i, vertices[i]);
        }
    }

    void reset() {
        object_type = ObjectType::Sphere;
        scale = vec3(1, 1, 1);
        rotation = vec3(0, 0, 0);
        position = {0.0f, 0.0f, 0.0f};
        texture_type = TextureType::SolidColor;
        material_type = MaterialType::Lambertian;
        name = "Sphere";
        color_values = {0.8f, 0.3f, 0.3f};
        color_values0 = {0.2f, 0.2f, 0.2f};
        refraction_index = 1.0;
        color_picker_open = false;
        texture_scale = 0.1;
        texture_file = "../assets/earthmap.jpg";
        noise_scale = 4.0f;
        fuzz = 0.1f;
        data.fill(0.0f);
        radius() = 0.4f;                     
        box_length()[0] = 0.5f;              
        box_length()[1] = 0.4f;              
        box_length()[2] = 0.6f;              
        cube_size() = 0.4f;                  
        u()[0] = 0.0f; u()[1] = 1.0f; u()[2] = 0.0f; 
        v()[0] = 0.0f; v()[1] = 0.0f; v()[2] = 1.0f; 
        axis()[0] = 0.0f; axis()[1] = 1.0f; axis()[2] = 0.0f; // Axis (y-axis)
        height() = 0.5f;                     // Cylinder/cone/prism height
        major_radius() = 0.6f;               // Torus major radius
        minor_radius() = 0.2f;               // Torus minor radius
        a()[0] = 0.4f; a()[1] = 0.0f; a()[2] = 0.0f; // Ellipsoid axis a
        b()[0] = 0.0f; b()[1] = 0.6f; b()[2] = 0.0f; // Ellipsoid axis b
        c()[0] = 0.0f; c()[1] = 0.0f; c()[2] = 0.8f; // Ellipsoid axis c
        inner_radius() = 0.2f;               // Hollow cylinder inner radius
        outer_radius() = 0.4f;               // Hollow cylinder outer radius
        size() = 0.5f;                       // Hexagon/octahedron size
        top_radius() = 0.3f;                 // Frustum top radius
        bottom_radius() = 0.5f;              // Frustum bottom radius
        p2()[0] = 0.5f; p2()[1] = 0.0f; p2()[2] = 0.0f; // Capsule/wedge/tetrahedron point 2
        p3()[0] = 0.0f; p3()[1] = 0.5f; p3()[2] = 0.0f; // Capsule/wedge/tetrahedron point 3
        p4()[0] = 0.0f; p4()[1] = 0.0f; p4()[2] = 0.5f; // Tetrahedron point 4
        normal()[0] = 1.0f; normal()[1] = 0.0f; normal()[2] = 0.0f; // Plane/hexagon normal
        vertices_count() = 0.0f;  
    }

    state() {
        reset();
    }
    //Copy and Assigment
    state(const state& other) = default;
    state& operator=(const state& other) = default;
    //Move
    state(state&&) = default;
    state& operator=(state&&) = default;
};

class scene {
private:
    // Command pattern for undo/redo
    class Command {
    public:
        virtual ~Command() = default;
        virtual void execute() = 0;
        virtual void undo() = 0;
    };

    class AddOrUpdateCommand : public Command {
    public:
        AddOrUpdateCommand(scene* s, std::shared_ptr<hittable> obj, int id, const state& st)
            : scene_(s), obj_(obj), id_(id), state_(st) {}
        void execute() override {
            if(scene_->object_map.find(id_) == scene_->object_map.end()){//Add
                scene_->object_map[id_] = std::vector<std::shared_ptr<hittable>>();
                assert(scene_->states.find(id_) == scene_->states.end());
            }else{//Update
                // scene_->bvh_world->remove(obj_);
            }
            scene_->object_map[id_].push_back(obj_);
            scene_->states[id_].push_back(state_);
            // scene_->bvh_world->insert(obj_);
            scene_->bvh_needs_rebuild = true;

        }
        void undo() override {
            auto& obj_vec = scene_->object_map[id_];
            auto& state_vec = scene_->states[id_];

            // scene_->bvh_world->remove(obj_);
            obj_vec.pop_back();
            state_vec.pop_back();

            if (!obj_vec.empty()) { // Update
                // scene_->bvh_world->insert(obj_vec.back());
            } else {// Add
                scene_->object_map.erase(id_);
                scene_->states.erase(id_);
            }

            scene_->bvh_needs_rebuild = true;
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
            : scene_(s), obj_(obj), id_(id) {}
        void execute() override {
            states_ = scene_->states[id_];
            objs_ = scene_->object_map[id_];
            assert(!states_.empty());
            scene_->object_map.erase(id_);
            scene_->states.erase(id_);
            // scene_->bvh_world->remove(obj_);
            scene_->bvh_needs_rebuild = true;
        }
        void undo() override {
            scene_->object_map[id_] = objs_;
            scene_->states[id_] = states_;
            // scene_->bvh_world->insert(objs_.back());
            scene_->bvh_needs_rebuild = true;
        }
    private:
        scene* scene_;
        std::shared_ptr<hittable> obj_;
        int id_;
        std::vector<state> states_;
        std::vector<std::shared_ptr<hittable>> objs_;
    };

    class MoveCommand : public Command {
    public:
        MoveCommand(scene* s, int id, const vec3& offset, const bool shouldComputeMove)
            : scene_(s), id_(id), offset_(offset), _shouldComputeMove(shouldComputeMove) {}
        void execute() override {
            assert(scene_->states.find(id_) != scene_->states.end());
            scene_->states[id_].back().position += offset_;
            if (_shouldComputeMove) {
                auto it = scene_->object_map.find(id_);
                if (it != scene_->object_map.end()){
                    it->second.back()->move_by(offset_);
                    // scene_->bvh_world->update(it->second);
                }
            }
        }
        void undo() override {
            auto& state_vec = scene_->states[id_];
            auto it = scene_->object_map.find(id_);
            if (it != scene_->object_map.end()) {
                // Revert position in state
                state_vec.back().position -= offset_;
                it->second.back()->move_by(-offset_);
                // scene_->bvh_world->update(it->second);
                scene_->bvh_needs_rebuild = true;
            }
        }
    private:
        scene* scene_;
        int id_;
        vec3 offset_;
        state state_;
        bool _shouldComputeMove;

    };

public:
    scene() {
        object_map.reserve(100);
        states.reserve(100);
        initialize();
        grid_visualization = std::make_shared<grid>();
    }

    // Select an object by casting a ray
    bool select_object(const ray& r, double max_t) {
        hit_record rec;
        double closest_t = max_t;
        int selected_id = -1;

        for (const auto& [id, objs] : object_map) {
            auto obj = objs.back();
            if (obj->hit(r, interval(0.001, closest_t), rec)) {
                closest_t = rec.t;
                selected_id = id;
                std::clog << "Hit " << (*obj) << "\n";
            }
        }

        selected_object_id = selected_id;
        return selected_id != -1;
    }

    point3 get_selected_position() const {
        if (selected_object_id == -1) {
            return point3(0, 0, 0);
        }
        auto it = object_map.find(selected_object_id);
        if (it != object_map.end()) {
            aabb bbox = it->second.back()->bounding_box();

            return (bbox.min() + bbox.max()) * 0.5;
        }
        return point3(0, 0, 0);
    }

    void move_selected(const point3& new_pos, int shouldMove = 0) {
        if (selected_object_id == -1) return;

        auto it = object_map.find(selected_object_id);
        if (it != object_map.end()) {
            aabb bbox = it->second.back()->bounding_box();
            point3 center = (bbox.min() + bbox.max()) * 0.5;
            vec3 offset = new_pos - center;
            state current_state = states[selected_object_id].back();
            current_state.position += offset;

            if (shouldMove == 1) { //During moving
                accumulated_offset += offset;
                it->second.back()->move_by(offset);
            } else if (shouldMove == 2) { // End moving
                execute_command(std::make_unique<MoveCommand>(this, selected_object_id, accumulated_offset, false));
                accumulated_offset = vec3(0, 0, 0);
            } else {// Move directly, using during function like update position
                execute_command(std::make_unique<MoveCommand>(this, selected_object_id, offset, true));
            }
        }
    }

    void add_or_update_object(const state& st, int id_object = -1) {
        if (id_object != -1) { // Update
            std::clog << "idobject " << id_object << "\n";
        }
        std::shared_ptr<texture> tex;
        switch (st.texture_type) {
            case TextureType::SolidColor:
                tex = std::make_shared<solid_color>(st.color_values);
                break;
            case TextureType::Checker:
                tex = std::make_shared<checker_texture>(st.texture_scale, st.color_values, st.color_values0);
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
        switch (st.material_type) {
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

        shared_ptr<hittable> obj = create_object(st);
        if(!obj) return;

        if(id_object == -1){//New(Add)
            id_object = next_id;
            next_id++;
        }

        execute_command(std::make_unique<AddOrUpdateCommand>(this, obj, id_object, st));

        if (object_type_map.find(st.object_type) != object_type_map.end()) {
            obj->set_icon(object_type_map.at(st.object_type).second.c_str());
        }
        obj->set_name(generate_unique_name(st.name));
        obj->set_id(id_object);
        obj->set_material(mat);
        apply_transformations(obj, st);


    }

    void delete_object(int id) {
        auto it = object_map.find(id);
        if (it != object_map.end()) {
            auto obj = it->second.back();
            execute_command(std::make_unique<DeleteCommand>(this, obj, id));
        }
    }

    int duplicate_object(int id) {
        auto it = object_map.find(id);
        if (it == object_map.end()) return -1;

        auto obj = it->second.back();
        state st = states[id].back();
        add_or_update_object(st);
        return next_id - 1;
    }

    const hittable& get_world() const {
        return *bvh_world;
    }

    const shared_ptr<hittable> get_world_ptr() const {
        return bvh_world;
    }

    // Initialize the scene with a default object
    void initialize() {
        bvh_world = make_shared<bvh_node>();
        state st;
        // add_or_update_object(st); 
    }

    std::unique_ptr<state> get_state(int id) {
        if (states.find(id) != states.end() && !states[id].empty()) {
            return std::make_unique<state>(states[id].back()); // returns a copy
        }
        return nullptr;
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
        std::unordered_map<int, std::shared_ptr<hittable>> maps;
        for(auto [id, objs] : object_map) maps[id] = objs.back();
        return maps;
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
        std::clog << "Undo " << undo_stack.size() << " " << redo_stack.size() << "\n";
    }

    void redo() {
        if (!redo_stack.empty()) {
            auto cmd = std::move(redo_stack.top());
            redo_stack.pop();
            cmd->execute();
            undo_stack.push(std::move(cmd));
        }
        std::clog << "Redo " << undo_stack.size() << " " << redo_stack.size() << "\n";
    }


    void load_new() {
        object_map.clear();
        states.clear();
        bvh_world = make_shared<bvh_node>();
        next_id = 0;
        undo_stack = std::stack<std::unique_ptr<Command>>();
        redo_stack = std::stack<std::unique_ptr<Command>>();
    }

    std::string getName() {
        return name;
    }
    void setName(std::string _name) {
        name = _name;
    }

    void rebuild_bvh() {
        if (!bvh_needs_rebuild) return;

        std::vector<std::shared_ptr<hittable>> objects;
        objects.reserve(object_map.size());
        for (const auto& [id, obj_vec] : object_map) {
            objects.push_back(obj_vec.back());
        }
        objects.insert(objects.end(), pending_objects.begin(), pending_objects.end());

        bvh_world = std::make_shared<bvh_node>(objects, 0, objects.size());
        pending_objects.clear();
        bvh_needs_rebuild = false;
    }

    void save_to_file(const std::string& filename) const;
    void load_from_file(const std::string& filename);

private:
    std::unordered_map<int, std::vector<std::shared_ptr<hittable>>> object_map;
    std::unordered_map<int, std::vector<state>> states;
    shared_ptr<bvh_node> bvh_world;
    std::shared_ptr<grid> grid_visualization;
    bool show_grid = true;
    int selected_object_id = -1;
    int next_id = 0;
    vec3 accumulated_offset;
    bool is_moving = false;
    vec3 move_start_pos;
    std::string name = "Untitled";
    std::vector<std::shared_ptr<hittable>> pending_objects;
    bool bvh_needs_rebuild = false;

    std::stack<std::unique_ptr<class Command>> undo_stack;
    std::stack<std::unique_ptr<class Command>> redo_stack;

    void apply_transformations(std::shared_ptr<hittable> obj, const state& st) {
        // if (st.scale != vec3(1, 1, 1)) {
        //     obj = std::make_shared<scale>(obj, st.scale);
        // }
        // // Apply rotation (assuming rotation is in degrees)
        // if (st.rotation != vec3(0, 0, 0)) {
        //     obj = std::make_shared<rotate_x>(obj, st.rotation.x);
        //     obj = std::make_shared<rotate_y>(obj, st.rotation.y);
        //     obj = std::make_shared<rotate_z>(obj, st.rotation.z);
        // }
        // obj->scale(st.scale);
        // obj->rotate(st.rotation);
    }

    std::shared_ptr<hittable> create_object(const state& st) {
        std::shared_ptr<hittable> obj;
        const float* u = st.u();
        const float* bl = st.box_length();
        const float* v = st.v();
        const float* a = st.a();
        const float* b = st.b();
        const float* c = st.c();
        const float* p2 = st.p2();
        const float* p3 = st.p3();
        const float* p4 = st.p4();
        const float* axis = st.axis();
        const float* normal = st.normal();
        const float cube_size = st.cube_size();


        switch (st.object_type) {
            case ObjectType::Sphere: {
                obj = std::make_shared<sphere>(st.position, st.radius());
                break;
            }
            case ObjectType::Box: {
                point3 box_min(st.position.x - bl[0] / 2, st.position.y - bl[1] / 2, st.position.z - bl[2] / 2);
                point3 box_max(st.position.x + bl[0] / 2, st.position.y + bl[1] / 2, st.position.z + bl[2] / 2);
                obj = std::make_shared<box>(box_min, box_max);
                break;
            }
            case ObjectType::Cube: {
                point3 a(st.position.x - cube_size, st.position.y - cube_size, st.position.z - cube_size);
                point3 b(st.position.x + cube_size, st.position.y + cube_size, st.position.z + cube_size);
                obj = std::make_shared<box>(a, b);
                break;
            }
            case ObjectType::Triangle: {
                obj = std::make_shared<triangle>(st.position, vec3(u[0], u[1], u[2]), vec3(v[0], v[1], v[2]));
                break;
            }
            case ObjectType::Rectangle: {
                obj = std::make_shared<rectangle>(st.position, vec3(u[0], u[1], u[2]), vec3(v[0], v[1], v[2]));
                break;
            }
            case ObjectType::Disk: {
                obj = std::make_shared<disk>(st.position, vec3(u[0], u[1], u[2]), vec3(v[0], v[1], v[2]), st.radius());
                break;
            }
            case ObjectType::Ellipse: {
                obj = std::make_shared<ellipse>(st.position, vec3(u[0], u[1], u[2]), vec3(v[0], v[1], v[2]));
                break;
            }
            case ObjectType::Ring: {
                obj = std::make_shared<ring>(st.position, vec3(u[0], u[1], u[2]), vec3(v[0], v[1], v[2]), st.inner_radius(), st.outer_radius());
                break;
            }
            case ObjectType::Cylinder: {
                obj = std::make_shared<cylinder>(st.position, vec3(axis[0], axis[1], axis[2]), st.radius(), st.height());
                break;
            }
            case ObjectType::Cone: {
                obj = std::make_shared<cone>(st.position, vec3(axis[0], axis[1], axis[2]), st.radius(), st.height());
                break;
            }
            case ObjectType::Torus: {
                obj = std::make_shared<torus>(st.position, st.major_radius(), st.minor_radius());
                break;
            }
            case ObjectType::Plane: {
                obj = std::make_shared<plane>(st.position, vec3(normal[0], normal[1], normal[2]));
                break;
            }
            case ObjectType::Ellipsoid: {
                obj = std::make_shared<ellipsoid>(st.position, vec3(a[0], a[1], a[2]), vec3(b[0], b[1], b[2]), vec3(c[0], c[1], c[2]));
                break;
            }
            case ObjectType::Capsule: {
                obj = std::make_shared<capsule>(st.position, point3(p2[0], p2[1], p2[2]), st.radius());
                break;
            }
            case ObjectType::HollowCylinder: {
                obj = std::make_shared<hollow_cylinder>(st.position, vec3(axis[0], axis[1], axis[2]), st.inner_radius(), st.outer_radius(), st.height());
                break;
            }
            case ObjectType::Hexagon: {
                obj = std::make_shared<hexagon>(st.position, vec3(normal[0], normal[1], normal[2]), st.size());
                break;
            }
            case ObjectType::Prism: {
                obj = std::make_shared<prism>(st.position, vec3(axis[0], axis[1], axis[2]), st.get_vertices(), st.height());
                break;
            }
            case ObjectType::Polyhedron: {
                std::vector<std::vector<int>> faces = {{0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}};
                obj = std::make_shared<polyhedron>(st.get_vertices(), faces);
                break;
            }
            case ObjectType::Frustum: {
                obj = std::make_shared<frustum>(st.position, vec3(axis[0], axis[1], axis[2]), st.bottom_radius(), st.top_radius(), st.height());
                break;
            }
            case ObjectType::Wedge: {
                obj = std::make_shared<wedge>(st.position, point3(p2[0], p2[1], p2[2]), point3(p3[0], p3[1], p3[2]), st.height());
                break;
            }
            case ObjectType::Tetrahedron: {
                obj = std::make_shared<tetrahedron>(st.position, point3(p2[0], p2[1], p2[2]), point3(p3[0], p3[1], p3[2]), point3(p4[0], p4[1], p4[2]));
                break;
            }
            case ObjectType::Octahedron: {
                obj = std::make_shared<octahedron>(st.position, st.size());
                break;
            }
        }
        return obj;
    }    

    std::string generate_unique_name(const std::string& base_name) const {
        std::string name = base_name.empty() ? "object" : base_name;
        std::unordered_set<std::string> existing_names;
        for (const auto& [id, obj_vec] : object_map) {
            existing_names.insert(obj_vec.back()->get_name());
        }

        if (existing_names.find(name) == existing_names.end()) {
            return name;
        }

        int counter = 1;
        std::string candidate;
        do {
            candidate = name + "_" + std::to_string(counter++);
        } while (existing_names.find(candidate) != existing_names.end());
        return candidate;
    }

    void execute_command(std::unique_ptr<Command> cmd) {
        cmd->execute();
        undo_stack.push(std::move(cmd));
        redo_stack = std::stack<std::unique_ptr<Command>>();
    }
};


void scene::save_to_file(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Header
    const char magic[] = "ZSC";
    uint32_t version = 3; // Updated version for new data array format
    uint32_t state_map_size = static_cast<uint32_t>(states.size());
    uint32_t n_id = static_cast<uint32_t>(next_id);
    uint32_t sh_grid = static_cast<uint32_t>(show_grid);

    out.write(magic, 3);
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&state_map_size), sizeof(state_map_size));
    out.write(reinterpret_cast<const char*>(&n_id), sizeof(n_id));
    out.write(reinterpret_cast<const char*>(&sh_grid), sizeof(sh_grid));

    uint32_t name_sc_len = static_cast<uint32_t>(name.size());
    out.write(reinterpret_cast<const char*>(&name_sc_len), sizeof(name_sc_len));
    out.write(name.data(), name_sc_len);

    // Serialize each state's vector
    for (const auto& [id, state_vec] : states) {
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        state s = state_vec.back();

        uint32_t obj_type = static_cast<uint32_t>(s.object_type);
        uint32_t tex_type = static_cast<uint32_t>(s.texture_type);
        uint32_t mat_type = static_cast<uint32_t>(s.material_type);
        out.write(reinterpret_cast<const char*>(&obj_type), sizeof(obj_type));
        out.write(reinterpret_cast<const char*>(&tex_type), sizeof(tex_type));
        out.write(reinterpret_cast<const char*>(&mat_type), sizeof(mat_type));

        float3 scale = to_float3(s.scale);
        float3 rotation = to_float3(s.rotation);
        float3 position = to_float3(s.position);
        out.write(reinterpret_cast<const char*>(&scale), sizeof(scale));
        out.write(reinterpret_cast<const char*>(&rotation), sizeof(rotation));
        out.write(reinterpret_cast<const char*>(&position), sizeof(position));

        uint32_t name_len = static_cast<uint32_t>(s.name.size());
        out.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        out.write(s.name.data(), name_len);

        float3 color_values = to_float3(s.color_values);
        float3 color_values0 = to_float3(s.color_values0);
        out.write(reinterpret_cast<const char*>(&color_values), sizeof(color_values));
        out.write(reinterpret_cast<const char*>(&color_values0), sizeof(color_values0));

        out.write(reinterpret_cast<const char*>(&s.refraction_index), sizeof(s.refraction_index));
        out.write(reinterpret_cast<const char*>(&s.color_picker_open), sizeof(s.color_picker_open));
        out.write(reinterpret_cast<const char*>(&s.texture_scale), sizeof(s.texture_scale));

        uint32_t texture_file_len = static_cast<uint32_t>(s.texture_file.size());
        out.write(reinterpret_cast<const char*>(&texture_file_len), sizeof(texture_file_len));
        out.write(s.texture_file.data(), texture_file_len);

        out.write(reinterpret_cast<const char*>(&s.noise_scale), sizeof(s.noise_scale));
        out.write(reinterpret_cast<const char*>(&s.fuzz), sizeof(s.fuzz));

        out.write(reinterpret_cast<const char*>(s.data.data()), sizeof(float) * s.data.size());
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
    uint32_t state_map_size;
    uint32_t n_id;
    uint32_t sh_grid;

    in.read(magic, 3);
    if (magic[0] != 'Z' || magic[1] != 'S' || magic[2] != 'C') {
        throw std::runtime_error("Invalid .zsc file: incorrect magic number");
    }

    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 3) {
        throw std::runtime_error("Unsupported .zsc file version: " + std::to_string(version));
    }
    in.read(reinterpret_cast<char*>(&state_map_size), sizeof(state_map_size));
    in.read(reinterpret_cast<char*>(&n_id), sizeof(n_id));
    in.read(reinterpret_cast<char*>(&sh_grid), sizeof(sh_grid));

    next_id = static_cast<int>(n_id);
    show_grid = static_cast<bool>(sh_grid);

    uint32_t name_sc_len;
    in.read(reinterpret_cast<char*>(&name_sc_len), sizeof(name_sc_len));
    if (name_sc_len > 1024) {
        throw std::runtime_error("Invalid name length in file");
    }
    name.resize(name_sc_len);
    in.read(name.data(), name_sc_len);

    // Read each state's vector
    for (uint32_t i = 0; i < state_map_size; ++i) {
        int id;
        in.read(reinterpret_cast<char*>(&id), sizeof(id));
        state s;
        uint32_t obj_type, tex_type, mat_type;
        in.read(reinterpret_cast<char*>(&obj_type), sizeof(obj_type));
        in.read(reinterpret_cast<char*>(&tex_type), sizeof(tex_type));
        in.read(reinterpret_cast<char*>(&mat_type), sizeof(mat_type));

        if (obj_type >= static_cast<uint32_t>(ObjectType::Count) ||
            tex_type >= static_cast<uint32_t>(TextureType::Count) ||
            mat_type >= static_cast<uint32_t>(MaterialType::Count)) {
            throw std::runtime_error("Invalid enum value in file");
        }

        s.object_type = static_cast<ObjectType>(obj_type);
        s.texture_type = static_cast<TextureType>(tex_type);
        s.material_type = static_cast<MaterialType>(mat_type);

        float3 scale, rotation, position;
        in.read(reinterpret_cast<char*>(&scale), sizeof(scale));
        in.read(reinterpret_cast<char*>(&rotation), sizeof(rotation));
        in.read(reinterpret_cast<char*>(&position), sizeof(position));
        s.scale = vec3(scale.x, scale.y, scale.z);
        s.rotation = vec3(rotation.x, rotation.y, rotation.z);
        s.position = point3(position.x, position.y, position.z);

        uint32_t name_len;
        in.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        if (name_len > 1024) {
            throw std::runtime_error("Invalid name length in file");
        }
        s.name.resize(name_len);
        in.read(s.name.data(), name_len);

        float3 color_values, color_values0;
        in.read(reinterpret_cast<char*>(&color_values), sizeof(color_values));
        in.read(reinterpret_cast<char*>(&color_values0), sizeof(color_values0));
        s.color_values = color(color_values.x, color_values.y, color_values.z);
        s.color_values0 = color(color_values0.x, color_values0.y, color_values0.z);

        in.read(reinterpret_cast<char*>(&s.refraction_index), sizeof(s.refraction_index));
        in.read(reinterpret_cast<char*>(&s.color_picker_open), sizeof(s.color_picker_open));
        in.read(reinterpret_cast<char*>(&s.texture_scale), sizeof(s.texture_scale));

        uint32_t texture_file_len;
        in.read(reinterpret_cast<char*>(&texture_file_len), sizeof(texture_file_len));
        if (texture_file_len > 1024) {
            throw std::runtime_error("Invalid texture file length in file");
        }
        s.texture_file.resize(texture_file_len);
        in.read(s.texture_file.data(), texture_file_len);

        in.read(reinterpret_cast<char*>(&s.noise_scale), sizeof(s.noise_scale));
        in.read(reinterpret_cast<char*>(&s.fuzz), sizeof(s.fuzz));

        // Read entire data array
        in.read(reinterpret_cast<char*>(s.data.data()), sizeof(float) * s.data.size());

        states[id] = std::vector<state>{s};
        add_or_update_object(s, id);
    }
    undo_stack = {};
    redo_stack = {};

    if (!in.good() && !in.eof()) {
        throw std::runtime_error("Error occurred while reading file: " + filename);
    }
}

#endif