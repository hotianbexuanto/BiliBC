/*
 * GLAD - OpenGL Loader Generator
 * GL 3.3 Core Profile
 * Generated from glad.dav1d.de (hand-curated minimal version)
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */
#ifndef GLAD_GL_H_
#define GLAD_GL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <KHR/khrplatform.h>

/* GLAD calling convention */
#ifndef GLAD_API_PTR
 #ifdef _WIN32
  #define GLAD_API_PTR __stdcall
 #else
  #define GLAD_API_PTR
 #endif
#endif

#ifndef GLAPI
 #define GLAPI extern
#endif

/* ---------- Basic GL types ---------- */
typedef void GLvoid;
typedef unsigned int GLenum;
typedef khronos_float_t GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef khronos_uint8_t GLubyte;
typedef khronos_int8_t GLbyte;
typedef khronos_int16_t GLshort;
typedef khronos_uint16_t GLushort;
typedef khronos_ssize_t GLsizeiptr;
typedef khronos_intptr_t GLintptr;
typedef char GLchar;
typedef khronos_int32_t GLfixed;
typedef khronos_uint16_t GLhalf;
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;
typedef struct __GLsync *GLsync;

/* ---------- GL constants (3.3 core) ---------- */
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NONE                           0
#define GL_ZERO                           0
#define GL_ONE                            1

/* Data types */
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_HALF_FLOAT                     0x140B

/* Errors */
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505

/* Enable/Disable */
#define GL_DEPTH_TEST                     0x0B71
#define GL_BLEND                          0x0BE2
#define GL_SCISSOR_TEST                   0x0C11
#define GL_CULL_FACE                      0x0B44
#define GL_STENCIL_TEST                   0x0B90
#define GL_MULTISAMPLE                    0x809D

/* Blend factors */
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307

/* Blend equations */
#define GL_FUNC_ADD                       0x8006
#define GL_FUNC_SUBTRACT                  0x800A
#define GL_FUNC_REVERSE_SUBTRACT          0x800B

/* Clear bits */
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400

/* Draw modes */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006

/* Texture */
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_REPEAT                         0x2901
#define GL_MIRRORED_REPEAT                0x8370

/* Pixel formats */
#define GL_RED                            0x1903
#define GL_RG                             0x8227
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_R8                             0x8229
#define GL_RG8                            0x822B
#define GL_RGB8                           0x8051
#define GL_RGBA8                          0x8058
#define GL_SRGB8_ALPHA8                   0x8C43
#define GL_DEPTH_COMPONENT                0x1902
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32F             0x8CAC

/* Framebuffer */
#define GL_FRAMEBUFFER                    0x8D40
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_RENDERBUFFER                   0x8D41
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_STENCIL_ATTACHMENT             0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT       0x821A
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5

/* Shaders */
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_ACTIVE_UNIFORMS                0x8B86
#define GL_ACTIVE_ATTRIBUTES              0x8B89

/* Buffer objects */
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STREAM_DRAW                    0x88E0

/* Get parameters */
#define GL_VIEWPORT                       0x0BA2
#define GL_MAX_TEXTURE_SIZE               0x0D33
#define GL_VERSION                        0x1F02
#define GL_RENDERER                       0x1F01
#define GL_VENDOR                         0x1F00
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05

/* Face culling */
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408
#define GL_CW                             0x0900
#define GL_CCW                            0x0901

/* Depth func */
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

/* ---------- GL function pointer types ---------- */
typedef void   (GLAD_API_PTR *PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void   (GLAD_API_PTR *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void   (GLAD_API_PTR *PFNGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar *name);
typedef void   (GLAD_API_PTR *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void   (GLAD_API_PTR *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void   (GLAD_API_PTR *PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void   (GLAD_API_PTR *PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void   (GLAD_API_PTR *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void   (GLAD_API_PTR *PFNGLBLENDEQUATIONPROC)(GLenum mode);
typedef void   (GLAD_API_PTR *PFNGLBLENDEQUATIONSEPARATEPROC)(GLenum modeRGB, GLenum modeAlpha);
typedef void   (GLAD_API_PTR *PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void   (GLAD_API_PTR *PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void   (GLAD_API_PTR *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void   (GLAD_API_PTR *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef GLenum (GLAD_API_PTR *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void   (GLAD_API_PTR *PFNGLCLEARPROC)(GLbitfield mask);
typedef void   (GLAD_API_PTR *PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void   (GLAD_API_PTR *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint (GLAD_API_PTR *PFNGLCREATEPROGRAMPROC)(void);
typedef GLuint (GLAD_API_PTR *PFNGLCREATESHADERPROC)(GLenum type);
typedef void   (GLAD_API_PTR *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void   (GLAD_API_PTR *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef void   (GLAD_API_PTR *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void   (GLAD_API_PTR *PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void   (GLAD_API_PTR *PFNGLDELETESHADERPROC)(GLuint shader);
typedef void   (GLAD_API_PTR *PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void   (GLAD_API_PTR *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void   (GLAD_API_PTR *PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void   (GLAD_API_PTR *PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void   (GLAD_API_PTR *PFNGLDISABLEPROC)(GLenum cap);
typedef void   (GLAD_API_PTR *PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void   (GLAD_API_PTR *PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void   (GLAD_API_PTR *PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void   (GLAD_API_PTR *PFNGLENABLEPROC)(GLenum cap);
typedef void   (GLAD_API_PTR *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void   (GLAD_API_PTR *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void   (GLAD_API_PTR *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void   (GLAD_API_PTR *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void   (GLAD_API_PTR *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void   (GLAD_API_PTR *PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint *renderbuffers);
typedef void   (GLAD_API_PTR *PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void   (GLAD_API_PTR *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef GLenum (GLAD_API_PTR *PFNGLGETERRORPROC)(void);
typedef void   (GLAD_API_PTR *PFNGLGETINTEGERVPROC)(GLenum pname, GLint *data);
typedef void   (GLAD_API_PTR *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void   (GLAD_API_PTR *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void   (GLAD_API_PTR *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void   (GLAD_API_PTR *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef GLint  (GLAD_API_PTR *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef const GLubyte* (GLAD_API_PTR *PFNGLGETSTRINGPROC)(GLenum name);
typedef void   (GLAD_API_PTR *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void   (GLAD_API_PTR *PFNGLPIXELSTOREIPROC)(GLenum pname, GLint param);
typedef void   (GLAD_API_PTR *PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void   (GLAD_API_PTR *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void   (GLAD_API_PTR *PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void   (GLAD_API_PTR *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void   (GLAD_API_PTR *PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void   (GLAD_API_PTR *PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void   (GLAD_API_PTR *PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void   (GLAD_API_PTR *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void   (GLAD_API_PTR *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void   (GLAD_API_PTR *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void   (GLAD_API_PTR *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void   (GLAD_API_PTR *PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void   (GLAD_API_PTR *PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void   (GLAD_API_PTR *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void   (GLAD_API_PTR *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void   (GLAD_API_PTR *PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void   (GLAD_API_PTR *PFNGLDRAWBUFFERSPROC)(GLsizei n, const GLenum *bufs);
typedef void   (GLAD_API_PTR *PFNGLGENERATEMIPMAPPROC)(GLenum target);

/* ---------- GL function declarations ---------- */
GLAPI PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
GLAPI PFNGLATTACHSHADERPROC glad_glAttachShader;
GLAPI PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation;
GLAPI PFNGLBINDBUFFERPROC glad_glBindBuffer;
GLAPI PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
GLAPI PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer;
GLAPI PFNGLBINDTEXTUREPROC glad_glBindTexture;
GLAPI PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
GLAPI PFNGLBLENDEQUATIONPROC glad_glBlendEquation;
GLAPI PFNGLBLENDEQUATIONSEPARATEPROC glad_glBlendEquationSeparate;
GLAPI PFNGLBLENDFUNCPROC glad_glBlendFunc;
GLAPI PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate;
GLAPI PFNGLBUFFERDATAPROC glad_glBufferData;
GLAPI PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
GLAPI PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
GLAPI PFNGLCLEARPROC glad_glClear;
GLAPI PFNGLCLEARCOLORPROC glad_glClearColor;
GLAPI PFNGLCOMPILESHADERPROC glad_glCompileShader;
GLAPI PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
GLAPI PFNGLCREATESHADERPROC glad_glCreateShader;
GLAPI PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
GLAPI PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers;
GLAPI PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
GLAPI PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers;
GLAPI PFNGLDELETESHADERPROC glad_glDeleteShader;
GLAPI PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
GLAPI PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
GLAPI PFNGLDEPTHFUNCPROC glad_glDepthFunc;
GLAPI PFNGLDEPTHMASKPROC glad_glDepthMask;
GLAPI PFNGLDISABLEPROC glad_glDisable;
GLAPI PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
GLAPI PFNGLDRAWARRAYSPROC glad_glDrawArrays;
GLAPI PFNGLDRAWELEMENTSPROC glad_glDrawElements;
GLAPI PFNGLENABLEPROC glad_glEnable;
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
GLAPI PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
GLAPI PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
GLAPI PFNGLGENBUFFERSPROC glad_glGenBuffers;
GLAPI PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
GLAPI PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers;
GLAPI PFNGLGENTEXTURESPROC glad_glGenTextures;
GLAPI PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
GLAPI PFNGLGETERRORPROC glad_glGetError;
GLAPI PFNGLGETINTEGERVPROC glad_glGetIntegerv;
GLAPI PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
GLAPI PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
GLAPI PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
GLAPI PFNGLGETSHADERIVPROC glad_glGetShaderiv;
GLAPI PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
GLAPI PFNGLGETSTRINGPROC glad_glGetString;
GLAPI PFNGLLINKPROGRAMPROC glad_glLinkProgram;
GLAPI PFNGLPIXELSTOREIPROC glad_glPixelStorei;
GLAPI PFNGLREADPIXELSPROC glad_glReadPixels;
GLAPI PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage;
GLAPI PFNGLSCISSORPROC glad_glScissor;
GLAPI PFNGLSHADERSOURCEPROC glad_glShaderSource;
GLAPI PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
GLAPI PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
GLAPI PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;
GLAPI PFNGLUNIFORM1FPROC glad_glUniform1f;
GLAPI PFNGLUNIFORM1IPROC glad_glUniform1i;
GLAPI PFNGLUNIFORM2FPROC glad_glUniform2f;
GLAPI PFNGLUNIFORM3FPROC glad_glUniform3f;
GLAPI PFNGLUNIFORM4FPROC glad_glUniform4f;
GLAPI PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
GLAPI PFNGLUSEPROGRAMPROC glad_glUseProgram;
GLAPI PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
GLAPI PFNGLVIEWPORTPROC glad_glViewport;
GLAPI PFNGLDRAWBUFFERSPROC glad_glDrawBuffers;
GLAPI PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;

/* Convenience macros */
#define glActiveTexture          glad_glActiveTexture
#define glAttachShader           glad_glAttachShader
#define glBindAttribLocation     glad_glBindAttribLocation
#define glBindBuffer             glad_glBindBuffer
#define glBindFramebuffer        glad_glBindFramebuffer
#define glBindRenderbuffer       glad_glBindRenderbuffer
#define glBindTexture            glad_glBindTexture
#define glBindVertexArray        glad_glBindVertexArray
#define glBlendEquation          glad_glBlendEquation
#define glBlendEquationSeparate  glad_glBlendEquationSeparate
#define glBlendFunc              glad_glBlendFunc
#define glBlendFuncSeparate      glad_glBlendFuncSeparate
#define glBufferData             glad_glBufferData
#define glBufferSubData          glad_glBufferSubData
#define glCheckFramebufferStatus glad_glCheckFramebufferStatus
#define glClear                  glad_glClear
#define glClearColor             glad_glClearColor
#define glCompileShader          glad_glCompileShader
#define glCreateProgram          glad_glCreateProgram
#define glCreateShader           glad_glCreateShader
#define glDeleteBuffers          glad_glDeleteBuffers
#define glDeleteFramebuffers     glad_glDeleteFramebuffers
#define glDeleteProgram          glad_glDeleteProgram
#define glDeleteRenderbuffers    glad_glDeleteRenderbuffers
#define glDeleteShader           glad_glDeleteShader
#define glDeleteTextures         glad_glDeleteTextures
#define glDeleteVertexArrays     glad_glDeleteVertexArrays
#define glDepthFunc              glad_glDepthFunc
#define glDepthMask              glad_glDepthMask
#define glDisable                glad_glDisable
#define glDisableVertexAttribArray glad_glDisableVertexAttribArray
#define glDrawArrays             glad_glDrawArrays
#define glDrawElements           glad_glDrawElements
#define glEnable                 glad_glEnable
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
#define glFramebufferRenderbuffer glad_glFramebufferRenderbuffer
#define glFramebufferTexture2D   glad_glFramebufferTexture2D
#define glGenBuffers             glad_glGenBuffers
#define glGenFramebuffers        glad_glGenFramebuffers
#define glGenRenderbuffers       glad_glGenRenderbuffers
#define glGenTextures            glad_glGenTextures
#define glGenVertexArrays        glad_glGenVertexArrays
#define glGetError               glad_glGetError
#define glGetIntegerv            glad_glGetIntegerv
#define glGetProgramInfoLog      glad_glGetProgramInfoLog
#define glGetProgramiv           glad_glGetProgramiv
#define glGetShaderInfoLog       glad_glGetShaderInfoLog
#define glGetShaderiv            glad_glGetShaderiv
#define glGetUniformLocation     glad_glGetUniformLocation
#define glGetString              glad_glGetString
#define glLinkProgram            glad_glLinkProgram
#define glPixelStorei            glad_glPixelStorei
#define glReadPixels             glad_glReadPixels
#define glRenderbufferStorage    glad_glRenderbufferStorage
#define glScissor                glad_glScissor
#define glShaderSource           glad_glShaderSource
#define glTexImage2D             glad_glTexImage2D
#define glTexParameteri          glad_glTexParameteri
#define glTexSubImage2D          glad_glTexSubImage2D
#define glUniform1f              glad_glUniform1f
#define glUniform1i              glad_glUniform1i
#define glUniform2f              glad_glUniform2f
#define glUniform3f              glad_glUniform3f
#define glUniform4f              glad_glUniform4f
#define glUniformMatrix4fv       glad_glUniformMatrix4fv
#define glUseProgram             glad_glUseProgram
#define glVertexAttribPointer    glad_glVertexAttribPointer
#define glViewport               glad_glViewport
#define glDrawBuffers            glad_glDrawBuffers
#define glGenerateMipmap         glad_glGenerateMipmap

/* ---------- Loader ---------- */
typedef void* (*GLADloadproc)(const char *name);
int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif
#endif /* GLAD_GL_H_ */
