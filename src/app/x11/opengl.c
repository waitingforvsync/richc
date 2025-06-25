#include "richc/defines.h"
#include "gfx/opengl/opengl.h"
#include <GL/glx.h>


#define LOAD_GL_PROC(name, type) \
    require(gl.name = (type)glXGetProcAddressARB((const GLubyte *)"gl" #name))

void opengl_init(void) {
    LOAD_GL_PROC(AttachShader, void (*)(GLuint, GLuint));
    LOAD_GL_PROC(BindBuffer, void (*)(GLenum, GLuint));
    LOAD_GL_PROC(BindVertexArray, void (*)(GLuint));
    LOAD_GL_PROC(BufferData, void (*)(GLenum, GLsizeiptr, const void *, GLenum));
    LOAD_GL_PROC(CompileShader, void (*)(GLuint));
    LOAD_GL_PROC(CreateProgram, GLuint (*)(void));
    LOAD_GL_PROC(CreateShader, GLuint (*)(GLenum));
    LOAD_GL_PROC(DeleteBuffers, void (*)(GLsizei, const GLuint *));
    LOAD_GL_PROC(DeleteProgram, void (*)(GLuint));
    LOAD_GL_PROC(DeleteShader, void (*)(GLuint));
    LOAD_GL_PROC(DeleteVertexArrays, void (*)(GLsizei, const GLuint *));
    LOAD_GL_PROC(EnableVertexAttribArray, void (*)(GLuint));
    LOAD_GL_PROC(GenBuffers, void (*)(GLsizei, GLuint *));
    LOAD_GL_PROC(GenVertexArrays, void (*)(GLsizei, GLuint *));
    LOAD_GL_PROC(GetProgramInfoLog, void (*)(GLuint, GLsizei, GLsizei *, GLchar *));
    LOAD_GL_PROC(GetProgramiv, void (*)(GLuint, GLenum, GLint *));
    LOAD_GL_PROC(GetShaderInfoLog, void (*)(GLuint, GLsizei, GLsizei *, GLchar *));
    LOAD_GL_PROC(GetShaderiv, void (*)(GLuint, GLenum, GLint *));
    LOAD_GL_PROC(GetUniformLocation, GLint (*)(GLuint, const GLchar *));
    LOAD_GL_PROC(LinkProgram, void (*)(GLuint));
    LOAD_GL_PROC(ShaderSource, void (*)(GLuint, GLsizei, const GLchar *const *, const GLint *));
    LOAD_GL_PROC(UseProgram, void (*)(GLuint));
    LOAD_GL_PROC(VertexAttributePointer, void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *));
    LOAD_GL_PROC(UniformMatrix4fv, void (*)(GLint, GLsizei, GLboolean, const GLfloat *));
}
