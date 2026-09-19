#include "render/GlApi.h"

namespace glapi {

#define GL_API_DEF(ret,name,args) ret (APIENTRY* name) args = nullptr
GL_API_DEF(void,GenVertexArrays,(GLsizei,GLuint*)); GL_API_DEF(void,BindVertexArray,(GLuint));
GL_API_DEF(void,DeleteVertexArrays,(GLsizei,const GLuint*)); GL_API_DEF(void,GenBuffers,(GLsizei,GLuint*));
GL_API_DEF(void,BindBuffer,(GLenum,GLuint)); GL_API_DEF(void,BufferData,(GLenum,Size,const void*,GLenum));
GL_API_DEF(void,BufferSubData,(GLenum,Size,Size,const void*));
GL_API_DEF(void,DeleteBuffers,(GLsizei,const GLuint*)); GL_API_DEF(void,EnableVertexAttribArray,(GLuint));
GL_API_DEF(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*));
GL_API_DEF(void,VertexAttribIPointer,(GLuint,GLint,GLenum,GLsizei,const void*));
GL_API_DEF(GLuint,CreateShader,(GLenum)); GL_API_DEF(void,ShaderSource,(GLuint,GLsizei,const Char* const*,const GLint*));
GL_API_DEF(void,CompileShader,(GLuint)); GL_API_DEF(void,GetShaderiv,(GLuint,GLenum,GLint*));
GL_API_DEF(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,Char*)); GL_API_DEF(void,DeleteShader,(GLuint));
GL_API_DEF(GLuint,CreateProgram,()); GL_API_DEF(void,AttachShader,(GLuint,GLuint)); GL_API_DEF(void,LinkProgram,(GLuint));
GL_API_DEF(void,GetProgramiv,(GLuint,GLenum,GLint*)); GL_API_DEF(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,Char*));
GL_API_DEF(void,DeleteProgram,(GLuint)); GL_API_DEF(void,UseProgram,(GLuint));
GL_API_DEF(GLint,GetUniformLocation,(GLuint,const Char*)); GL_API_DEF(void,UniformMatrix4fv,(GLint,GLsizei,GLboolean,const GLfloat*));
GL_API_DEF(void,Uniform4f,(GLint,GLfloat,GLfloat,GLfloat,GLfloat)); GL_API_DEF(void,Uniform3f,(GLint,GLfloat,GLfloat,GLfloat)); GL_API_DEF(void,Uniform2f,(GLint,GLfloat,GLfloat)); GL_API_DEF(void,Uniform1i,(GLint,GLint)); GL_API_DEF(void,Uniform1iv,(GLint,GLsizei,const GLint*)); GL_API_DEF(void,Uniform1f,(GLint,GLfloat));
GL_API_DEF(void,DrawElements,(GLenum,GLsizei,GLenum,const void*)); GL_API_DEF(void,DrawArrays,(GLenum,GLint,GLsizei));
GL_API_DEF(void,ActiveTexture,(GLenum)); GL_API_DEF(void,TexBuffer,(GLenum,GLenum,GLuint));
GL_API_DEF(void,CompressedTexImage2D,(GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const void*));
GL_API_DEF(void,GenFramebuffers,(GLsizei,GLuint*)); GL_API_DEF(void,BindFramebuffer,(GLenum,GLuint));
GL_API_DEF(void,DeleteFramebuffers,(GLsizei,const GLuint*)); GL_API_DEF(void,FramebufferTexture2D,(GLenum,GLenum,GLenum,GLuint,GLint));
GL_API_DEF(GLenum,CheckFramebufferStatus,(GLenum)); GL_API_DEF(void,GenRenderbuffers,(GLsizei,GLuint*));
GL_API_DEF(void,BindRenderbuffer,(GLenum,GLuint)); GL_API_DEF(void,DeleteRenderbuffers,(GLsizei,const GLuint*));
GL_API_DEF(void,RenderbufferStorage,(GLenum,GLenum,GLsizei,GLsizei));
GL_API_DEF(void,RenderbufferStorageMultisample,(GLenum,GLsizei,GLenum,GLsizei,GLsizei));
GL_API_DEF(void,FramebufferRenderbuffer,(GLenum,GLenum,GLenum,GLuint));
GL_API_DEF(void,BlitFramebuffer,(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum));
GL_API_DEF(void,GenerateMipmap,(GLenum));
GL_API_DEF(void,BlendFuncSeparate,(GLenum,GLenum,GLenum,GLenum));
#undef GL_API_DEF

bool load() {
#define LOAD(name) name=reinterpret_cast<decltype(name)>(glfwGetProcAddress("gl" #name)); if(!name)return false
    LOAD(GenVertexArrays); LOAD(BindVertexArray); LOAD(DeleteVertexArrays); LOAD(GenBuffers); LOAD(BindBuffer);
    LOAD(BufferData); LOAD(BufferSubData); LOAD(DeleteBuffers); LOAD(EnableVertexAttribArray); LOAD(VertexAttribPointer); LOAD(VertexAttribIPointer);
    LOAD(CreateShader); LOAD(ShaderSource); LOAD(CompileShader); LOAD(GetShaderiv); LOAD(GetShaderInfoLog); LOAD(DeleteShader);
    LOAD(CreateProgram); LOAD(AttachShader); LOAD(LinkProgram); LOAD(GetProgramiv); LOAD(GetProgramInfoLog); LOAD(DeleteProgram);
    LOAD(UseProgram); LOAD(GetUniformLocation); LOAD(UniformMatrix4fv); LOAD(Uniform4f); LOAD(Uniform3f); LOAD(Uniform2f); LOAD(Uniform1i); LOAD(Uniform1iv); LOAD(Uniform1f);
    LOAD(DrawElements); LOAD(DrawArrays); LOAD(ActiveTexture); LOAD(TexBuffer); LOAD(CompressedTexImage2D);
    LOAD(GenFramebuffers); LOAD(BindFramebuffer); LOAD(DeleteFramebuffers); LOAD(FramebufferTexture2D); LOAD(CheckFramebufferStatus);
    LOAD(GenRenderbuffers); LOAD(BindRenderbuffer); LOAD(DeleteRenderbuffers); LOAD(RenderbufferStorage); LOAD(RenderbufferStorageMultisample); LOAD(FramebufferRenderbuffer); LOAD(BlitFramebuffer);
    LOAD(GenerateMipmap);
    LOAD(BlendFuncSeparate);
#undef LOAD
    return true;
}

} // namespace glapi
