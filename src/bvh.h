#ifndef BVH_H
#define BVH_H

#include "aabb.h"
#include "hittable.h"
#include <algorithm>
#include <memory>
#include <vector>
#include <iostream>

class bvh_node : public hittable {
public:
    bvh_node(hittable_list list) : bvh_node(list.objects, 0, list.objects.size()) {
        // Constructor for initializing from a hittable list
    }

    bvh_node(std::vector<std::shared_ptr<hittable>>& objects, size_t start, size_t end) {
        // Initialize AABB to empty
        bbox = aabb::empty;
        for (size_t object_index = start; object_index < end; object_index++) {
            if (!objects[object_index]) {
                std::cerr << "Error: Null object at index " << object_index << "\n";
                continue;
            }
            bbox = aabb(bbox, objects[object_index]->bounding_box());
        }

        int axis = bbox.longest_axis();
        auto comparator = (axis == 0) ? box_x_compare
                        : (axis == 1) ? box_y_compare
                                      : box_z_compare;

        size_t object_span = end - start;

        if (object_span == 1) {
            left = right = objects[start];
        } else if (object_span == 2) {
            left = objects[start];
            right = objects[start + 1];
        } else {
            std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);
            auto mid = start + object_span / 2;
            left = std::make_shared<bvh_node>(objects, start, mid);
            right = std::make_shared<bvh_node>(objects, mid, end);
        }

        // Update AABB to enclose both children
        if (left && right)
            bbox = aabb(left->bounding_box(), right->bounding_box());
        else
            bbox = left ? left->bounding_box() : right ? right->bounding_box() : aabb::empty;
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        if (!bbox.hit(r, ray_t))
            return false;

        bool hit_left = left ? left->hit(r, ray_t, rec) : false;
        bool hit_right = right ? right->hit(r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec) : false;

        return hit_left || hit_right;
    }

    std::ostream& print(std::ostream& out) const override {
        return out;
    }

    std::istream& write(std::istream& in) const override {
        return in;
    }

    // Insert a new hittable object into the BVH
    void insert(std::shared_ptr<hittable> obj) {
        if (!obj) {
            std::cerr << "Error: Inserting null obj\n";
            return;
        }
        if (!left && !right) {
            // Leaf node: create a new internal node
            left = obj;
            right = nullptr;
            bbox = obj->bounding_box();
        } else if (!right) {
            // Single child: pair with new object
            right = obj;
            bbox = aabb(left->bounding_box(), right->bounding_box());
        } else {
            // Internal node: insert into the child that minimizes AABB growth
            float left_cost = compute_sah_cost(left->bounding_box(), obj->bounding_box());
            float right_cost = compute_sah_cost(right->bounding_box(), obj->bounding_box());

            if (left_cost < right_cost) {
                auto left_node = std::dynamic_pointer_cast<bvh_node>(left);
                if (left_node) {
                    left_node->insert(obj);
                    // Update AABB
                    bbox = aabb(left->bounding_box(), right ? right->bounding_box() : aabb::empty);
                } else {
                    // Replace leaf with new internal node
                    auto new_left = std::make_shared<bvh_node>();
                    new_left->left = left;
                    new_left->right = obj;
                    new_left->bbox = aabb(left->bounding_box(), obj->bounding_box());
                    left = new_left;
                    bbox = aabb(left->bounding_box(), right ? right->bounding_box() : aabb::empty);
                }
            } else {
                auto right_node = std::dynamic_pointer_cast<bvh_node>(right);
                if (right_node) {
                    right_node->insert(obj);
                    // Update AABB
                    bbox = aabb(left->bounding_box(), right->bounding_box());
                } else {
                    // Replace leaf with new internal node
                    auto new_right = std::make_shared<bvh_node>();
                    new_right->left = right;
                    new_right->right = obj;
                    new_right->bbox = aabb(right->bounding_box(), obj->bounding_box());
                    right = new_right;
                    bbox = aabb(left->bounding_box(), right->bounding_box());
                }
            }
        }
    }

    // Delete a hittable object from the BVH
    bool remove(const std::shared_ptr<hittable>& obj) {
        if (!obj) {
            std::cerr << "Error: Removing null obj\n";
            return false;
        }
        if (!left && !right) {
            return false; // Empty node
        }

        if (left == obj) {
            left = right;
            right = nullptr;
            bbox = left ? left->bounding_box() : aabb::empty;
            return true;
        } else if (right == obj) {
            right = nullptr;
            bbox = left ? left->bounding_box() : aabb::empty;
            return true;
        }

        bool removed = false;
        if (auto left_node = std::dynamic_pointer_cast<bvh_node>(left)) {
            removed = left_node->remove(obj);
            if (removed) {
                // Update AABB
                bbox = aabb(left->bounding_box(), right ? right->bounding_box() : aabb::empty);
                // If left becomes empty, collapse
                if (!left_node->left && !left_node->right) {
                    left = right;
                    right = nullptr;
                }
            }
        }
        if (!removed) {
            if (auto right_node = std::dynamic_pointer_cast<bvh_node>(right)) {
                removed = right_node->remove(obj);
                if (removed) {
                    // Update AABB
                    bbox = aabb(left->bounding_box(), right ? right->bounding_box() : aabb::empty);
                    // If right becomes empty, collapse
                    if (!right_node->left && !right_node->right) {
                        right = nullptr;
                    }
                }
            }
        }
        return removed;
    }

    // Update the position of a hittable object in the BVH
    bool update(const std::shared_ptr<hittable>& obj) {
        if (!obj) {
            std::cerr << "Error: Updating null obj\n";
            return false;
        }
        // Remove the object from the BVH
        bool removed = remove(obj);
        if (!removed) {
            std::cerr << "Error: Object not found in BVH\n";
            return false;
        }
        // Reinsert the object with its updated bounding box
        insert(obj);
        return true;
    }

    bvh_node() : left(nullptr), right(nullptr), bbox(aabb::empty) {}

private:
    std::shared_ptr<hittable> left;
    std::shared_ptr<hittable> right;
    aabb bbox;

    // Surface Area Heuristic (SAH) cost for inserting an object
    float compute_sah_cost(const aabb& node_box, const aabb& obj_box) const {
        aabb combined = aabb(node_box, obj_box);
        float surface_area = combined.x.size() * combined.y.size() +
                            combined.y.size() * combined.z.size() +
                            combined.z.size() * combined.x.size();
        return surface_area;
    }

    static bool box_compare(const std::shared_ptr<hittable> a, const std::shared_ptr<hittable> b, int axis_index) {
        auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
        auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
        return a_axis_interval.min < b_axis_interval.min;
    }

    static bool box_x_compare(const std::shared_ptr<hittable> a, const std::shared_ptr<hittable> b) {
        return box_compare(a, b, 0);
    }

    static bool box_y_compare(const std::shared_ptr<hittable> a, const std::shared_ptr<hittable> b) {
        return box_compare(a, b, 1);
    }

    static bool box_z_compare(const std::shared_ptr<hittable> a, const std::shared_ptr<hittable> b) {
        return box_compare(a, b, 2);
    }
};

#endif