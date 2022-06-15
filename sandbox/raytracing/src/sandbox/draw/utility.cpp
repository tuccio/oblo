#include <sandbox/draw/utility.hpp>

#include <GL/glew.h>

namespace oblo
{
    u32 compile_vert_frag_program(const char* vsCode, const char* fsCode)
    {
        const char* vertexShaders[] = {vsCode};
        const char* fragmentShaders[] = {fsCode};

        GLint success;

        const auto vertShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertShader, 1, vertexShaders, nullptr);
        glCompileShader(vertShader);
        glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            return 0;
        }

        const auto fragShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragShader, 1, fragmentShaders, nullptr);
        glCompileShader(fragShader);
        glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            glDeleteShader(vertShader);
            return 0;
        }

        const auto shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertShader);
        glAttachShader(shaderProgram, fragShader);
        glLinkProgram(shaderProgram);
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

        if (!success)
        {
            glDeleteShader(vertShader);
            glDeleteShader(fragShader);
            glDeleteProgram(shaderProgram);

            return 0;
        }

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        return shaderProgram;
    }
}