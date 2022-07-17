#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

unsigned int compile_render_shaders(const char *vert_filepath, const char *frag_filepath) {
    FILE *vert_file = fopen(vert_filepath, "r");
    if (!vert_file) {
        int error = errno;
        fprintf(stderr, "unable to load vertex shader at '%s'.\nerror: %s", vert_filepath, strerror(error));
        exit(EXIT_FAILURE);
    }

    FILE *frag_file = fopen(frag_filepath, "r");
    if (!frag_file) {
        int error = errno;
        fprintf(stderr, "unable to load fragment shader at '%s'.\nerror: %s", frag_filepath, strerror(error));
        exit(EXIT_FAILURE);
    }

    fseek(vert_file, 0, SEEK_END);
    unsigned int vert_size = ftell(vert_file);
    fseek(vert_file, 0, SEEK_SET);

    fseek(frag_file, 0, SEEK_END);
    unsigned int frag_size = ftell(frag_file);
    fseek(frag_file, 0, SEEK_SET);

    char *vert_content = malloc(vert_size + 1);
    fread(vert_content, vert_size, 1, vert_file);
    vert_content[vert_size] = 0;

    char *frag_content = malloc(frag_size + 1);
    fread(frag_content, frag_size, 1, frag_file);
    frag_content[frag_size] = 0;

    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, (const char**)&vert_content, NULL);
    glCompileShader(vert);

    int success;
    char infoLog[512];
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(vert, 512, NULL, infoLog);
        fprintf(stderr, "vertex shader failed to compile.\nerror: %s\n", infoLog);
        exit(EXIT_FAILURE);
    }

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, (const char**)&frag_content, NULL);
    glCompileShader(frag);

    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag, 512, NULL, infoLog);
        fprintf(stderr, "fragment shader failed to compile.\nerror: %s\n", infoLog);
        exit(EXIT_FAILURE);
    }
    
    free(vert_content);
    free(frag_content);

    unsigned int prog = glCreateProgram();

    glAttachShader(prog, vert);
    glAttachShader(prog, frag);

    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        fprintf(stderr, "render shader program failed to link.\nerror: %s\n", infoLog);
        exit(EXIT_FAILURE);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    return prog;
}

unsigned int compile_compute_shader(const char* filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        int error = errno;
        fprintf(stderr, "unable to load compute shader at '%s'.\nerror: %s", filepath, strerror(error));
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    unsigned int size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, size, 1, file);
    content[size] = 0;

    unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, (const char**)&content, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "compute shader failed to compile.\nerror: %s\n", infoLog);
        exit(EXIT_FAILURE);
    }

    free(content);

    unsigned int prog = glCreateProgram();

    glAttachShader(prog, shader);

    glLinkProgram(prog);
    
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        fprintf(stderr, "compute shader program failed to link.\nerror: %s\n", infoLog);
        exit(EXIT_FAILURE);
    }

    glDeleteShader(shader);

    return prog;
}

static double mag = 1.0;
static double x_offset = 0.0, y_offset = 0.0;
static char regen_set = 1;

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    if (yoffset > 0.0f)
        mag *= 1.1;
    else
        mag *= 1.0/1.1;
    regen_set = 1;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_W:
                y_offset += 0.1f/mag;
                break;
            case GLFW_KEY_A:
                x_offset -= 0.1f/mag;
                break;
            case GLFW_KEY_S:
                y_offset -= 0.1f/mag;
                break;
            case GLFW_KEY_D:
                x_offset += 0.1f/mag;
                break;
            case GLFW_KEY_P:
                printf("mag: %f, offset: (%f, %f).\n", mag, x_offset, y_offset);
                break;
        }
        regen_set = 1;
    }
}

typedef struct {
    float r;
    float g;
    float b;
} color;

typedef struct {
    color color;
    float s;
    unsigned int pos;
} interval;

color c_lerp(color a, color b, float t) {
    color res = {(1 - t) * a.r + t * b.r, (1 - t) * a.g + t * b.g, (1 - t) * a.b + t * b.b};
    return res;
}

void gen_hue(color start_color, unsigned int int_count, interval *intervals, unsigned int col_count, color *hue) {
    if (!int_count) {
        for (unsigned int i = 0; i < col_count; ++i)
            hue[i] = start_color;
        return;
    }

    unsigned int last_pos = 0;
    for (unsigned int i = 0; i < int_count; ++i) {
        color s_color = i > 0 ? intervals[i - 1].color : start_color;
        for (unsigned int j = 0; j <= intervals[i].pos - last_pos; ++j) {
            float t = pow((float)j/(intervals[i].pos - last_pos), intervals[i].s);
            hue[j + last_pos] = c_lerp(s_color, intervals[i].color, t);
        }
        last_pos = intervals[i].pos + 1;
    }
}

int main() {
    if (!glfwInit()) {
        const char *description;
        int code = glfwGetError(&description);
        fprintf(stderr, "glfw failed to initialize.\nerror (%d): %s\n", code, description);
        exit(EXIT_FAILURE);
    }

    const unsigned int w = 600, h = 600;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(w, h, "dev", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    int max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    printf("max texture size: %d\n", max_texture_size);
    assert(w <= max_texture_size && h <= max_texture_size);

    GLenum err; 
    if ((err = glewInit()) != GLEW_OK) {
        fprintf(stderr, "glew failed to initialize.\nerror (%u): %s\n", err, glewGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    printf("version: %s\n", glGetString(GL_VERSION));

    float vertices[] = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0
    };

    color hue[256];
    for (unsigned int i = 0; i < 256; ++i) {
        hue[i].r = i/255.0f;
        hue[i].g = 0.0f;
        hue[i].b = 0.0f;
    }
    
    color start_color = {0.0f, 0.0f, 0.25f};
    interval intervals[] = {
        { {0.9f, 0.0f, 0.0f}, 1.26f, 128 },
        { {0.0f, 0.0f, 0.0f}, 0.9f, 255 },
    };
    gen_hue(start_color, sizeof(intervals)/sizeof(interval), intervals, 256, hue);
    
    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    unsigned int vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    unsigned char *texture_data = malloc(w * h);
    
    unsigned int texture;
    glGenTextures(1, &texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, texture_data);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

    unsigned int compute_prog = compile_compute_shader("genset.glsl");
    
    unsigned int render_prog = compile_render_shaders("vert.glsl", "frag.glsl");
    glUseProgram(render_prog);
    glUniform3fv(glGetUniformLocation(render_prog, "hue"), 256, (float*)hue);

    const unsigned int work_group_size = 30;
    glUseProgram(compute_prog);
    glUniform2d(glGetUniformLocation(compute_prog, "bottom_left"), -1.0, -1.0);
    glUniform2d(glGetUniformLocation(compute_prog, "upper_right"), 1.0, 1.0);
    glUniform1f(glGetUniformLocation(compute_prog, "max_iters"), 1000.0f);

    assert(!glGetError());

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        if (regen_set) {
            glUseProgram(compute_prog);
            glUniform1d(glGetUniformLocation(compute_prog, "mag"), mag);
            glUniform2d(glGetUniformLocation(compute_prog, "offset"), x_offset, y_offset);
            glDispatchCompute(w / work_group_size, h / work_group_size, 1);
            regen_set = 0;
        }

        glUseProgram(render_prog);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    free(texture_data);
    glfwTerminate();
}
