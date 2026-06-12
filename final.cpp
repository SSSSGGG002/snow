#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB

#include <GLFW/glfw3.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Shader.h"

namespace {
constexpr int kWindowWidth = 1100;
constexpr int kWindowHeight = 760;
constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    float m[16];

    Mat4() {
        for (float& value : m) {
            value = 0.0f;
        }
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    float u = 0.0f;
    float v = 0.0f;
};

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertex_count = 0;
};

struct Material {
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;
    Vec3 emission;
    GLuint texture = 0;
    float shininess;
    float alpha;
    bool use_texture = false;
};

struct ModelPart {
    Mesh mesh;
    Material material;
    std::string name;
};

struct ObjModel {
    std::vector<ModelPart> parts;
    bool loaded = false;
};

struct SnowParticle {
    Vec3 position;
    float scale;
    float angle;
    float fall_speed;
    float sway;
    float spin_speed;
};

std::unique_ptr<Shader> scene_shader;
std::unique_ptr<Shader> skybox_shader;
Mesh cube_mesh;
Mesh sphere_mesh;
Mesh cone_mesh;
Mesh cylinder_mesh;
Mesh pyramid_mesh;
Mesh snowflake_mesh;
Mesh terrain_mesh;
std::vector<SnowParticle> snow_particles;
GLuint winter_background_texture = 0;
GLuint rose_texture = 0;
GLuint gold_texture = 0;
GLuint snow_texture = 0;
GLuint wood_texture = 0;
constexpr const char* kKenneyHolidayObjDir = "资源/online/kenney_holiday-kit/extracted/Models/OBJ format";
ObjModel holiday_snowman_model;
ObjModel holiday_tree_snow_a_model;
ObjModel holiday_tree_snow_b_model;
ObjModel holiday_tree_snow_c_model;
ObjModel holiday_cabin_wall_model;
ObjModel holiday_cabin_wall_roof_model;
ObjModel holiday_cabin_wall_wreath_model;
ObjModel holiday_cabin_window_model;
ObjModel holiday_cabin_roof_snow_model;
ObjModel holiday_cabin_roof_snow_chimney_model;
ObjModel holiday_cabin_roof_snow_point_model;
ObjModel holiday_cabin_corner_logs_model;
ObjModel holiday_cabin_overhang_doorway_model;
ObjModel holiday_present_a_cube_model;
ObjModel holiday_present_b_cube_model;
ObjModel holiday_present_a_rectangle_model;
ObjModel holiday_present_b_rectangle_model;
ObjModel holiday_present_a_round_model;
ObjModel holiday_present_b_round_model;

float camera_yaw = 0.0f;
float camera_pitch = 18.0f;
float camera_distance = 10.5f;
bool night_mode = true;
bool snowfall_enabled = true;
bool dragging_stump = false;
bool rotating_stump = false;
double last_mouse_x = 0.0;
double last_mouse_y = 0.0;
Vec3 stump_offset = {0.0f, 0.0f, 0.0f};
Vec3 stump_scale = {1.0f, 1.0f, 1.0f};
float stump_rotation = 0.0f;

Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalize(const Vec3& value) {
    const float length = std::sqrt(dot(value, value));
    if (length < 0.000001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

Vec3 mix_vec3(const Vec3& a, const Vec3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

float radians(float degrees) {
    return degrees * kPi / 180.0f;
}

float clampf(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 result;
    for (float& value : result.m) {
        value = 0.0f;
    }
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                result.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
        }
    }
    return result;
}

Mat4 translate(const Vec3& offset) {
    Mat4 result;
    result.m[12] = offset.x;
    result.m[13] = offset.y;
    result.m[14] = offset.z;
    return result;
}

Mat4 scale(const Vec3& factor) {
    Mat4 result;
    result.m[0] = factor.x;
    result.m[5] = factor.y;
    result.m[10] = factor.z;
    return result;
}

Mat4 rotate(float angle_degrees, const Vec3& axis) {
    const Vec3 a = normalize(axis);
    const float angle = radians(angle_degrees);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0f - c;

    Mat4 result;
    result.m[0] = t * a.x * a.x + c;
    result.m[1] = t * a.x * a.y + s * a.z;
    result.m[2] = t * a.x * a.z - s * a.y;
    result.m[4] = t * a.x * a.y - s * a.z;
    result.m[5] = t * a.y * a.y + c;
    result.m[6] = t * a.y * a.z + s * a.x;
    result.m[8] = t * a.x * a.z + s * a.y;
    result.m[9] = t * a.y * a.z - s * a.x;
    result.m[10] = t * a.z * a.z + c;
    return result;
}

Mat4 perspective(float fov_degrees, float aspect, float near_plane, float far_plane) {
    Mat4 result;
    for (float& value : result.m) {
        value = 0.0f;
    }
    const float tan_half_fov = std::tan(radians(fov_degrees) * 0.5f);
    result.m[0] = 1.0f / (aspect * tan_half_fov);
    result.m[5] = 1.0f / tan_half_fov;
    result.m[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
    return result;
}

Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 result;
    result.m[0] = s.x;
    result.m[4] = s.y;
    result.m[8] = s.z;
    result.m[1] = u.x;
    result.m[5] = u.y;
    result.m[9] = u.z;
    result.m[2] = -f.x;
    result.m[6] = -f.y;
    result.m[10] = -f.z;
    result.m[12] = -dot(s, eye);
    result.m[13] = -dot(u, eye);
    result.m[14] = dot(f, eye);
    return result;
}

void set_mat4(const char* name, const Mat4& matrix) {
    glUniformMatrix4fv(glGetUniformLocation(scene_shader->id(), name), 1, GL_FALSE, matrix.m);
}

void set_vec3(const char* name, const Vec3& value) {
    glUniform3f(glGetUniformLocation(scene_shader->id(), name), value.x, value.y, value.z);
}

void set_float(const char* name, float value) {
    glUniform1f(glGetUniformLocation(scene_shader->id(), name), value);
}

void set_material(const Material& material) {
    set_vec3("material.ambient", material.ambient);
    set_vec3("material.diffuse", material.diffuse);
    set_vec3("material.specular", material.specular);
    set_vec3("material.emission", material.emission);
    glUniform1i(glGetUniformLocation(scene_shader->id(), "material.useTexture"), material.use_texture ? GL_TRUE : GL_FALSE);
    if (material.use_texture && material.texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.texture);
        glUniform1i(glGetUniformLocation(scene_shader->id(), "material.diffuseTexture"), 0);
    }
    set_float("material.shininess", material.shininess);
    set_float("material.alpha", material.alpha);
}

void add_triangle(std::vector<Vertex>& vertices, Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    vertices.push_back({a, normal});
    vertices.push_back({b, normal});
    vertices.push_back({c, normal});
}

Mesh create_mesh(const std::vector<Vertex>& vertices) {
    Mesh mesh;
    mesh.vertex_count = static_cast<GLsizei>(vertices.size());
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(sizeof(Vec3)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(sizeof(Vec3) * 2));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return mesh;
}

bool load_image_rgba(const std::string& path, std::vector<unsigned char>& pixels, int& width, int& height, bool flip_y) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(path.c_str()),
        static_cast<CFIndex>(path.size()),
        false);
    if (url == nullptr) {
        std::cout << "Failed to create image URL: " << path << std::endl;
        return false;
    }

    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (source == nullptr) {
        std::cout << "Failed to load image: " << path << std::endl;
        return false;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (image == nullptr) {
        std::cout << "Failed to decode image: " << path << std::endl;
        return false;
    }

    width = static_cast<int>(CGImageGetWidth(image));
    height = static_cast<int>(CGImageGetHeight(image));
    pixels.assign(static_cast<size_t>(width * height * 4), 0);

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        8,
        static_cast<size_t>(width * 4),
        color_space,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(color_space);

    if (context == nullptr) {
        CGImageRelease(image);
        std::cout << "Failed to create bitmap context: " << path << std::endl;
        return false;
    }

    if (flip_y) {
        CGContextTranslateCTM(context, 0.0, static_cast<CGFloat>(height));
        CGContextScaleCTM(context, 1.0, -1.0);
    }
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)), image);
    CGContextRelease(context);
    CGImageRelease(image);
    return true;
}

GLuint create_texture_2d_from_file(const std::string& path) {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    if (!load_image_rgba(path, pixels, width, height, true)) {
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void init_resource_textures() {
    rose_texture = create_texture_2d_from_file("资源/boxF.png");
    gold_texture = create_texture_2d_from_file("资源/specularGold.png");
    snow_texture = create_texture_2d_from_file("资源/online/Snow002_1K-JPG/Snow002_1K-JPG_Color.jpg");
    wood_texture = create_texture_2d_from_file("资源/online/Wood025_1K-JPG/Wood025_1K-JPG_Color.jpg");
    winter_background_texture = create_texture_2d_from_file("资源/online/snow_field/snow_field_2k.jpg");
    if (snow_texture != 0) {
        glBindTexture(GL_TEXTURE_2D, snow_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (wood_texture != 0) {
        glBindTexture(GL_TEXTURE_2D, wood_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (winter_background_texture != 0) {
        glBindTexture(GL_TEXTURE_2D, winter_background_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

std::vector<Vertex> build_cube_vertices() {
    const std::array<Vec3, 8> p = {
        Vec3{-0.5f, -0.5f, -0.5f}, Vec3{0.5f, -0.5f, -0.5f},
        Vec3{0.5f, 0.5f, -0.5f},   Vec3{-0.5f, 0.5f, -0.5f},
        Vec3{-0.5f, -0.5f, 0.5f},  Vec3{0.5f, -0.5f, 0.5f},
        Vec3{0.5f, 0.5f, 0.5f},    Vec3{-0.5f, 0.5f, 0.5f}
    };
    std::vector<Vertex> vertices;
    auto add_quad = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal) {
        vertices.push_back({a, normal, 0.0f, 0.0f});
        vertices.push_back({b, normal, 1.0f, 0.0f});
        vertices.push_back({c, normal, 1.0f, 1.0f});
        vertices.push_back({a, normal, 0.0f, 0.0f});
        vertices.push_back({c, normal, 1.0f, 1.0f});
        vertices.push_back({d, normal, 0.0f, 1.0f});
    };
    add_quad(p[0], p[1], p[2], p[3], {0.0f, 0.0f, -1.0f});
    add_quad(p[5], p[4], p[7], p[6], {0.0f, 0.0f, 1.0f});
    add_quad(p[4], p[0], p[3], p[7], {-1.0f, 0.0f, 0.0f});
    add_quad(p[1], p[5], p[6], p[2], {1.0f, 0.0f, 0.0f});
    add_quad(p[3], p[2], p[6], p[7], {0.0f, 1.0f, 0.0f});
    add_quad(p[4], p[5], p[1], p[0], {0.0f, -1.0f, 0.0f});
    return vertices;
}

std::vector<Vertex> build_sphere_vertices(int sectors, int stacks) {
    std::vector<Vertex> vertices;
    for (int stack = 0; stack < stacks; ++stack) {
        const float phi0 = -kPi * 0.5f + kPi * static_cast<float>(stack) / stacks;
        const float phi1 = -kPi * 0.5f + kPi * static_cast<float>(stack + 1) / stacks;
        for (int sector = 0; sector < sectors; ++sector) {
            const float theta0 = 2.0f * kPi * static_cast<float>(sector) / sectors;
            const float theta1 = 2.0f * kPi * static_cast<float>(sector + 1) / sectors;
            auto point = [](float phi, float theta) {
                return Vec3{
                    std::cos(phi) * std::cos(theta) * 0.5f,
                    std::sin(phi) * 0.5f,
                    std::cos(phi) * std::sin(theta) * 0.5f
                };
            };
            const Vec3 a = point(phi0, theta0);
            const Vec3 b = point(phi0, theta1);
            const Vec3 c = point(phi1, theta1);
            const Vec3 d = point(phi1, theta0);
            const float u0 = static_cast<float>(sector) / sectors;
            const float u1 = static_cast<float>(sector + 1) / sectors;
            const float v0 = static_cast<float>(stack) / stacks;
            const float v1 = static_cast<float>(stack + 1) / stacks;
            vertices.push_back({a, normalize(a), u0, v0});
            vertices.push_back({b, normalize(b), u1, v0});
            vertices.push_back({c, normalize(c), u1, v1});
            vertices.push_back({a, normalize(a), u0, v0});
            vertices.push_back({c, normalize(c), u1, v1});
            vertices.push_back({d, normalize(d), u0, v1});
        }
    }
    return vertices;
}

std::vector<Vertex> build_cylinder_vertices(int sectors) {
    std::vector<Vertex> vertices;
    for (int i = 0; i < sectors; ++i) {
        const float t0 = 2.0f * kPi * static_cast<float>(i) / sectors;
        const float t1 = 2.0f * kPi * static_cast<float>(i + 1) / sectors;
        const float u0 = static_cast<float>(i) / sectors;
        const float u1 = static_cast<float>(i + 1) / sectors;
        const Vec3 b0 = {0.5f * std::cos(t0), -0.5f, 0.5f * std::sin(t0)};
        const Vec3 b1 = {0.5f * std::cos(t1), -0.5f, 0.5f * std::sin(t1)};
        const Vec3 t0p = {b0.x, 0.5f, b0.z};
        const Vec3 t1p = {b1.x, 0.5f, b1.z};
        const Vec3 n0 = normalize({std::cos(t0), 0.0f, std::sin(t0)});
        const Vec3 n1 = normalize({std::cos(t1), 0.0f, std::sin(t1)});
        vertices.push_back({b0, n0, u0, 0.0f});
        vertices.push_back({b1, n1, u1, 0.0f});
        vertices.push_back({t1p, n1, u1, 1.0f});
        vertices.push_back({b0, n0, u0, 0.0f});
        vertices.push_back({t1p, n1, u1, 1.0f});
        vertices.push_back({t0p, n0, u0, 1.0f});

        vertices.push_back({{0.0f, -0.5f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.5f, 0.5f});
        vertices.push_back({b0, {0.0f, -1.0f, 0.0f}, 0.5f + b0.x, 0.5f + b0.z});
        vertices.push_back({b1, {0.0f, -1.0f, 0.0f}, 0.5f + b1.x, 0.5f + b1.z});

        vertices.push_back({{0.0f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, 0.5f, 0.5f});
        vertices.push_back({t1p, {0.0f, 1.0f, 0.0f}, 0.5f + t1p.x, 0.5f + t1p.z});
        vertices.push_back({t0p, {0.0f, 1.0f, 0.0f}, 0.5f + t0p.x, 0.5f + t0p.z});
    }
    return vertices;
}

std::vector<Vertex> build_cone_vertices(int sectors) {
    std::vector<Vertex> vertices;
    const Vec3 apex = {0.0f, 0.5f, 0.0f};
    for (int i = 0; i < sectors; ++i) {
        const float t0 = 2.0f * kPi * static_cast<float>(i) / sectors;
        const float t1 = 2.0f * kPi * static_cast<float>(i + 1) / sectors;
        const Vec3 b0 = {0.5f * std::cos(t0), -0.5f, 0.5f * std::sin(t0)};
        const Vec3 b1 = {0.5f * std::cos(t1), -0.5f, 0.5f * std::sin(t1)};
        const Vec3 normal = normalize(cross(b1 - b0, apex - b0));
        add_triangle(vertices, b0, b1, apex, normal);
        add_triangle(vertices, {0.0f, -0.5f, 0.0f}, b1, b0, {0.0f, -1.0f, 0.0f});
    }
    return vertices;
}

std::vector<Vertex> build_pyramid_vertices() {
    std::vector<Vertex> vertices;
    const Vec3 a = {-0.5f, -0.5f, -0.5f};
    const Vec3 b = {0.5f, -0.5f, -0.5f};
    const Vec3 c = {0.5f, -0.5f, 0.5f};
    const Vec3 d = {-0.5f, -0.5f, 0.5f};
    const Vec3 top = {0.0f, 0.5f, 0.0f};
    add_triangle(vertices, a, b, top, normalize(cross(b - a, top - a)));
    add_triangle(vertices, b, c, top, normalize(cross(c - b, top - b)));
    add_triangle(vertices, c, d, top, normalize(cross(d - c, top - c)));
    add_triangle(vertices, d, a, top, normalize(cross(a - d, top - d)));
    add_triangle(vertices, a, d, c, {0.0f, -1.0f, 0.0f});
    add_triangle(vertices, a, c, b, {0.0f, -1.0f, 0.0f});
    return vertices;
}

std::vector<Vertex> build_snowflake_vertices() {
    std::vector<Vertex> vertices;
    const Vec3 n = {0.0f, 0.0f, 1.0f};
    for (int arm = 0; arm < 6; ++arm) {
        const float angle = static_cast<float>(arm) * kPi / 3.0f;
        const Vec3 center = {0.0f, 0.0f, 0.0f};
        const Vec3 tip = {std::cos(angle) * 0.52f, std::sin(angle) * 0.52f, 0.0f};
        const Vec3 left = {std::cos(angle + 0.12f) * 0.12f, std::sin(angle + 0.12f) * 0.12f, 0.0f};
        const Vec3 right = {std::cos(angle - 0.12f) * 0.12f, std::sin(angle - 0.12f) * 0.12f, 0.0f};
        add_triangle(vertices, center, right, tip, n);
        add_triangle(vertices, center, tip, left, n);
        const Vec3 branch_a = {std::cos(angle + 0.55f) * 0.28f, std::sin(angle + 0.55f) * 0.28f, 0.0f};
        const Vec3 branch_b = {std::cos(angle - 0.55f) * 0.28f, std::sin(angle - 0.55f) * 0.28f, 0.0f};
        const Vec3 mid = {std::cos(angle) * 0.27f, std::sin(angle) * 0.27f, 0.0f};
        add_triangle(vertices, center, branch_a, mid, n);
        add_triangle(vertices, center, mid, branch_b, n);
    }
    return vertices;
}

float terrain_height(float x, float z) {
    const float ripple =
        0.045f * std::sin(x * 0.85f + z * 0.34f)
        + 0.035f * std::cos(z * 0.72f - x * 0.18f)
        + 0.020f * std::sin((x + z) * 1.55f);
    const float drift =
        0.11f * std::exp(-((x + 5.5f) * (x + 5.5f) + (z - 0.9f) * (z - 0.9f)) * 0.16f)
        + 0.09f * std::exp(-((x - 4.8f) * (x - 4.8f) + (z + 2.8f) * (z + 2.8f)) * 0.18f)
        + 0.06f * std::exp(-(x * x + (z - 3.0f) * (z - 3.0f)) * 0.12f);
    return -0.55f + ripple + drift;
}

Vec3 terrain_normal(float x, float z) {
    const float step = 0.08f;
    const float dx = terrain_height(x + step, z) - terrain_height(x - step, z);
    const float dz = terrain_height(x, z + step) - terrain_height(x, z - step);
    return normalize({-dx / (2.0f * step), 1.0f, -dz / (2.0f * step)});
}

std::vector<Vertex> build_terrain_vertices() {
    constexpr int kGrid = 72;
    constexpr float kWidth = 42.0f;
    constexpr float kDepth = 34.0f;
    constexpr float kCenterZ = -2.4f;
    std::vector<Vertex> vertices;
    vertices.reserve(kGrid * kGrid * 6);

    auto point = [](int ix, int iz) {
        const float x = -kWidth * 0.5f + kWidth * static_cast<float>(ix) / kGrid;
        const float z = kCenterZ - kDepth * 0.5f + kDepth * static_cast<float>(iz) / kGrid;
        return Vec3{x, terrain_height(x, z), z};
    };

    for (int z = 0; z < kGrid; ++z) {
        for (int x = 0; x < kGrid; ++x) {
            const Vec3 a = point(x, z);
            const Vec3 b = point(x + 1, z);
            const Vec3 c = point(x + 1, z + 1);
            const Vec3 d = point(x, z + 1);
            const float ua = (a.x + kWidth * 0.5f) / 2.2f;
            const float va = (a.z - (kCenterZ - kDepth * 0.5f)) / 2.2f;
            const float ub = (b.x + kWidth * 0.5f) / 2.2f;
            const float vb = (b.z - (kCenterZ - kDepth * 0.5f)) / 2.2f;
            const float uc = (c.x + kWidth * 0.5f) / 2.2f;
            const float vc = (c.z - (kCenterZ - kDepth * 0.5f)) / 2.2f;
            const float ud = (d.x + kWidth * 0.5f) / 2.2f;
            const float vd = (d.z - (kCenterZ - kDepth * 0.5f)) / 2.2f;
            vertices.push_back({a, terrain_normal(a.x, a.z), ua, va});
            vertices.push_back({b, terrain_normal(b.x, b.z), ub, vb});
            vertices.push_back({c, terrain_normal(c.x, c.z), uc, vc});
            vertices.push_back({a, terrain_normal(a.x, a.z), ua, va});
            vertices.push_back({c, terrain_normal(c.x, c.z), uc, vc});
            vertices.push_back({d, terrain_normal(d.x, d.z), ud, vd});
        }
    }
    return vertices;
}

void destroy_mesh(Mesh& mesh) {
    if (mesh.vbo != 0) {
        glDeleteBuffers(1, &mesh.vbo);
    }
    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
    }
    mesh = {};
}

void init_meshes() {
    cube_mesh = create_mesh(build_cube_vertices());
    sphere_mesh = create_mesh(build_sphere_vertices(32, 16));
    cylinder_mesh = create_mesh(build_cylinder_vertices(32));
    cone_mesh = create_mesh(build_cone_vertices(32));
    pyramid_mesh = create_mesh(build_pyramid_vertices());
    snowflake_mesh = create_mesh(build_snowflake_vertices());
    terrain_mesh = create_mesh(build_terrain_vertices());
}

float random_range(float low, float high) {
    return low + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (high - low);
}

void reset_particle(SnowParticle& particle, bool start_above) {
    particle.position.x = random_range(-8.4f, 8.4f);
    particle.position.y = start_above ? random_range(2.8f, 7.8f) : random_range(0.2f, 7.8f);
    particle.position.z = random_range(-8.4f, 4.8f);
    particle.scale = random_range(0.025f, 0.095f);
    particle.angle = random_range(0.0f, 360.0f);
    particle.fall_speed = random_range(0.28f, 1.25f);
    particle.sway = random_range(0.6f, 1.8f);
    particle.spin_speed = random_range(-90.0f, 90.0f);
}

void init_particles() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    snow_particles.resize(260);
    for (SnowParticle& particle : snow_particles) {
        reset_particle(particle, false);
    }
}

Material material(
    Vec3 diffuse,
    Vec3 specular = {0.15f, 0.15f, 0.15f},
    float shininess = 16.0f,
    float alpha = 1.0f,
    Vec3 emission = {0.0f, 0.0f, 0.0f}) {
    return {diffuse * 0.55f, diffuse, specular, emission, 0, shininess, alpha, false};
}

Material textured_material(
    GLuint texture,
    Vec3 tint = {1.0f, 1.0f, 1.0f},
    Vec3 specular = {0.25f, 0.25f, 0.25f},
    float shininess = 24.0f,
    float alpha = 1.0f,
    Vec3 emission = {0.0f, 0.0f, 0.0f}) {
    if (texture == 0) {
        return material(tint, specular, shininess, alpha, emission);
    }
    return {tint * 0.55f, tint, specular, emission, texture, shininess, alpha, true};
}

std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string directory_name(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return ".";
    }
    return path.substr(0, slash);
}

std::string join_path(const std::string& directory, const std::string& file) {
    if (file.empty()) {
        return directory;
    }
    if (file.front() == '/' || file.find(':') != std::string::npos) {
        return file;
    }
    if (directory.empty() || directory == ".") {
        return file;
    }
    return directory + "/" + file;
}

int obj_index(int index, int count) {
    if (index > 0) {
        return index - 1;
    }
    if (index < 0) {
        return count + index;
    }
    return -1;
}

struct ObjFaceIndex {
    int position = -1;
    int texcoord = -1;
    int normal = -1;
};

ObjFaceIndex parse_face_index(const std::string& token, int position_count, int texcoord_count, int normal_count) {
    ObjFaceIndex result;
    std::stringstream stream(token);
    std::string part;
    if (std::getline(stream, part, '/') && !part.empty()) {
        result.position = obj_index(std::stoi(part), position_count);
    }
    if (std::getline(stream, part, '/') && !part.empty()) {
        result.texcoord = obj_index(std::stoi(part), texcoord_count);
    }
    if (std::getline(stream, part, '/') && !part.empty()) {
        result.normal = obj_index(std::stoi(part), normal_count);
    }
    return result;
}

std::map<std::string, Material> load_mtl_materials(
    const std::string& path,
    std::map<std::string, GLuint>& texture_cache) {
    std::map<std::string, Material> materials;
    std::ifstream file(path);
    if (!file) {
        std::cout << "Failed to open MTL: " << path << std::endl;
        return materials;
    }

    const std::string base_dir = directory_name(path);
    std::string current_name;
    Material current = material({0.72f, 0.72f, 0.72f}, {0.25f, 0.25f, 0.25f}, 28.0f);

    auto save_current = [&]() {
        if (!current_name.empty()) {
            materials[current_name] = current;
        }
    };

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::stringstream stream(line);
        std::string keyword;
        stream >> keyword;

        if (keyword == "newmtl") {
            save_current();
            stream >> current_name;
            current = material({0.72f, 0.72f, 0.72f}, {0.25f, 0.25f, 0.25f}, 28.0f);
        } else if (keyword == "Ka") {
            stream >> current.ambient.x >> current.ambient.y >> current.ambient.z;
        } else if (keyword == "Kd") {
            stream >> current.diffuse.x >> current.diffuse.y >> current.diffuse.z;
            current.ambient = current.diffuse * 0.45f;
        } else if (keyword == "Ks") {
            stream >> current.specular.x >> current.specular.y >> current.specular.z;
        } else if (keyword == "Ns") {
            stream >> current.shininess;
            current.shininess = clampf(current.shininess, 1.0f, 128.0f);
        } else if (keyword == "d") {
            stream >> current.alpha;
        } else if (keyword == "Tr") {
            float transparency = 0.0f;
            stream >> transparency;
            current.alpha = 1.0f - transparency;
        } else if (keyword == "map_Kd") {
            const size_t pos = line.find("map_Kd");
            const std::string texture_name = trim(line.substr(pos + 6));
            const std::string texture_path = join_path(base_dir, texture_name);
            auto cached = texture_cache.find(texture_path);
            if (cached == texture_cache.end()) {
                const GLuint texture = create_texture_2d_from_file(texture_path);
                cached = texture_cache.emplace(texture_path, texture).first;
            }
            if (cached->second != 0) {
                current.texture = cached->second;
                current.use_texture = true;
            }
        }
    }
    save_current();
    return materials;
}

[[maybe_unused]] ObjModel load_obj_model(const std::string& path) {
    ObjModel model;
    std::ifstream file(path);
    if (!file) {
        std::cout << "Failed to open OBJ: " << path << std::endl;
        return model;
    }

    const std::string base_dir = directory_name(path);
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<std::array<float, 2>> texcoords;
    std::vector<Vertex> current_vertices;
    std::map<std::string, Material> materials;
    std::map<std::string, GLuint> texture_cache;
    std::string current_material_name = "default";
    Material current_material = material({0.70f, 0.72f, 0.75f}, {0.36f, 0.36f, 0.38f}, 38.0f);

    auto flush_part = [&]() {
        if (current_vertices.empty()) {
            return;
        }
        ModelPart part;
        part.mesh = create_mesh(current_vertices);
        part.material = current_material;
        part.name = current_material_name;
        model.parts.push_back(part);
        current_vertices.clear();
    };

    auto make_vertex = [&](const ObjFaceIndex& index) {
        Vertex vertex = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f};
        if (index.position >= 0 && index.position < static_cast<int>(positions.size())) {
            vertex.position = positions[index.position];
        }
        if (index.normal >= 0 && index.normal < static_cast<int>(normals.size())) {
            vertex.normal = normals[index.normal];
        }
        if (index.texcoord >= 0 && index.texcoord < static_cast<int>(texcoords.size())) {
            vertex.u = texcoords[index.texcoord][0];
            vertex.v = texcoords[index.texcoord][1];
        }
        return vertex;
    };

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::stringstream stream(line);
        std::string keyword;
        stream >> keyword;

        if (keyword == "mtllib") {
            std::string material_file;
            stream >> material_file;
            const auto loaded_materials = load_mtl_materials(join_path(base_dir, material_file), texture_cache);
            materials.insert(loaded_materials.begin(), loaded_materials.end());
        } else if (keyword == "v") {
            Vec3 position;
            stream >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (keyword == "vt") {
            std::array<float, 2> texcoord = {0.0f, 0.0f};
            stream >> texcoord[0] >> texcoord[1];
            texcoords.push_back(texcoord);
        } else if (keyword == "vn") {
            Vec3 normal;
            stream >> normal.x >> normal.y >> normal.z;
            normals.push_back(normalize(normal));
        } else if (keyword == "usemtl") {
            flush_part();
            stream >> current_material_name;
            const auto found = materials.find(current_material_name);
            current_material = found == materials.end()
                ? material({0.70f, 0.72f, 0.75f}, {0.36f, 0.36f, 0.38f}, 38.0f)
                : found->second;
        } else if (keyword == "f") {
            std::vector<Vertex> face;
            std::string token;
            while (stream >> token) {
                ObjFaceIndex index = parse_face_index(
                    token,
                    static_cast<int>(positions.size()),
                    static_cast<int>(texcoords.size()),
                    static_cast<int>(normals.size()));
                if (index.position >= 0 && index.position < static_cast<int>(positions.size())) {
                    face.push_back(make_vertex(index));
                }
            }
            if (face.size() < 3) {
                continue;
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                Vertex a = face[0];
                Vertex b = face[i];
                Vertex c = face[i + 1];
                const Vec3 face_normal = normalize(cross(b.position - a.position, c.position - a.position));
                if (dot(a.normal, a.normal) < 0.000001f) {
                    a.normal = face_normal;
                }
                if (dot(b.normal, b.normal) < 0.000001f) {
                    b.normal = face_normal;
                }
                if (dot(c.normal, c.normal) < 0.000001f) {
                    c.normal = face_normal;
                }
                current_vertices.push_back(a);
                current_vertices.push_back(b);
                current_vertices.push_back(c);
            }
        }
    }
    flush_part();
    model.loaded = !model.parts.empty();
    std::cout << "Loaded OBJ model: " << path << " (" << model.parts.size() << " material parts)" << std::endl;
    return model;
}

[[maybe_unused]] void destroy_model(ObjModel& model) {
    std::set<GLuint> textures;
    for (ModelPart& part : model.parts) {
        destroy_mesh(part.mesh);
        if (part.material.texture != 0) {
            textures.insert(part.material.texture);
        }
    }
    for (GLuint texture : textures) {
        glDeleteTextures(1, &texture);
    }
    model = {};
}

std::string kenney_holiday_path(const std::string& filename) {
    return join_path(kKenneyHolidayObjDir, filename);
}

void init_holiday_models() {
    holiday_snowman_model = load_obj_model(kenney_holiday_path("snowman-hat.obj"));
    holiday_tree_snow_a_model = load_obj_model(kenney_holiday_path("tree-snow-a.obj"));
    holiday_tree_snow_b_model = load_obj_model(kenney_holiday_path("tree-snow-b.obj"));
    holiday_tree_snow_c_model = load_obj_model(kenney_holiday_path("tree-snow-c.obj"));
    holiday_cabin_wall_model = load_obj_model(kenney_holiday_path("cabin-wall.obj"));
    holiday_cabin_wall_roof_model = load_obj_model(kenney_holiday_path("cabin-wall-roof.obj"));
    holiday_cabin_wall_wreath_model = load_obj_model(kenney_holiday_path("cabin-wall-wreath.obj"));
    holiday_cabin_window_model = load_obj_model(kenney_holiday_path("cabin-window-a.obj"));
    holiday_cabin_roof_snow_model = load_obj_model(kenney_holiday_path("cabin-roof-snow.obj"));
    holiday_cabin_roof_snow_chimney_model = load_obj_model(kenney_holiday_path("cabin-roof-snow-chimney.obj"));
    holiday_cabin_roof_snow_point_model = load_obj_model(kenney_holiday_path("cabin-roof-snow-point.obj"));
    holiday_cabin_corner_logs_model = load_obj_model(kenney_holiday_path("cabin-corner-logs.obj"));
    holiday_cabin_overhang_doorway_model = load_obj_model(kenney_holiday_path("cabin-overhang-door-rotate.obj"));
    holiday_present_a_cube_model = load_obj_model(kenney_holiday_path("present-a-cube.obj"));
    holiday_present_b_cube_model = load_obj_model(kenney_holiday_path("present-b-cube.obj"));
    holiday_present_a_rectangle_model = load_obj_model(kenney_holiday_path("present-a-rectangle.obj"));
    holiday_present_b_rectangle_model = load_obj_model(kenney_holiday_path("present-b-rectangle.obj"));
    holiday_present_a_round_model = load_obj_model(kenney_holiday_path("present-a-round.obj"));
    holiday_present_b_round_model = load_obj_model(kenney_holiday_path("present-b-round.obj"));
}

void destroy_holiday_models() {
    destroy_model(holiday_snowman_model);
    destroy_model(holiday_tree_snow_a_model);
    destroy_model(holiday_tree_snow_b_model);
    destroy_model(holiday_tree_snow_c_model);
    destroy_model(holiday_cabin_wall_model);
    destroy_model(holiday_cabin_wall_roof_model);
    destroy_model(holiday_cabin_wall_wreath_model);
    destroy_model(holiday_cabin_window_model);
    destroy_model(holiday_cabin_roof_snow_model);
    destroy_model(holiday_cabin_roof_snow_chimney_model);
    destroy_model(holiday_cabin_roof_snow_point_model);
    destroy_model(holiday_cabin_corner_logs_model);
    destroy_model(holiday_cabin_overhang_doorway_model);
    destroy_model(holiday_present_a_cube_model);
    destroy_model(holiday_present_b_cube_model);
    destroy_model(holiday_present_a_rectangle_model);
    destroy_model(holiday_present_b_rectangle_model);
    destroy_model(holiday_present_a_round_model);
    destroy_model(holiday_present_b_round_model);
}

void draw_mesh(const Mesh& mesh, const Mat4& model, const Material& mat) {
    set_mat4("model", model);
    set_material(mat);
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
}

[[maybe_unused]] void draw_model(const ObjModel& model, const Mat4& transform) {
    if (!model.loaded) {
        return;
    }
    for (const ModelPart& part : model.parts) {
        draw_mesh(part.mesh, transform, part.material);
    }
}

Mat4 compose_model(Vec3 position, Vec3 size, float yaw_degrees = 0.0f, float roll_degrees = 0.0f) {
    Mat4 model = translate(position);
    model = multiply(model, rotate(yaw_degrees, {0.0f, 1.0f, 0.0f}));
    model = multiply(model, rotate(roll_degrees, {0.0f, 0.0f, 1.0f}));
    model = multiply(model, scale(size));
    return model;
}

void draw_model_at(const ObjModel& model, Vec3 position, Vec3 size, float yaw_degrees = 0.0f, float roll_degrees = 0.0f) {
    if (!model.loaded) {
        return;
    }
    draw_model(model, compose_model(position, size, yaw_degrees, roll_degrees));
}

[[maybe_unused]] bool scan_obj_asset(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return false;
    }

    const std::string base_dir = directory_name(path);
    int position_count = 0;
    int texcoord_count = 0;
    int normal_count = 0;
    int face_count = 0;
    std::set<std::string> material_files;
    std::set<std::string> used_materials;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::stringstream stream(line);
        std::string keyword;
        stream >> keyword;
        if (keyword == "v") {
            ++position_count;
        } else if (keyword == "vt") {
            ++texcoord_count;
        } else if (keyword == "vn") {
            ++normal_count;
        } else if (keyword == "f") {
            ++face_count;
        } else if (keyword == "usemtl") {
            std::string name;
            stream >> name;
            used_materials.insert(name);
        } else if (keyword == "mtllib") {
            std::string name;
            stream >> name;
            material_files.insert(join_path(base_dir, name));
        }
    }

    int material_count = 0;
    int diffuse_texture_count = 0;
    for (const std::string& material_path : material_files) {
        std::ifstream material_file(material_path);
        if (!material_file) {
            std::cerr << "Failed to open MTL: " << material_path << std::endl;
            return false;
        }
        while (std::getline(material_file, line)) {
            line = trim(line);
            if (line.rfind("newmtl ", 0) == 0) {
                ++material_count;
            } else if (line.rfind("map_Kd ", 0) == 0) {
                ++diffuse_texture_count;
            }
        }
    }

    std::cout << "OBJ asset check: " << path << std::endl;
    std::cout << "  vertices: " << position_count
              << ", texcoords: " << texcoord_count
              << ", normals: " << normal_count << std::endl;
    std::cout << "  faces: " << face_count
              << ", used materials: " << used_materials.size() << std::endl;
    std::cout << "  MTL materials: " << material_count
              << ", diffuse textures: " << diffuse_texture_count << std::endl;
    return position_count > 0 && face_count > 0 && !used_materials.empty();
}

void draw_cube(const Mat4& model, const Material& mat) {
    draw_mesh(cube_mesh, model, mat);
}

void draw_sphere(const Mat4& model, const Material& mat) {
    draw_mesh(sphere_mesh, model, mat);
}

Mat4 compose(Vec3 position, Vec3 size) {
    return multiply(translate(position), scale(size));
}

Vec3 camera_position() {
    const float yaw = radians(camera_yaw);
    const float pitch = radians(camera_pitch);
    return {
        std::sin(yaw) * std::cos(pitch) * camera_distance,
        std::sin(pitch) * camera_distance + 2.2f,
        std::cos(yaw) * std::cos(pitch) * camera_distance
    };
}

void draw_skybox(const Mat4& view, const Mat4& projection, float night_factor) {
    if (winter_background_texture == 0 || skybox_shader == nullptr) {
        return;
    }

    Mat4 sky_view = view;
    sky_view.m[12] = 0.0f;
    sky_view.m[13] = 0.0f;
    sky_view.m[14] = 0.0f;

    skybox_shader->use();
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glUniformMatrix4fv(glGetUniformLocation(skybox_shader->id(), "view"), 1, GL_FALSE, sky_view.m);
    glUniformMatrix4fv(glGetUniformLocation(skybox_shader->id(), "projection"), 1, GL_FALSE, projection.m);
    glUniform1f(glGetUniformLocation(skybox_shader->id(), "nightTint"), night_factor);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, winter_background_texture);
    glUniform1i(glGetUniformLocation(skybox_shader->id(), "environmentMap"), 0);
    glBindVertexArray(cube_mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void configure_lights(float night_factor) {
    const Vec3 day_sky = {0.66f, 0.80f, 0.96f};
    const Vec3 night_sky = {0.018f, 0.026f, 0.060f};
    const Vec3 sky = mix_vec3(day_sky, night_sky, night_factor);
    glClearColor(sky.x, sky.y, sky.z, 1.0f);
    set_vec3("fogColor", mix_vec3({0.70f, 0.82f, 0.96f}, {0.055f, 0.070f, 0.120f}, night_factor));
    set_float("fogDensity", mix_vec3({0.0035f, 0.0f, 0.0f}, {0.0065f, 0.0f, 0.0f}, night_factor).x);

    set_vec3("sunLight.direction", mix_vec3({-0.38f, -0.92f, -0.28f}, {0.22f, -0.38f, 0.20f}, night_factor));
    set_vec3("sunLight.ambient", mix_vec3({0.46f, 0.46f, 0.48f}, {0.13f, 0.145f, 0.22f}, night_factor));
    set_vec3("sunLight.diffuse", mix_vec3({0.75f, 0.72f, 0.66f}, {0.20f, 0.24f, 0.38f}, night_factor));
    set_vec3("sunLight.specular", mix_vec3({0.58f, 0.55f, 0.50f}, {0.22f, 0.25f, 0.40f}, night_factor));

    const std::array<Vec3, 2> light_positions = {
        Vec3{-3.75f, 1.55f, -1.55f},
        Vec3{3.6f, 2.1f, 1.5f}
    };
    const std::array<Vec3, 2> light_colors = {
        mix_vec3({0.48f, 0.30f, 0.12f}, {1.0f, 0.64f, 0.28f}, night_factor),
        mix_vec3({0.42f, 0.30f, 0.12f}, {1.0f, 0.76f, 0.34f}, night_factor)
    };
    for (size_t i = 0; i < light_positions.size(); ++i) {
        const std::string prefix = "pointLights[" + std::to_string(i) + "]";
        set_vec3((prefix + ".position").c_str(), light_positions[i]);
        set_vec3((prefix + ".ambient").c_str(), light_colors[i] * (i == 0 ? 0.18f : 0.10f));
        set_vec3((prefix + ".diffuse").c_str(), light_colors[i] * (i == 0 ? 1.20f : 1.35f));
        set_vec3((prefix + ".specular").c_str(), light_colors[i] * 0.85f);
        set_float((prefix + ".constant").c_str(), 1.0f);
        set_float((prefix + ".linear").c_str(), i == 0 ? 0.13f : 0.18f);
        set_float((prefix + ".quadratic").c_str(), i == 0 ? 0.045f : 0.075f);
    }
}

void draw_tree(Vec3 base, float size, float elapsed_time) {
    const float ground = terrain_height(base.x, base.z) + base.y - 0.02f * size;
    const std::array<const ObjModel*, 3> tree_models = {
        &holiday_tree_snow_a_model,
        &holiday_tree_snow_b_model,
        &holiday_tree_snow_c_model
    };
    const int variant = std::abs(static_cast<int>(base.x * 13.0f + base.z * 7.0f)) % static_cast<int>(tree_models.size());
    const ObjModel* tree_model = tree_models[variant];
    if (tree_model->loaded) {
        const float sway = std::sin(elapsed_time * 1.7f + base.x) * 2.8f;
        const float yaw = base.x * 18.0f + base.z * 9.0f;
        const float model_scale = 1.02f * size;
        draw_model_at(*tree_model, {base.x, ground, base.z}, {model_scale, model_scale, model_scale}, yaw, sway);
        return;
    }

    const Material bark = material({0.31f, 0.17f, 0.08f}, {0.04f, 0.035f, 0.03f}, 8.0f);
    const Material pine = material({0.035f, 0.25f, 0.12f}, {0.04f, 0.08f, 0.04f}, 10.0f);
    const Material snow = material({0.90f, 0.96f, 1.0f}, {0.58f, 0.63f, 0.70f}, 30.0f);
    draw_mesh(cylinder_mesh, compose({base.x, ground + 0.38f * size, base.z}, {0.22f * size, 0.75f * size, 0.22f * size}), bark);
    draw_mesh(cone_mesh, compose({base.x, ground + 1.28f * size, base.z}, {1.30f * size, 1.65f * size, 1.30f * size}), pine);
    draw_sphere(compose({base.x, ground + 1.90f * size, base.z}, {0.42f * size, 0.18f * size, 0.42f * size}), snow);
}

void draw_house() {
    const float ground = terrain_height(-3.8f, -2.5f) - 0.18f;
    if (!holiday_cabin_wall_model.loaded || !holiday_cabin_roof_snow_chimney_model.loaded) {
        const Material wall = textured_material(wood_texture, {0.74f, 0.50f, 0.30f}, {0.10f, 0.07f, 0.045f}, 20.0f);
        const Material roof = textured_material(wood_texture, {0.28f, 0.105f, 0.070f}, {0.10f, 0.065f, 0.045f}, 18.0f);
        draw_cube(compose({-3.8f, ground + 0.80f, -2.5f}, {2.35f, 1.45f, 1.85f}), wall);
        draw_cube(compose({-3.8f, ground + 1.75f, -2.5f}, {2.75f, 0.35f, 2.20f}), roof);
        return;
    }

    constexpr float unit = 1.02f;
    const float center_x = -3.8f;
    const float center_z = -2.5f;
    const float front_z = -1.62f;
    const float back_z = -3.44f;
    const float left_x = center_x - 1.38f;
    const float right_x = center_x + 1.38f;
    const Material base = material({0.30f, 0.30f, 0.31f}, {0.08f, 0.08f, 0.08f}, 14.0f);

    draw_cube(compose({center_x, ground + 0.05f, center_z}, {3.15f, 0.10f, 2.25f}), base);

    const std::array<float, 3> front_x = {center_x - unit, center_x, center_x + unit};
    draw_model_at(holiday_cabin_window_model, {front_x[0], ground, front_z - 0.50f * unit}, {unit, unit, unit}, 0.0f);
    draw_model_at(holiday_cabin_overhang_doorway_model, {front_x[1], ground, front_z - 0.50f * unit}, {unit, unit, unit}, 0.0f);
    draw_model_at(holiday_cabin_window_model, {front_x[2], ground, front_z - 0.50f * unit}, {unit, unit, unit}, 0.0f);

    const Material window_warm = material(
        {1.0f, 0.62f, 0.25f},
        {0.75f, 0.48f, 0.18f},
        42.0f,
        1.0f,
        {0.32f, 0.16f, 0.035f});
    draw_cube(compose({front_x[0], ground + 0.50f, front_z - 0.12f}, {0.34f, 0.25f, 0.018f}), window_warm);
    draw_cube(compose({front_x[2], ground + 0.50f, front_z - 0.12f}, {0.34f, 0.25f, 0.018f}), window_warm);

    draw_model_at(holiday_cabin_wall_model, {front_x[0], ground, back_z + 0.50f * unit}, {unit, unit, unit}, 180.0f);
    draw_model_at(holiday_cabin_wall_wreath_model, {front_x[1], ground, back_z + 0.50f * unit}, {unit, unit, unit}, 180.0f);
    draw_model_at(holiday_cabin_wall_model, {front_x[2], ground, back_z + 0.50f * unit}, {unit, unit, unit}, 180.0f);

    const std::array<float, 2> side_z = {center_z - 0.46f * unit, center_z + 0.46f * unit};
    for (float z : side_z) {
        draw_model_at(holiday_cabin_wall_model, {left_x + 0.50f * unit, ground, z}, {unit, unit, unit}, -90.0f);
        draw_model_at(holiday_cabin_window_model, {right_x - 0.50f * unit, ground, z}, {unit, unit, unit}, 90.0f);
    }

    draw_model_at(holiday_cabin_corner_logs_model, {left_x + 0.50f * unit, ground, front_z - 0.50f * unit}, {unit, unit, unit}, 0.0f);
    draw_model_at(holiday_cabin_corner_logs_model, {right_x - 0.50f * unit, ground, front_z - 0.50f * unit}, {unit, unit, unit}, 90.0f);
    draw_model_at(holiday_cabin_corner_logs_model, {right_x - 0.50f * unit, ground, back_z + 0.50f * unit}, {unit, unit, unit}, 180.0f);
    draw_model_at(holiday_cabin_corner_logs_model, {left_x + 0.50f * unit, ground, back_z + 0.50f * unit}, {unit, unit, unit}, -90.0f);

    const float upper_y = ground + 0.82f * unit;
    draw_model_at(holiday_cabin_wall_roof_model, {center_x, upper_y, front_z - 0.50f * unit}, {unit, unit, unit}, 0.0f);
    draw_model_at(holiday_cabin_wall_roof_model, {center_x, upper_y, back_z + 0.50f * unit}, {unit, unit, unit}, 180.0f);

    const float roof_y = ground + 0.72f * unit;
    draw_model_at(holiday_cabin_roof_snow_point_model, {center_x - 1.02f * unit, roof_y, center_z}, {unit, unit, 1.32f * unit}, 0.0f);
    draw_model_at(holiday_cabin_roof_snow_chimney_model, {center_x, roof_y, center_z}, {unit, unit, 1.32f * unit}, 0.0f);
    draw_model_at(holiday_cabin_roof_snow_model, {center_x + 1.02f * unit, roof_y, center_z}, {unit, unit, 1.32f * unit}, 0.0f);
}

void draw_snowman() {
    const float ground = terrain_height(1.9f, -2.0f) - 0.12f;
    if (holiday_snowman_model.loaded) {
        draw_model_at(holiday_snowman_model, {1.9f, ground, -2.0f}, {1.55f, 1.55f, 1.55f}, 0.0f);
        return;
    }

    const Material snow = textured_material(snow_texture, {0.96f, 0.99f, 1.0f}, {0.70f, 0.78f, 0.86f}, 48.0f);
    const Material black = material({0.015f, 0.012f, 0.01f}, {0.25f, 0.25f, 0.25f}, 24.0f);
    draw_sphere(compose({1.9f, ground + 0.56f, -2.0f}, {1.45f, 1.12f, 1.45f}), snow);
    draw_sphere(compose({1.9f, ground + 1.25f, -2.0f}, {0.98f, 0.90f, 0.98f}), snow);
    draw_sphere(compose({1.9f, ground + 1.88f, -2.0f}, {0.66f, 0.62f, 0.66f}), snow);
    draw_sphere(compose({1.72f, ground + 1.98f, -1.45f}, {0.08f, 0.08f, 0.08f}), black);
    draw_sphere(compose({2.08f, ground + 1.98f, -1.45f}, {0.08f, 0.08f, 0.08f}), black);
}

void draw_stump() {
    const Material bark = textured_material(wood_texture, {0.62f, 0.38f, 0.18f}, {0.14f, 0.09f, 0.05f}, 18.0f);
    const Material rings = material({0.76f, 0.54f, 0.28f}, {0.16f, 0.12f, 0.07f}, 16.0f);
    const Material snow = textured_material(snow_texture, {0.93f, 0.98f, 1.0f}, {0.58f, 0.65f, 0.74f}, 34.0f);

    const float stump_x = -1.80f + stump_offset.x;
    const float stump_z = 1.72f + stump_offset.z;
    const float ground = terrain_height(stump_x, stump_z) - 0.10f;

    Mat4 base = translate({stump_x, ground + 0.31f * stump_scale.y, stump_z});
    base = multiply(base, rotate(stump_rotation, {0.0f, 1.0f, 0.0f}));
    base = multiply(base, scale({0.56f * stump_scale.x, 0.62f * stump_scale.y, 0.56f * stump_scale.z}));
    draw_cube(base, bark);

    Mat4 top = translate({stump_x, ground + 0.64f * stump_scale.y, stump_z});
    top = multiply(top, rotate(stump_rotation, {0.0f, 1.0f, 0.0f}));
    top = multiply(top, scale({0.48f * stump_scale.x, 0.055f, 0.48f * stump_scale.z}));
    draw_cube(top, rings);

    Mat4 cap = translate({stump_x, ground + 0.70f * stump_scale.y, stump_z});
    cap = multiply(cap, rotate(stump_rotation, {0.0f, 1.0f, 0.0f}));
    cap = multiply(cap, scale({0.62f * stump_scale.x, 0.045f, 0.62f * stump_scale.z}));
    draw_cube(cap, snow);
}

void draw_lamp(float night_factor) {
    const float ground = terrain_height(3.6f, 1.5f) - 0.16f;
    const Material metal = material({0.08f, 0.08f, 0.075f}, {0.35f, 0.35f, 0.30f}, 36.0f);
    const Material light = material(mix_vec3({0.55f, 0.48f, 0.30f}, {1.0f, 0.82f, 0.38f}, night_factor),
                                    {1.0f, 0.88f, 0.58f},
                                    64.0f,
                                    1.0f,
                                    mix_vec3({0.04f, 0.03f, 0.0f}, {0.48f, 0.32f, 0.08f}, night_factor));
    draw_mesh(cylinder_mesh, compose({3.6f, ground + 1.0f, 1.5f}, {0.12f, 2.0f, 0.12f}), metal);
    draw_cube(compose({3.6f, ground + 2.04f, 1.5f}, {0.72f, 0.07f, 0.16f}), metal);
    draw_sphere(compose({3.6f, ground + 2.1f, 1.5f}, {0.28f, 0.28f, 0.28f}), light);
}

void draw_light_glows(float night_factor) {
    if (night_factor < 0.01f) {
        return;
    }
    const Material lamp_core = material({1.0f, 0.72f, 0.30f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.16f, {0.48f, 0.30f, 0.08f});
    const Material lamp_air = material({1.0f, 0.64f, 0.22f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.045f, {0.18f, 0.10f, 0.02f});
    const Material lamp_spill = material({1.0f, 0.58f, 0.20f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.12f, {0.20f, 0.10f, 0.015f});
    const Material window_air = material({1.0f, 0.58f, 0.22f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.055f, {0.18f, 0.09f, 0.018f});
    const Material window_spill = material({1.0f, 0.52f, 0.18f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.10f, {0.18f, 0.08f, 0.012f});
    const Material moon_glow = material({0.42f, 0.62f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.08f, {0.06f, 0.09f, 0.18f});

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    const float lamp_ground = terrain_height(3.6f, 1.5f) - 0.16f;
    const float house_ground = terrain_height(-3.8f, -2.5f) - 0.18f;
    draw_sphere(compose({3.6f, lamp_ground + 2.1f, 1.5f}, {0.44f, 0.44f, 0.44f}), lamp_core);
    draw_sphere(compose({3.6f, lamp_ground + 2.1f, 1.5f}, {0.86f, 0.72f, 0.86f}), lamp_air);
    draw_sphere(compose({3.6f, lamp_ground + 0.05f, 1.55f}, {1.25f, 0.020f, 0.95f}), lamp_spill);
    draw_sphere(compose({-4.82f, house_ground + 0.58f, -1.42f}, {0.34f, 0.24f, 0.040f}), window_air);
    draw_sphere(compose({-2.78f, house_ground + 0.58f, -1.42f}, {0.34f, 0.24f, 0.040f}), window_air);
    draw_sphere(compose({-3.80f, house_ground + 0.04f, -1.18f}, {1.20f, 0.018f, 0.42f}), window_spill);
    draw_sphere(compose({-6.2f, 4.72f, -11.6f}, {0.48f, 0.48f, 0.08f}), moon_glow);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void draw_background(float night_factor) {
    const Material sky = material(
        mix_vec3({0.64f, 0.78f, 0.95f}, {0.02f, 0.028f, 0.065f}, night_factor),
        {0.0f, 0.0f, 0.0f},
        1.0f,
        1.0f,
        mix_vec3({0.20f, 0.25f, 0.32f}, {0.015f, 0.020f, 0.055f}, night_factor));
    const Material mountain_far = material(mix_vec3({0.42f, 0.55f, 0.70f}, {0.11f, 0.15f, 0.25f}, night_factor), {0.10f, 0.12f, 0.15f}, 18.0f);
    const Material mountain_near = material(mix_vec3({0.50f, 0.62f, 0.76f}, {0.15f, 0.19f, 0.30f}, night_factor), {0.12f, 0.14f, 0.18f}, 20.0f);
    const Material moon = material({0.86f, 0.90f, 1.0f}, {0.90f, 0.95f, 1.0f}, 80.0f, 1.0f, {0.30f, 0.34f, 0.44f});
    const Material star = material({0.90f, 0.94f, 1.0f}, {0.7f, 0.75f, 0.9f}, 64.0f, 1.0f, {0.34f, 0.36f, 0.48f});

    if (winter_background_texture == 0) {
        draw_cube(compose({0.0f, 4.1f, -10.2f}, {24.0f, 10.0f, 0.08f}), sky);
        draw_mesh(pyramid_mesh, compose({-7.6f, 1.45f, -8.9f}, {4.6f, 3.6f, 1.8f}), mountain_far);
        draw_mesh(pyramid_mesh, compose({-3.8f, 1.65f, -9.2f}, {5.5f, 4.0f, 2.0f}), mountain_near);
        draw_mesh(pyramid_mesh, compose({1.2f, 1.34f, -9.1f}, {4.3f, 3.4f, 1.7f}), mountain_far);
        draw_mesh(pyramid_mesh, compose({5.7f, 1.50f, -8.8f}, {5.2f, 3.8f, 1.9f}), mountain_near);
    }

    if (night_factor > 0.01f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_sphere(compose({-6.2f, 4.72f, -11.7f}, {0.40f, 0.40f, 0.40f}), moon);
        const std::array<Vec3, 20> stars = {
            Vec3{-7.8f, 5.8f, -8.7f}, Vec3{-6.2f, 4.8f, -8.6f}, Vec3{-4.1f, 5.25f, -8.6f},
            Vec3{-2.8f, 6.1f, -8.7f}, Vec3{-1.3f, 4.95f, -8.6f}, Vec3{0.6f, 5.75f, -8.7f},
            Vec3{2.2f, 5.25f, -8.6f}, Vec3{3.4f, 6.2f, -8.7f}, Vec3{4.8f, 5.1f, -8.6f},
            Vec3{6.5f, 5.65f, -8.7f}, Vec3{7.6f, 4.9f, -8.6f}, Vec3{-7.1f, 3.95f, -8.6f},
            Vec3{-5.2f, 6.35f, -8.7f}, Vec3{-0.2f, 6.55f, -8.7f}, Vec3{1.6f, 4.45f, -8.6f},
            Vec3{5.9f, 6.35f, -8.7f}, Vec3{7.1f, 6.1f, -8.7f}, Vec3{-3.3f, 4.3f, -8.6f},
            Vec3{3.0f, 4.55f, -8.6f}, Vec3{0.9f, 6.25f, -8.7f}
        };
        for (size_t i = 0; i < stars.size(); ++i) {
            const float s = 0.035f + 0.018f * static_cast<float>(i % 3);
            draw_sphere(compose(stars[i], {s, s, s}), star);
        }
        glDisable(GL_BLEND);
    }
}

void draw_snow_path() {
    const Material packed = material({0.67f, 0.72f, 0.78f}, {0.14f, 0.16f, 0.18f}, 12.0f);
    const Material rim = material({0.93f, 0.98f, 1.0f}, {0.45f, 0.52f, 0.62f}, 30.0f);
    for (int i = 0; i < 12; ++i) {
        const float t = static_cast<float>(i) / 11.0f;
        const float z = 3.2f - t * 4.55f;
        const float x = -0.35f + std::sin(t * 5.3f) * 0.33f;
        const float y = terrain_height(x, z) + 0.035f;
        const float width = 0.74f - t * 0.22f;
        draw_sphere(compose({x, y, z}, {width, 0.035f, 0.42f}), packed);
    }
    draw_sphere(compose({-0.95f, terrain_height(-0.95f, 1.15f) + 0.035f, 1.15f}, {0.92f, 0.055f, 0.92f}), rim);
}

void draw_fence() {
    const Material fence = material({0.40f, 0.24f, 0.12f}, {0.08f, 0.055f, 0.035f}, 12.0f);
    const Material snow = material({0.91f, 0.96f, 1.0f}, {0.55f, 0.62f, 0.70f}, 30.0f);
    for (int i = 0; i < 9; ++i) {
        const float x = -6.8f + static_cast<float>(i) * 1.7f;
        const float ground = terrain_height(x, 2.95f) - 0.14f;
        draw_cube(compose({x, ground + 0.55f, 2.95f}, {0.16f, 1.05f, 0.16f}), fence);
        draw_cube(compose({x, ground + 1.12f, 2.95f}, {0.24f, 0.08f, 0.24f}), snow);
    }
    const float rail_ground = terrain_height(0.0f, 2.95f) - 0.14f;
    draw_cube(compose({0.0f, rail_ground + 0.84f, 2.95f}, {14.5f, 0.13f, 0.12f}), fence);
    draw_cube(compose({0.0f, rail_ground + 0.42f, 2.95f}, {14.5f, 0.11f, 0.10f}), fence);
}

void draw_woodpile() {
    const float pile_ground = terrain_height(-5.55f, -0.75f) - 0.08f;
    const Material log_side = material({0.30f, 0.15f, 0.065f}, {0.05f, 0.035f, 0.02f}, 10.0f);
    const Material log_cut = material({0.64f, 0.43f, 0.22f}, {0.08f, 0.06f, 0.035f}, 14.0f);
    const Material snow = material({0.90f, 0.96f, 1.0f}, {0.48f, 0.55f, 0.66f}, 28.0f);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const float x = -5.9f + static_cast<float>(col) * 0.38f + (row % 2) * 0.16f;
            const float y = pile_ground + 0.18f + static_cast<float>(row) * 0.22f;
            const float z = -0.75f;
            Mat4 log_model = translate({x, y, z});
            log_model = multiply(log_model, rotate(90.0f, {0.0f, 0.0f, 1.0f}));
            log_model = multiply(log_model, scale({0.18f, 0.70f, 0.18f}));
            draw_mesh(cylinder_mesh, log_model, log_side);
            draw_sphere(compose({x + 0.36f, y, z}, {0.09f, 0.09f, 0.09f}), log_cut);
        }
    }
    draw_cube(compose({-5.55f, pile_ground + 0.86f, -0.75f}, {1.35f, 0.055f, 0.48f}), snow);
}

void draw_shrubs(float elapsed_time) {
    const Material branch = material({0.18f, 0.085f, 0.035f}, {0.03f, 0.02f, 0.015f}, 8.0f);
    const Material berry = material({0.68f, 0.035f, 0.045f}, {0.22f, 0.05f, 0.05f}, 26.0f);
    const std::array<Vec3, 3> bases = {
        Vec3{3.15f, 0.08f, -0.55f},
        Vec3{-2.15f, 0.08f, 2.58f},
        Vec3{5.25f, 0.08f, 2.15f}
    };
    for (const Vec3& base : bases) {
        for (int i = 0; i < 7; ++i) {
            const float angle = -58.0f + static_cast<float>(i) * 18.0f + std::sin(elapsed_time + base.x) * 2.0f;
            Mat4 twig = translate({base.x, terrain_height(base.x, base.z) + 0.20f, base.z});
            twig = multiply(twig, rotate(angle, {0.0f, 0.0f, 1.0f}));
            twig = multiply(twig, rotate(12.0f * static_cast<float>(i % 3), {0.0f, 1.0f, 0.0f}));
            twig = multiply(twig, scale({0.035f, 0.62f, 0.035f}));
            draw_mesh(cylinder_mesh, twig, branch);
        }
        draw_sphere(compose({base.x + 0.18f, terrain_height(base.x, base.z) + 0.54f, base.z + 0.05f}, {0.055f, 0.055f, 0.055f}), berry);
        draw_sphere(compose({base.x - 0.14f, terrain_height(base.x, base.z) + 0.40f, base.z - 0.02f}, {0.045f, 0.045f, 0.045f}), berry);
    }
}

void draw_ground_details() {
    const Material mound = material({0.90f, 0.96f, 1.0f}, {0.52f, 0.60f, 0.72f}, 36.0f);
    const Material footprint = material({0.55f, 0.62f, 0.70f}, {0.12f, 0.14f, 0.16f}, 10.0f);
    const Material stone = material({0.34f, 0.36f, 0.38f}, {0.10f, 0.11f, 0.12f}, 14.0f);
    const std::array<Vec3, 7> mounds = {
        Vec3{-5.8f, 0.02f, -0.7f}, Vec3{-2.3f, 0.02f, 1.0f}, Vec3{3.0f, 0.02f, 0.35f},
        Vec3{5.6f, 0.02f, -1.6f}, Vec3{0.0f, 0.02f, -4.6f}, Vec3{-6.2f, 0.02f, 2.4f},
        Vec3{4.8f, 0.02f, 2.7f}
    };
    for (size_t i = 0; i < mounds.size(); ++i) {
        const float scale_x = 1.0f + 0.25f * static_cast<float>(i % 3);
        draw_sphere(compose({mounds[i].x, terrain_height(mounds[i].x, mounds[i].z) + 0.05f, mounds[i].z}, {scale_x, 0.13f, 0.62f}), mound);
    }
    for (int i = 0; i < 8; ++i) {
        const float z = 2.35f - static_cast<float>(i) * 0.40f;
        const float x = -0.32f + (i % 2 == 0 ? -0.16f : 0.16f);
        draw_sphere(compose({x, terrain_height(x, z) + 0.035f, z}, {0.18f, 0.018f, 0.32f}), footprint);
    }
    draw_sphere(compose({-6.8f, terrain_height(-6.8f, -1.8f) + 0.06f, -1.8f}, {0.38f, 0.15f, 0.26f}), stone);
    draw_sphere(compose({4.9f, terrain_height(4.9f, 1.35f) + 0.04f, 1.35f}, {0.26f, 0.10f, 0.22f}), stone);
}

void draw_smoke(float elapsed_time) {
    const float ground = terrain_height(-3.8f, -2.5f) - 0.18f;
    const Material smoke = material({0.60f, 0.64f, 0.70f}, {0.02f, 0.02f, 0.02f}, 6.0f, 0.26f, {0.03f, 0.035f, 0.045f});
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (int i = 0; i < 7; ++i) {
        const float phase = std::fmod(elapsed_time * 0.32f + static_cast<float>(i) * 0.18f, 1.0f);
        const float height = 0.15f + phase * 1.25f;
        const float drift = std::sin(elapsed_time * 0.9f + static_cast<float>(i)) * 0.18f + phase * 0.22f;
        const float size = 0.18f + phase * 0.28f;
        draw_sphere(compose({-4.68f + drift, ground + 2.83f + height, -2.82f}, {size, size * 0.72f, size}), smoke);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void draw_scene_objects(float elapsed_time, float night_factor) {
    const Material ground_snow = textured_material(snow_texture, {0.94f, 0.98f, 1.0f}, {0.50f, 0.58f, 0.70f}, 42.0f);

    draw_background(night_factor);
    draw_mesh(terrain_mesh, Mat4(), ground_snow);
    draw_snow_path();
    draw_ground_details();
    draw_fence();
    draw_woodpile();
    draw_shrubs(elapsed_time);
    draw_house();
    draw_smoke(elapsed_time);
    draw_snowman();
    draw_tree({-6.0f, 0.0f, -3.8f}, 1.25f, elapsed_time);
    draw_tree({-5.5f, 0.0f, 1.2f}, 0.95f, elapsed_time + 1.0f);
    draw_tree({4.8f, 0.0f, -3.6f}, 1.15f, elapsed_time + 0.3f);
    draw_tree({5.9f, 0.0f, 0.2f}, 0.85f, elapsed_time + 2.0f);
    draw_stump();
    draw_lamp(night_factor);
    draw_light_glows(night_factor);
}

float snowfall_amount(float elapsed_time) {
    if (!snowfall_enabled) {
        return 0.0f;
    }
    const float phase = std::fmod(elapsed_time, 22.0f);
    if (phase < 12.0f) {
        return 1.0f;
    }
    if (phase < 18.0f) {
        return 1.0f - (phase - 12.0f) / 6.0f;
    }
    return 0.0f;
}

void update_particles(float dt, float elapsed_time, float amount) {
    if (amount <= 0.0f) {
        return;
    }
    for (SnowParticle& particle : snow_particles) {
        particle.position.y -= particle.fall_speed * dt;
        particle.position.x += std::sin(elapsed_time * particle.sway + particle.position.z) * 0.16f * dt;
        particle.angle += particle.spin_speed * dt;
        if (particle.position.y < terrain_height(particle.position.x, particle.position.z) + 0.05f) {
            reset_particle(particle, true);
        }
    }
}

void draw_snow(float elapsed_time, float amount) {
    if (amount <= 0.0f) {
        return;
    }
    const Material near_snow = material({0.94f, 0.98f, 1.0f}, {0.35f, 0.42f, 0.50f}, 22.0f, 0.64f, {0.05f, 0.06f, 0.07f});
    const Material far_snow = material({0.86f, 0.91f, 0.98f}, {0.18f, 0.22f, 0.28f}, 12.0f, 0.42f, {0.025f, 0.03f, 0.04f});
    const size_t visible_count = static_cast<size_t>(static_cast<float>(snow_particles.size()) * amount);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (size_t i = 0; i < visible_count; ++i) {
        const SnowParticle& particle = snow_particles[i];
        const bool near_camera = particle.position.z > -2.0f;
        const float flutter = 0.5f + 0.5f * std::sin(elapsed_time * 1.8f + particle.angle);
        const float size = particle.scale * (near_camera ? 0.72f : 0.42f) * (0.75f + 0.35f * flutter);
        Mat4 model = translate(particle.position);
        model = multiply(model, scale({size, size * 0.72f, size}));
        draw_sphere(model, near_camera ? near_snow : far_snow);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void reset_stump() {
    stump_offset = {0.0f, 0.0f, 0.0f};
    stump_scale = {1.0f, 1.0f, 1.0f};
    stump_rotation = 0.0f;
}

void render(float elapsed_time, float dt) {
    scene_shader->use();

    const float night_factor = night_mode ? 1.0f : 0.0f;
    configure_lights(night_factor);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int framebuffer_width = kWindowWidth;
    int framebuffer_height = kWindowHeight;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &framebuffer_width, &framebuffer_height);
    const float aspect = static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height);

    const Vec3 eye = camera_position();
    const Vec3 center = {0.0f, 1.1f, -0.8f};
    const Mat4 view = look_at(eye, center, {0.0f, 1.0f, 0.0f});
    const Mat4 projection = perspective(45.0f, aspect, 0.1f, 80.0f);
    set_mat4("view", view);
    set_mat4("projection", projection);
    set_vec3("viewPosition", eye);

    draw_skybox(view, projection, night_factor);
    scene_shader->use();
    draw_scene_objects(elapsed_time, night_factor);

    const float amount = snowfall_amount(elapsed_time);
    update_particles(dt, elapsed_time, amount);
    draw_snow(elapsed_time, amount);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    if (key == GLFW_KEY_N && action == GLFW_PRESS) {
        night_mode = !night_mode;
    } else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        snowfall_enabled = !snowfall_enabled;
    } else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        reset_stump();
    } else if (key == GLFW_KEY_A) {
        camera_yaw -= 3.0f;
    } else if (key == GLFW_KEY_D) {
        camera_yaw += 3.0f;
    } else if (key == GLFW_KEY_W) {
        camera_pitch = clampf(camera_pitch + 2.0f, -8.0f, 55.0f);
    } else if (key == GLFW_KEY_S) {
        camera_pitch = clampf(camera_pitch - 2.0f, -8.0f, 55.0f);
    } else if (key == GLFW_KEY_Q) {
        camera_distance = clampf(camera_distance - 0.35f, 5.5f, 18.0f);
    } else if (key == GLFW_KEY_E) {
        camera_distance = clampf(camera_distance + 0.35f, 5.5f, 18.0f);
    } else if (key == GLFW_KEY_UP) {
        stump_scale.y = clampf(stump_scale.y + 0.08f, 0.55f, 1.45f);
    } else if (key == GLFW_KEY_DOWN) {
        stump_scale.y = clampf(stump_scale.y - 0.08f, 0.55f, 1.45f);
    } else if (key == GLFW_KEY_RIGHT) {
        stump_scale.x = clampf(stump_scale.x + 0.08f, 0.55f, 1.55f);
        stump_scale.z = clampf(stump_scale.z + 0.08f, 0.55f, 1.55f);
    } else if (key == GLFW_KEY_LEFT) {
        stump_scale.x = clampf(stump_scale.x - 0.08f, 0.55f, 1.55f);
        stump_scale.z = clampf(stump_scale.z - 0.08f, 0.55f, 1.55f);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        reset_stump();
        return;
    }
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    if (action == GLFW_PRESS) {
        dragging_stump = true;
        rotating_stump =
            (mods & GLFW_MOD_CONTROL) != 0
            || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);
    } else if (action == GLFW_RELEASE) {
        dragging_stump = false;
        rotating_stump = false;
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (!dragging_stump) {
        return;
    }
    const double dx = xpos - last_mouse_x;
    const double dy = ypos - last_mouse_y;
    last_mouse_x = xpos;
    last_mouse_y = ypos;

    if (rotating_stump) {
        stump_rotation += static_cast<float>(dx) * 0.45f;
        return;
    }
    stump_offset.x = clampf(stump_offset.x + static_cast<float>(dx) * 0.012f, -3.6f, 3.6f);
    stump_offset.z = clampf(stump_offset.z + static_cast<float>(dy) * 0.012f, -3.2f, 3.2f);
}

}  // namespace

int main() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Snow Scene - Final Team Experiment", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    try {
        scene_shader = std::make_unique<Shader>("shaders/snow_scene.vert", "shaders/snow_scene.frag");
        skybox_shader = std::make_unique<Shader>("shaders/skybox.vert", "shaders/skybox.frag");
    } catch (const std::exception& ex) {
        std::cout << ex.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    init_meshes();
    init_resource_textures();
    init_holiday_models();
    init_particles();

    std::cout << "Snow Scene controls:" << std::endl;
    std::cout << "  Left mouse drag       : translate cube stump" << std::endl;
    std::cout << "  Ctrl + left drag      : rotate cube stump" << std::endl;
    std::cout << "  Arrow keys            : stretch cube stump" << std::endl;
    std::cout << "  Right mouse button/R  : reset cube stump" << std::endl;
    std::cout << "  N                     : switch day/night lighting" << std::endl;
    std::cout << "  Space                 : toggle snowfall animation" << std::endl;
    std::cout << "  A/D/W/S/Q/E           : orbit camera and zoom" << std::endl;
    std::cout << "  ESC                   : exit" << std::endl;

    float last_time = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window)) {
        const float current_time = static_cast<float>(glfwGetTime());
        const float dt = current_time - last_time;
        last_time = current_time;

        render(current_time, dt);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    destroy_holiday_models();
    destroy_mesh(cube_mesh);
    destroy_mesh(sphere_mesh);
    destroy_mesh(cone_mesh);
    destroy_mesh(cylinder_mesh);
    destroy_mesh(pyramid_mesh);
    destroy_mesh(snowflake_mesh);
    destroy_mesh(terrain_mesh);
    if (winter_background_texture != 0) {
        glDeleteTextures(1, &winter_background_texture);
    }
    if (rose_texture != 0) {
        glDeleteTextures(1, &rose_texture);
    }
    if (gold_texture != 0) {
        glDeleteTextures(1, &gold_texture);
    }
    if (snow_texture != 0) {
        glDeleteTextures(1, &snow_texture);
    }
    if (wood_texture != 0) {
        glDeleteTextures(1, &wood_texture);
    }
    skybox_shader.reset();
    scene_shader.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
