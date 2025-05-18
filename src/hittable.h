#ifndef HITTABLE_H
#define HITTABLE_H

#include "aabb.h"
#include "material.h"

class hittable {
public:
    virtual ~hittable() = default;

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;
    virtual void set_bounding_box() {};
    virtual void move_by(const point3& offset) {};
    virtual int get_id() const { return id; } 
    virtual void set_id(const int& id0) { id = id0;}
    virtual void set_name(const std::string& name0) { name = name0;}
    virtual std::string get_name() const { return name;}
    virtual void set_icon(const std::string& icon0) { icon = icon0;}
    virtual std::string get_icon() const { return icon;}
    virtual shared_ptr<material> get_material() const { return mat;}
    virtual void set_material(shared_ptr<material> mat0){ mat = mat0;}
    virtual aabb bounding_box() const { return bbox; }
    virtual std::ostream& print(std::ostream& out) const = 0;
    virtual std::istream& write(std::istream& in) const = 0;
protected:
    int id = -1; // Set by scene
    aabb bbox;
    std::string name = "Object";
    std::string icon = "\uf0a3";
    shared_ptr<material> mat;
};
std::ostream& operator<<(std::ostream& out, const hittable& h) {
    return h.print(out);
}

using std::make_shared;
using std::shared_ptr;

class hittable_list : public hittable {
  public:
    std::vector<shared_ptr<hittable>> objects;

    hittable_list() {}
    hittable_list(shared_ptr<hittable> object) { add(object); }

    void clear() { objects.clear(); }

    void add(shared_ptr<hittable> object) {
        objects.push_back(object);
        bbox = aabb(bbox, object->bounding_box());
    }
    void remove(shared_ptr<hittable> object) {
        int id = 0;
        for(auto o : objects){
            if(o->get_id() == object->get_id()){
                objects.erase(objects.begin()+id);
                break;
            }
            id++;
        }
        bbox = aabb(bbox, object->bounding_box());
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest_so_far = ray_t.max;

        for (const auto& object : objects) {
            if (object->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }
        return hit_anything;
    }

    void set_material(shared_ptr<material> mat)override{
        for(auto obj : objects)obj->set_material(mat);
    }

    std::ostream& print(std::ostream& out)  const override{
        for(int i = 0; i < objects.size(); i++){
            objects[i]->print(out);
            if(i+1 < objects.size())out << "\n";
        }
        return out;
    }

    std::istream& write(std::istream& in) const override{
        return in;
    }
};

class translate : public hittable {
  public:
    translate(shared_ptr<hittable> object, const vec3& offset)
      : object(object), offset(offset)
    {
        bbox = object->bounding_box() + offset;
    }
    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        // Move the ray backwards by the offset
        ray offset_r(r.origin() - offset, r.direction(), r.time());

        // Determine whether an intersection exists along the offset ray (and if so, where)
        if (!object->hit(offset_r, ray_t, rec))
            return false;

        // Move the intersection point forwards by the offset
        rec.p += offset;

        return true;
    }

    std::ostream& print(std::ostream& out) const override{
        return out;
    }
    std::istream& write(std::istream& in) const override{
        return in;
    }
  private:
    shared_ptr<hittable> object;
    vec3 offset;

};

class rotate_y : public hittable {
  public:
        rotate_y(shared_ptr<hittable> object, double angle) : object(object) {
        auto radians = degrees_to_radians(angle);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);
        bbox = object->bounding_box();

        point3 min( infinity,  infinity,  infinity);
        point3 max(-infinity, -infinity, -infinity);

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                for (int k = 0; k < 2; k++) {
                    auto x = i*bbox.x.max + (1-i)*bbox.x.min;
                    auto y = j*bbox.y.max + (1-j)*bbox.y.min;
                    auto z = k*bbox.z.max + (1-k)*bbox.z.min;

                    auto newx =  cos_theta*x + sin_theta*z;
                    auto newz = -sin_theta*x + cos_theta*z;

                    vec3 tester(newx, y, newz);

                    for (int c = 0; c < 3; c++) {
                        min[c] = std::fmin(min[c], tester[c]);
                        max[c] = std::fmax(max[c], tester[c]);
                    }
                }
            }
        }

        bbox = aabb(min, max);
    }


    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {

        // Transform the ray from world space to object space.

        auto origin = point3(
            (cos_theta * r.origin().x()) - (sin_theta * r.origin().z()),
            r.origin().y(),
            (sin_theta * r.origin().x()) + (cos_theta * r.origin().z())
        );

        auto direction = vec3(
            (cos_theta * r.direction().x()) - (sin_theta * r.direction().z()),
            r.direction().y(),
            (sin_theta * r.direction().x()) + (cos_theta * r.direction().z())
        );

        ray rotated_r(origin, direction, r.time());

        // Determine whether an intersection exists in object space (and if so, where).

        if (!object->hit(rotated_r, ray_t, rec))
            return false;

        // Transform the intersection from object space back to world space.

        rec.p = point3(
            (cos_theta * rec.p.x()) + (sin_theta * rec.p.z()),
            rec.p.y(),
            (-sin_theta * rec.p.x()) + (cos_theta * rec.p.z())
        );

        rec.normal = vec3(
            (cos_theta * rec.normal.x()) + (sin_theta * rec.normal.z()),
            rec.normal.y(),
            (-sin_theta * rec.normal.x()) + (cos_theta * rec.normal.z())
        );

        return true;
    }

    std::ostream& print(std::ostream& out)  const override{
        return out;
    }
    std::istream& write(std::istream& in) const override{
        return in;
    }


  private:
    shared_ptr<hittable> object;
    double sin_theta;
    double cos_theta;
};


class constant_medium : public hittable {
  public:
    constant_medium(shared_ptr<hittable> boundary, double density, shared_ptr<texture> tex)
      : boundary(boundary), neg_inv_density(-1/density),
        phase_function(make_shared<isotropic>(tex))
    {}

    constant_medium(shared_ptr<hittable> boundary, double density, const color& albedo)
      : boundary(boundary), neg_inv_density(-1/density),
        phase_function(make_shared<isotropic>(make_shared<solid_color>(albedo)))
    {}

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record rec1, rec2;

        if (!boundary->hit(r, interval::universe, rec1))
            return false;

        if (!boundary->hit(r, interval(rec1.t+0.0001, infinity), rec2))
            return false;

        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;

        if (rec1.t >= rec2.t)
            return false;

        if (rec1.t < 0)
            rec1.t = 0;

        auto ray_length = r.direction().length();
        auto distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        auto hit_distance = neg_inv_density * std::log(random_double());

        if (hit_distance > distance_inside_boundary)
            return false;

        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);

        rec.normal = vec3(1,0,0);  // arbitrary
        rec.front_face = true;     // also arbitrary
        rec.mat = phase_function;

        return true;
    }

    aabb bounding_box() const override { return boundary->bounding_box(); }


    std::istream& write(std::istream& in) const override{
        return in;
    }
    
  private:
    shared_ptr<hittable> boundary;
    double neg_inv_density;
    shared_ptr<material> phase_function;
};




#endif
