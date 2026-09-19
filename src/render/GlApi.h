#pragma once

#include <GLFW/glfw3.h>
#include <cstddef>

namespace glapi {

using Size = std::ptrdiff_t;
using Char = char;

inline constexpr GLenum ArrayBuffer=0x8892, ElementArrayBuffer=0x8893, TextureBuffer=0x8C2A;
inline constexpr GLenum StaticDraw=0x88E4, DynamicDraw=0x88E8;
inline constexpr GLenum VertexShader=0x8B31, FragmentShader=0x8B30, CompileStatus=0x8B81, LinkStatus=0x8B82;
inline constexpr GLenum Framebuffer=0x8D40, ReadFramebuffer=0x8CA8, DrawFramebuffer=0x8CA9, Renderbuffer=0x8D41, ColorAttachment0=0x8CE0;
inline constexpr GLenum DepthStencilAttachment=0x821A, Depth24Stencil8=0x88F0, FramebufferComplete=0x8CD5;
inline constexpr GLenum DepthAttachment=0x8D00,DepthComponent24=0x81A6,DepthComponent32f=0x8CAC,ClampToBorder=0x812D,ClampToEdge=0x812F;
inline constexpr GLenum Rgba8=0x8058, Rgba16f=0x881A, Rgb16f=0x881B, Srgb8Alpha8=0x8C43, Rgba32f=0x8814, R32f=0x822E, Texture0=0x84C0;

extern void (APIENTRY* GenVertexArrays)(GLsizei,GLuint*);
extern void (APIENTRY* BindVertexArray)(GLuint);
extern void (APIENTRY* DeleteVertexArrays)(GLsizei,const GLuint*);
extern void (APIENTRY* GenBuffers)(GLsizei,GLuint*);
extern void (APIENTRY* BindBuffer)(GLenum,GLuint);
extern void (APIENTRY* BufferData)(GLenum,Size,const void*,GLenum);
extern void (APIENTRY* BufferSubData)(GLenum,Size,Size,const void*);
extern void (APIENTRY* DeleteBuffers)(GLsizei,const GLuint*);
extern void (APIENTRY* EnableVertexAttribArray)(GLuint);
extern void (APIENTRY* VertexAttribPointer)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
extern void (APIENTRY* VertexAttribIPointer)(GLuint,GLint,GLenum,GLsizei,const void*);
extern GLuint (APIENTRY* CreateShader)(GLenum);
extern void (APIENTRY* ShaderSource)(GLuint,GLsizei,const Char* const*,const GLint*);
extern void (APIENTRY* CompileShader)(GLuint);
extern void (APIENTRY* GetShaderiv)(GLuint,GLenum,GLint*);
extern void (APIENTRY* GetShaderInfoLog)(GLuint,GLsizei,GLsizei*,Char*);
extern void (APIENTRY* DeleteShader)(GLuint);
extern GLuint (APIENTRY* CreateProgram)();
extern void (APIENTRY* AttachShader)(GLuint,GLuint);
extern void (APIENTRY* LinkProgram)(GLuint);
extern void (APIENTRY* GetProgramiv)(GLuint,GLenum,GLint*);
extern void (APIENTRY* GetProgramInfoLog)(GLuint,GLsizei,GLsizei*,Char*);
extern void (APIENTRY* DeleteProgram)(GLuint);
extern void (APIENTRY* UseProgram)(GLuint);
extern GLint (APIENTRY* GetUniformLocation)(GLuint,const Char*);
extern void (APIENTRY* UniformMatrix4fv)(GLint,GLsizei,GLboolean,const GLfloat*);
extern void (APIENTRY* Uniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
extern void (APIENTRY* Uniform3f)(GLint,GLfloat,GLfloat,GLfloat);
extern void (APIENTRY* Uniform2f)(GLint,GLfloat,GLfloat);
extern void (APIENTRY* Uniform1i)(GLint,GLint);
extern void (APIENTRY* Uniform1iv)(GLint,GLsizei,const GLint*);
extern void (APIENTRY* Uniform1f)(GLint,GLfloat);
extern void (APIENTRY* DrawElements)(GLenum,GLsizei,GLenum,const void*);
extern void (APIENTRY* DrawArrays)(GLenum,GLint,GLsizei);
extern void (APIENTRY* ActiveTexture)(GLenum);
extern void (APIENTRY* TexBuffer)(GLenum,GLenum,GLuint);
extern void (APIENTRY* CompressedTexImage2D)(GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const void*);
extern void (APIENTRY* GenFramebuffers)(GLsizei,GLuint*);
extern void (APIENTRY* BindFramebuffer)(GLenum,GLuint);
extern void (APIENTRY* DeleteFramebuffers)(GLsizei,const GLuint*);
extern void (APIENTRY* FramebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint);
extern GLenum (APIENTRY* CheckFramebufferStatus)(GLenum);
extern void (APIENTRY* GenRenderbuffers)(GLsizei,GLuint*);
extern void (APIENTRY* BindRenderbuffer)(GLenum,GLuint);
extern void (APIENTRY* DeleteRenderbuffers)(GLsizei,const GLuint*);
extern void (APIENTRY* RenderbufferStorage)(GLenum,GLenum,GLsizei,GLsizei);
extern void (APIENTRY* RenderbufferStorageMultisample)(GLenum,GLsizei,GLenum,GLsizei,GLsizei);
extern void (APIENTRY* FramebufferRenderbuffer)(GLenum,GLenum,GLenum,GLuint);
extern void (APIENTRY* BlitFramebuffer)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
extern void (APIENTRY* GenerateMipmap)(GLenum);
extern void (APIENTRY* BlendFuncSeparate)(GLenum,GLenum,GLenum,GLenum);

[[nodiscard]] bool load();

} // namespace glapi
