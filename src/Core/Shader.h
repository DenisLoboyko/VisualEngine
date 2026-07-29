#pragma once
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace VE {

    class Shader
    {
    public:
        unsigned int ID;

        Shader(const char* vertSrc, const char* fragSrc)
        {
            unsigned int vert = Compile(GL_VERTEX_SHADER, vertSrc);
            unsigned int frag = Compile(GL_FRAGMENT_SHADER, fragSrc);

            ID = glCreateProgram();
            glAttachShader(ID, vert);
            glAttachShader(ID, frag);
            glLinkProgram(ID);

            int success;
            glGetProgramiv(ID, GL_LINK_STATUS, &success);
            if (!success) {
                char log[512];
                glGetProgramInfoLog(ID, 512, nullptr, log);
                std::cerr << "Shader link error: " << log << "\n";
            }

            glDeleteShader(vert);
            glDeleteShader(frag);
        }

        void Use() { glUseProgram(ID); }
        void Delete() { glDeleteProgram(ID); }

    private:
        unsigned int Compile(unsigned int type, const char* src)
        {
            unsigned int shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            int success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char log[512];
                glGetShaderInfoLog(shader, 512, nullptr, log);
                std::cerr << "Shader compile error: " << log << "\n";
            }
            return shader;
        }
    };
}