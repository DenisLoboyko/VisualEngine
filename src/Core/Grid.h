#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace VE {

    class Grid
    {
    public:
        unsigned int VAO, VBO;
        int vertexCount;

        Grid(int size = 20)
        {
            std::vector<float> verts;

            for (int i = -size; i <= size; i++)
            {
                // X lines
                verts.push_back((float)i);
                verts.push_back(0.0f);
                verts.push_back(-(float)size);

                verts.push_back((float)i);
                verts.push_back(0.0f);
                verts.push_back((float)size);

                // Z lines
                verts.push_back(-(float)size);
                verts.push_back(0.0f);
                verts.push_back((float)i);

                verts.push_back((float)size);
                verts.push_back(0.0f);
                verts.push_back((float)i);
            }

            vertexCount = verts.size() / 3;

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER,
                verts.size() * sizeof(float),
                verts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
        }

        void Draw(unsigned int shaderID,
                  const glm::mat4& view,
                  const glm::mat4& proj)
        {
            glUseProgram(shaderID);
            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"),      1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"),       1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3f(glGetUniformLocation(shaderID, "gridColor"), 0.3f, 0.3f, 0.35f);
            glBindVertexArray(VAO);
            glDrawArrays(GL_LINES, 0, vertexCount);
            glBindVertexArray(0);
        }

        ~Grid()
        {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
        }
    };
}