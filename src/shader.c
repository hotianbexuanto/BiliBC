/* shader.c - GLSL shader compile/link utilities */
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GLuint shader_compile(GLenum type, const char *source) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "[Shader] compile error (%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "vert" : "frag", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint shader_link(GLuint vert, GLuint frag) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);

    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "[Shader] link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

GLuint shader_create(const char *vert_src, const char *frag_src) {
    GLuint v = shader_compile(GL_VERTEX_SHADER, vert_src);
    if (!v) return 0;
    GLuint f = shader_compile(GL_FRAGMENT_SHADER, frag_src);
    if (!f) { glDeleteShader(v); return 0; }

    GLuint p = shader_link(v, f);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

char *shader_load_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[Shader] cannot open: %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    fread(buf, 1, sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    return buf;
}

GLuint shader_load(const char *vert_path, const char *frag_path) {
    char *vs = shader_load_file(vert_path);
    if (!vs) return 0;
    char *fs = shader_load_file(frag_path);
    if (!fs) { free(vs); return 0; }

    GLuint p = shader_create(vs, fs);
    free(vs);
    free(fs);
    return p;
}
