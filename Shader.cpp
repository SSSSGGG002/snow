#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

std::string Shader::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compile_shader(GLenum shader_type, const std::string& source) {
    const char* source_ptr = source.c_str();

    // 创建并编译着色器，失败时输出编译日志。
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source_ptr, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<GLchar> log(static_cast<size_t>(log_length));
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());
        std::string message(log.begin(), log.end());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile failed: " + message);
    }

    return shader;
}

Shader::Shader(const std::string& vertex_path, const std::string& fragment_path) : program_id_(0) {
    const std::string vertex_source = read_file(vertex_path);
    const std::string fragment_source = read_file(fragment_path);

    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);

    // 将顶点着色器和片元着色器链接为一个可用的程序对象。
    program_id_ = glCreateProgram();
    glAttachShader(program_id_, vertex_shader);
    glAttachShader(program_id_, fragment_shader);
    glLinkProgram(program_id_);

    GLint success = 0;
    glGetProgramiv(program_id_, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint log_length = 0;
        glGetProgramiv(program_id_, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<GLchar> log(static_cast<size_t>(log_length));
        glGetProgramInfoLog(program_id_, log_length, nullptr, log.data());
        std::string message(log.begin(), log.end());
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(program_id_);
        throw std::runtime_error("Shader link failed: " + message);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

Shader::~Shader() {
    if (program_id_ != 0) {
        glDeleteProgram(program_id_);
    }
}

void Shader::use() const {
    glUseProgram(program_id_);
}

GLuint Shader::id() const {
    return program_id_;
}
