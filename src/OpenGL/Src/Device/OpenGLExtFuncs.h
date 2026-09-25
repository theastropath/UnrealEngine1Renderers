/*=============================================================================
	OpenGLExtFuncs.h: extension entry point declarations.
	Portions copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:

=============================================================================*/

/*-----------------------------------------------------------------------------
	OpenGL extensions.
-----------------------------------------------------------------------------*/

// BGRA texture uploads.
GL_EXT_NAME(_GL_EXT_bgra)

// Paletted texture uploads.
GL_EXT_NAME(_GL_EXT_paletted_texture)
GL_EXT_PROC(_GL_EXT_paletted_texture, void, glColorTableEXT, (GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const void *data))
GL_EXT_PROC(_GL_EXT_paletted_texture, void, glColorSubTableEXT, (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const void *data))
GL_EXT_PROC(_GL_EXT_paletted_texture, void, glGetColorTableEXT, (GLenum target, GLenum format, GLenum type, void *data))
GL_EXT_PROC(_GL_EXT_paletted_texture, void, glGetColorTableParameterivEXT, (GLenum target, GLenum pname, int *params))
GL_EXT_PROC(_GL_EXT_paletted_texture, void, glGetColorTableParameterfvEXT, (GLenum target, GLenum pname, float *params))

// Generic texture compression.
GL_EXT_NAME(_GL_ARB_texture_compression)
GL_EXT_PROC(_GL_ARB_texture_compression, void, glCompressedTexSubImage2DARB, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid *))
GL_EXT_PROC(_GL_ARB_texture_compression, void, glCompressedTexImage2DARB, (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *))

// S3TC texture compression.
GL_EXT_NAME(_GL_EXT_texture_compression_s3tc)

// Detail texture combiners.
GL_EXT_NAME(_GL_EXT_texture_env_combine)
GL_EXT_NAME(_GL_ARB_texture_env_combine)

// Anisotropic texture filtering.
GL_EXT_NAME(_GL_EXT_texture_filter_anisotropic)

// Unpadded capture for gamma.
GL_EXT_NAME(_GL_ARB_texture_non_power_of_two)

// Single pass fog.
GL_EXT_NAME(_GL_ATI_texture_env_combine3)
GL_EXT_NAME(_GL_NV_texture_env_combine4)

// Texture LOD bias.
GL_EXT_NAME(_GL_EXT_texture_lod_bias)

// Secondary color arrays.
GL_EXT_NAME(_GL_EXT_secondary_color)
GL_EXT_PROC(_GL_EXT_secondary_color, void, glSecondaryColorPointerEXT, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))

// ARB multitexture units.
GL_EXT_NAME(_GL_ARB_multitexture)
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord1fARB, (GLenum target, GLfloat))
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord2fARB, (GLenum target, GLfloat, GLfloat))
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord3fARB, (GLenum target, GLfloat, GLfloat, GLfloat))
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord4fARB, (GLenum target, GLfloat, GLfloat, GLfloat, GLfloat))
//These fv entries took by-value floats.
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord1fvARB, (GLenum target, const GLfloat *v))
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord2fvARB, (GLenum target, const GLfloat *v))
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord3fvARB, (GLenum target, const GLfloat *v))
GL_EXT_PROC(_GL_ARB_multitexture, void, glMultiTexCoord4fvARB, (GLenum target, const GLfloat *v))
GL_EXT_PROC(_GL_ARB_multitexture, void, glActiveTextureARB, (GLenum target))
GL_EXT_PROC(_GL_ARB_multitexture, void, glClientActiveTextureARB, (GLenum target))

// Multi draw arrays.
//Const matches the canonical prototypes.
GL_EXT_NAME(_GL_EXT_multi_draw_arrays)
GL_EXT_PROC(_GL_EXT_multi_draw_arrays, void, glMultiDrawArraysEXT, (GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount))
GL_EXT_PROC(_GL_EXT_multi_draw_arrays, void, glMultiDrawElementsEXT, (GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei primcount))

// Streaming vertex buffers.
GL_EXT_NAME(_GL_ARB_vertex_buffer_object)
GL_EXT_PROC(_GL_ARB_vertex_buffer_object, void, glBindBufferARB, (GLenum target, GLuint buffer))
GL_EXT_PROC(_GL_ARB_vertex_buffer_object, void, glDeleteBuffersARB, (GLsizei n, const GLuint *buffers))
GL_EXT_PROC(_GL_ARB_vertex_buffer_object, void, glGenBuffersARB, (GLsizei n, GLuint *buffers))
GL_EXT_PROC(_GL_ARB_vertex_buffer_object, void, glBufferDataARB, (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage))
GL_EXT_PROC(_GL_ARB_vertex_buffer_object, void, glBufferSubDataARB, (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data))

// ARB vertex programs.
GL_EXT_NAME(_GL_ARB_vertex_program)
GL_EXT_PROC(_GL_ARB_vertex_program, void, glProgramStringARB, (GLenum target, GLenum format, GLsizei len, const GLvoid *string))
GL_EXT_PROC(_GL_ARB_vertex_program, void, glBindProgramARB, (GLenum target, GLuint program))
GL_EXT_PROC(_GL_ARB_vertex_program, void, glDeleteProgramsARB, (GLsizei n, const GLuint *programs))
GL_EXT_PROC(_GL_ARB_vertex_program, void, glGenProgramsARB, (GLsizei n, GLuint *programs))

GL_EXT_PROC(_GL_ARB_vertex_program, void, glProgramEnvParameter4fARB, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))
GL_EXT_PROC(_GL_ARB_vertex_program, void, glProgramLocalParameter4fARB, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))

//Detects programs the driver would emulate.
GL_EXT_PROC(_GL_ARB_vertex_program, void, glGetProgramivARB, (GLenum target, GLenum pname, GLint *params))

GL_EXT_PROC(_GL_ARB_vertex_program, void, glVertexAttrib4fARB, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))

// ARB fragment programs.
GL_EXT_NAME(_GL_ARB_fragment_program)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
