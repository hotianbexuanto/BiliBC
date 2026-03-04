/* shader.h - GLSL shader compile/link utilities */
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

/* Compile a shader from source string. Returns shader ID or 0 on error. */
GLuint shader_compile(GLenum type, const char *source);

/* Link vertex + fragment shaders into a program. Returns program ID or 0. */
GLuint shader_link(GLuint vert, GLuint frag);

/* Convenience: compile + link from source strings. */
GLuint shader_create(const char *vert_src, const char *frag_src);

/* Load shader source from file. Caller must free() the result. */
char *shader_load_file(const char *path);

/* Load + compile + link from file paths. */
GLuint shader_load(const char *vert_path, const char *frag_path);

#endif /* SHADER_H */
