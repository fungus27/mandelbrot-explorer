#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define MAX_COMMAND_SIZE 512
#define MAX_INTERVAL_COUNT 100

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

typedef struct {
    float r;
    float g;
    float b;
} color;

typedef struct {
    color color;
    float s;
    unsigned char pos;
} interval;

typedef struct {
    double x;
    double y;
} dd;

// double precision functions
dd dd_set(double d) {
    dd t = {d, 0.0};
    return t;
}

dd dd_split64(double d)
{
    const double SPLITTER = (1 << 29) + 1;
    double t = d * SPLITTER;
    dd result;
    result.x = t - (t - d);
    result.y = d - result.x;

    return result;
}

dd dd_quick_two_sum(double a, double b)
{
    dd temp;
    temp.x = a + b;
    temp.y = b - (temp.x - a);
    return temp;
}

dd dd_two_prod(double a, double b) {
    dd p;
    p.x = a * b;
    dd aS = dd_split64(a);
    dd bS = dd_split64(b);
    p.y = (aS.x * bS.x - p.x) + aS.x * bS.y + aS.y * bS.x + aS.y * bS.y;
    return p;
}

dd dd_add(dd dsa, dd dsb)
{
    dd dsc;
    double t1, t2, e;

    t1 = dsa.x + dsb.x;
    e = t1 - dsa.x;
    t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;

    dsc.x = t1 + t2;
    dsc.y = t2 - (dsc.x - t1);
    return dsc;
}

dd dd_mul(dd a, dd b)
{
    dd p;

    p = dd_two_prod(a.x, b.x);
    p.y += a.x * b.y + a.y * b.x;
    p = dd_quick_two_sum(p.x, p.y);
    return p;
}

dd dd_div(dd b, dd a) {
    double xn = 1.0 / a.x;
    dd yn = {b.x * xn, 0.0};
    dd a_yn = dd_mul(a, yn);
    a_yn.x = -a_yn.x;
    a_yn.y = -a_yn.y;

    double diff = (dd_add(b, a_yn)).x;
    dd prod = dd_two_prod(xn, diff);
    return dd_add(yn, prod);
}

enum input_mode { MOVE, HUE };

static dd mag = {0.5, 0.0};
static dd x_offset = {-.5, 0.0}, y_offset = {-.5, 0.0};
static char regen_set = 1;

static int current_mode = MOVE;
static char change_mode = 1;

static color hue[256];
static char change_hue = 1;
    
static color start_color = {0.0f, 0.0f, 0.25f};
static unsigned int selected_interval = 0;
static unsigned int interval_count = 4;
static interval intervals[MAX_INTERVAL_COUNT] = {
        { {0.129000f, 0.921000f, 0.415000f} , 0.700000f , 89},
        { {0.882000f, 0.917000f, 0.125000f} , 4.699998f , 149},
        { {0.701000f, 0.094000f, 0.094000f} , 3.899998f , 217},
        { {0.000000f, 0.000000f, 0.000000f} , 2.400000f , 255},
};

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    if (yoffset > 0.0f)
        mag = dd_mul(mag, dd_set(1.1));
    else
        mag = dd_mul(mag, dd_set(1.0/1.1));
    regen_set = 1;
}

void *get_commands(void *arg) {
    char *t_command = (char*)arg;
    while (1) {
        int c, i = 0;
        while ((c = getchar()) != '\n' && c != EOF && i < MAX_COMMAND_SIZE) {
            if (i > 0 && t_command[i-1] == ' ' && c == ' ')
                continue;
            t_command[i] = c;
            ++i;
        }
        t_command[i] = 0;
    }
    return NULL;
}

color c_lerp(color a, color b, float t) {
    color res = {(1 - t) * a.r + t * b.r, (1 - t) * a.g + t * b.g, (1 - t) * a.b + t * b.b};
    return res;
}

void gen_hue(color start_color, unsigned int int_count, interval *intervals, unsigned int col_count, color *hue) {
    for (unsigned int i = 0; i < col_count; ++i)
        hue[i] = start_color;

    if (!int_count)
        return;

    unsigned char last_pos = 0;
    for (unsigned int i = 0; i < int_count; ++i) {
        color s_color = i > 0 ? intervals[i - 1].color : start_color;
        unsigned int range = (intervals[i].pos - last_pos) > 0 ? intervals[i].pos - last_pos : 0;
        for (unsigned int j = 0; j <= range; ++j) {
            float t = pow((float)j/range, intervals[i].s);
            hue[j + last_pos] = c_lerp(s_color, intervals[i].color, t);
        }
        last_pos = intervals[i].pos + 1;
    }
}

// TODO: do continous input (holding down keys)
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {

        // mode changing
        switch (key) {
            case GLFW_KEY_H:
                current_mode = current_mode == MOVE ? HUE : MOVE;
                printf("changed mode to %s.\n", current_mode == MOVE ? "MOVE" : "HUE");
                change_mode = 1;
                break;
        }

        // modes
        if (current_mode == MOVE) {
            switch (key) {
                case GLFW_KEY_W:
                    y_offset = dd_add(y_offset, dd_div(dd_set(0.1), mag));
                    break;
                case GLFW_KEY_A:
                    x_offset = dd_add(x_offset, dd_div(dd_set(-0.1), mag));
                    break;
                case GLFW_KEY_S:
                    y_offset = dd_add(y_offset, dd_div(dd_set(-0.1), mag));
                    break;
                case GLFW_KEY_D:
                    x_offset = dd_add(x_offset, dd_div(dd_set(0.1), mag));
                    break;
                case GLFW_KEY_P:
                    printf("mag: %f, offset: (%f, %f).\n", mag.x, x_offset.x, y_offset.x);
                    break;
            }
            regen_set = 1;
        }
        else if (current_mode == HUE) {
            switch (key) {
                case GLFW_KEY_W:
                    intervals[selected_interval].s += 0.1f;
                    gen_hue(start_color, interval_count, intervals, 256, hue);
                    break;
                case GLFW_KEY_A:
                    intervals[selected_interval].pos--;
                    while (selected_interval > 0 && intervals[selected_interval].pos <= intervals[selected_interval - 1].pos) {
                        interval t = intervals[selected_interval];
                        intervals[selected_interval] = intervals[selected_interval - 1];
                        intervals[selected_interval - 1] = t;
                        selected_interval--;
                    }
                    gen_hue(start_color, interval_count, intervals, 256, hue);
                    break;
                case GLFW_KEY_S:
                    intervals[selected_interval].s -= 0.1f;
                    gen_hue(start_color, interval_count, intervals, 256, hue);
                    break;
                case GLFW_KEY_D:
                    intervals[selected_interval].pos++;
                    while (selected_interval < interval_count - 1 && intervals[selected_interval].pos > intervals[selected_interval + 1].pos) {
                        interval t = intervals[selected_interval];
                        intervals[selected_interval] = intervals[selected_interval + 1];
                        intervals[selected_interval + 1] = t;
                        selected_interval++;
                    }
                    gen_hue(start_color, interval_count, intervals, 256, hue);
                    break;
                case GLFW_KEY_C:
                    if (interval_count + 1 > MAX_INTERVAL_COUNT) {
                        printf("reached max interval count (%u).\n", MAX_INTERVAL_COUNT);
                        break;
                    }
                    for (unsigned int i = interval_count; i > 0; --i)
                        intervals[i] = intervals[i - 1];
                    interval t = {{0.0f, 0.0f, 0.0f}, 1.0f, 0};
                    intervals[0] = t;
                    selected_interval = 0;
                    interval_count++;
                    for (unsigned int i = 0; i < interval_count; ++i) {
                        printf("%c%u -> { {%f, %f, %f} , %f , %u}\n", i == selected_interval ? '*' : ' ', i, intervals[i].color.r, intervals[i].color.g, intervals[i].color.b, intervals[i].s, intervals[i].pos);
                    }
                    gen_hue(start_color, interval_count, intervals, 256, hue);
                    printf("created new interval.\n");
                    break;
            }
            change_hue = 1;
            //printf("selected interval: {{%f, %f, %f}, %f, %u}\n", intervals[selected_interval].color.r, intervals[selected_interval].color.g, intervals[selected_interval].color.b, intervals[selected_interval].s, intervals[selected_interval].pos);
        }

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

    gen_hue(start_color, interval_count, intervals, 256, hue);
    
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

    const unsigned int work_group_size = 30;
    glUseProgram(compute_prog);
    glUniform2d(glGetUniformLocation(compute_prog, "bottom_left"), -1.0, -1.0);
    glUniform2d(glGetUniformLocation(compute_prog, "upper_right"), 1.0, 1.0);
    glUniform1f(glGetUniformLocation(compute_prog, "max_iters"), 3000.0f);

    char command[MAX_COMMAND_SIZE + 1];

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, get_commands, (void*)command);

    assert(!glGetError());

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        if (regen_set) {
            glUseProgram(compute_prog);
            glUniform2d(glGetUniformLocation(compute_prog, "mag"), mag.x, mag.y);
            glUniform2d(glGetUniformLocation(compute_prog, "offsetx"), x_offset.x, x_offset.y);
            glUniform2d(glGetUniformLocation(compute_prog, "offsety"), y_offset.x, y_offset.y);
            glDispatchCompute(w / work_group_size, h / work_group_size, 1);
            regen_set = 0;
        }

        
        glUseProgram(render_prog);

        if (change_mode) {
            glUniform1i(glGetUniformLocation(render_prog, "current_mode"), current_mode);
            change_mode = 0;
        }

        if (change_hue) {
            glUniform3fv(glGetUniformLocation(render_prog, "hue"), 256, (float*)hue);
            change_hue = 0;
        }
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        if (command[0] != 0) {
            char *first_tok = strtok(command, " ");
            if (!strcmp(first_tok, "set_int_pos")) {
                sscanf(strtok(NULL, " "), "%hhu", &intervals[selected_interval].pos);
                printf("position set.\n");
                gen_hue(start_color, interval_count, intervals, 256, hue);
                change_hue = 1;
            }
            else if (!strcmp(first_tok, "set_int_col")) {
                sscanf(strtok(NULL, " "), "{%f,%f,%f}", &intervals[selected_interval].color.r, &intervals[selected_interval].color.g, &intervals[selected_interval].color.b);
                printf("color set.\n");
                gen_hue(start_color, interval_count, intervals, 256, hue);
                change_hue = 1;
            }
            else if (!strcmp(first_tok, "set_int_s")) {
                sscanf(strtok(NULL, " "), "%f", &intervals[selected_interval].s);
                printf("s set.\n");
                gen_hue(start_color, interval_count, intervals, 256, hue);
                change_hue = 1;
            }
            else if (!strcmp(first_tok, "set_int_sel")) {
                sscanf(strtok(NULL, " "), "%u", &selected_interval);
                printf("selected interval set.\n");
            }
            else if (!strcmp(first_tok, "set_start_col")) {
                sscanf(strtok(NULL, " "), "{%f,%f,%f}", &start_color.r, &start_color.g, &start_color.b);
                printf("start color set.\n");
                gen_hue(start_color, interval_count, intervals, 256, hue);
                change_hue = 1;
            }
            else if (!strcmp(first_tok, "la_int")) {
                for (unsigned int i = 0; i < interval_count; ++i) {
                    printf("%c%u -> { {%f, %f, %f} , %f , %u}\n", i == selected_interval ? '*' : ' ', i, intervals[i].color.r, intervals[i].color.g, intervals[i].color.b, intervals[i].s, intervals[i].pos);
                }
            }
            else if (!strcmp(first_tok, "dump_int")) {
                printf("static unsigned int interval_count = %u;\nstatic interval intervals[MAX_INTERVAL_COUNT] = {\n", interval_count);
                for (unsigned int i = 0; i < interval_count; ++i) {
                    printf("\t{ {%ff, %ff, %ff} , %ff , %u},\n", intervals[i].color.r, intervals[i].color.g, intervals[i].color.b, intervals[i].s, intervals[i].pos);
                }
                printf("};\n");
            }
            
            command[0] = 0;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    free(texture_data);
    glfwTerminate();
}
