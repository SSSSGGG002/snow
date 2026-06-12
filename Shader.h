#pragma once

#define GL_SILENCE_DEPRECATION

#include <OpenGL/gl3.h>

#include <string>

class Shader {
public:
    // 传入顶点着色器和片元着色器路径，完成读取、编译和链接。
    Shader(const std::string& vertex_path, const std::string& fragment_path);
    ~Shader();

    // 激活当前着色器程序。
    void use() const;
    GLuint id() const;

private:
    GLuint program_id_;

    // 从文件中读取着色器源码。
    static std::string read_file(const std::string& path);

    // 编译单个着色器对象。
    static GLuint compile_shader(GLenum shader_type, const std::string& source);
};
