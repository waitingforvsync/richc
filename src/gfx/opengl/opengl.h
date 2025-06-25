#ifndef RICHC_GFX_OPENGL_OPENGL_H_
#define RICHC_GFX_OPENGL_OPENGL_H_

#include <GL/gl.h>

typedef struct glfns_t {
    void (*AttachShader)(GLuint program, GLuint shader);
    void (*BindBuffer)(GLenum target, GLuint buffer);
    void (*BindVertexArray)(GLuint array);
    void (*BufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
    void (*Clear)(GLbitfield mask);
    void (*ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void (*CompileShader)(GLuint shader);
    GLuint (*CreateProgram)(void);
    GLuint (*CreateShader)(GLenum type);
    void (*DeleteBuffers)(GLsizei n, const GLuint *buffers);
    void (*DeleteProgram)(GLuint program);
    void (*DeleteShader)(GLuint shader);
    void (*DeleteVertexArrays)(GLsizei n, const GLuint *arrays);
    void (*Disable)(GLenum cap);
    void (*Enable)(GLenum cap);
    void (*EnableVertexAttribArray)(GLuint index);
    void (*GenBuffers)(GLsizei n, GLuint *buffers);
    void (*GenVertexArrays)(GLsizei n, GLuint *arrays);
    void (*GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void (*GetProgramiv)(GLuint program, GLenum pname, GLint *params);
    void (*GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void (*GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
    GLint (*GetUniformLocation)(GLuint program, const GLchar *name);
    void (*LinkProgram)(GLuint program);
    void (*ShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
    void (*UseProgram)(GLuint program);
    void (*VertexAttributePointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
    void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*UniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
} glfns_t;

extern glfns_t gl;


#endif // ifndef RICHC_GFX_OPENGL_OPENGL_H_
