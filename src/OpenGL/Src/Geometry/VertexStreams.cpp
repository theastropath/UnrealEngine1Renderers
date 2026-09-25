/*=============================================================================
	VertexStreams.cpp: the buffer objects the arrays upload through.

	One stream per attribute.
	Each ring is orphaned and rewound when it fills.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::InitVertexStreams(void) {
	guard(UOpenGLRenderDevice::InitVertexStreams);

	unsigned int u;

	//Re-entrant.
	//Must precede the describe loop.
	ShutdownVertexStreams();

	//Describe every stream first, so the fallback path has valid pointers.
	//Each ring is sized for the widest stride its stream can carry.
	for (u = 0; u < NUM_VERTEX_STREAMS; u++) {
		m_vertexStreams[u].BufferId = 0;
		m_vertexStreams[u].RingPosBytes = 0;
		m_vertexStreams[u].BaseByteOffset = 0;
		m_vertexStreams[u].pCpuData = NULL;
		m_vertexStreams[u].PendingStride = 0;
		m_vertexStreams[u].PendingCount = 0;
	}
	m_vertexStreams[VERTEX_STREAM_VERTEX].CapacityBytes = VERTEX_STREAM_RING_VERTS * sizeof(FGLVertex);
	m_vertexStreams[VERTEX_STREAM_COLOR].CapacityBytes = VERTEX_STREAM_RING_VERTS * sizeof(FGLColorAlloc);
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_vertexStreams[VERTEX_STREAM_TEXCOORD0 + u].CapacityBytes = VERTEX_STREAM_RING_VERTS * sizeof(FGLTexCoord);
	}

	m_vboActive = false;
	m_curArrayBuffer = 0;
	m_colorStreamIsDouble = false;

	if (!UseVBO) {
		debugf(TEXT("Geometry streaming: client arrays (UseVBO off or GL_ARB_vertex_buffer_object missing)"));
		return;
	}

	/*
	One buffer per stream, because sharing would force every stream to advance together and
	kill the reuse of positions across a facet's passes, and the array is zeroed up front
	since a partial allocation leaves entries a rollback would hand to the delete call as if
	they named live buffers, glDeleteBuffers on a stale name being silently ignored.
	*/
	GLuint bufferIds[NUM_VERTEX_STREAMS] = { 0 };
	glGenBuffersARB(NUM_VERTEX_STREAMS, bufferIds);
	for (u = 0; u < NUM_VERTEX_STREAMS; u++) {
		if (bufferIds[u] == 0) {
			//Back to staging.
			debugf(TEXT("Geometry streaming: client arrays (buffer object allocation failed)"));
			glDeleteBuffersARB(NUM_VERTEX_STREAMS, bufferIds);
			for (unsigned int r = 0; r < NUM_VERTEX_STREAMS; r++) {
				m_vertexStreams[r].BufferId = 0;
			}
			return;
		}
		m_vertexStreams[u].BufferId = bufferIds[u];
	}

	for (u = 0; u < NUM_VERTEX_STREAMS; u++) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m_vertexStreams[u].BufferId);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
			(GLsizeiptrARB)m_vertexStreams[u].CapacityBytes, NULL, GL_STREAM_DRAW_ARB);
	}
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	m_vboActive = true;

	debugf(TEXT("Geometry streaming: vertex buffer objects"));

	unguard;
}

void UOpenGLRenderDevice::ShutdownVertexStreams(void) {
	guard(UOpenGLRenderDevice::ShutdownVertexStreams);

	if (m_vertexStreams[VERTEX_STREAM_VERTEX].BufferId == 0) {
		return;
	}

	//Back to client memory.
	m_vboActive = false;
	BindArrayBuffer(0);

	GLuint bufferIds[NUM_VERTEX_STREAMS];
	for (unsigned int u = 0; u < NUM_VERTEX_STREAMS; u++) {
		bufferIds[u] = m_vertexStreams[u].BufferId;
		m_vertexStreams[u].BufferId = 0;
	}
	glDeleteBuffersARB(NUM_VERTEX_STREAMS, bufferIds);

	unguard;
}

void UOpenGLRenderDevice::SetVertexStreamPointers(void) {
	unsigned int u;

//In buffer mode each pointer is an offset into that stream's buffer.
//Draw offsets are unchanged either way.
#define UTGLR_STREAM_OFFSET(stream, cpuPtr) \
	(m_vboActive \
			? (const void *)((BYTE *)NULL + m_vertexStreams[stream].BaseByteOffset) \
			: (const void *)(cpuPtr))

	if (m_vboActive) {
		BindArrayBuffer(m_vertexStreams[VERTEX_STREAM_VERTEX].BufferId);
	} else {
		BindArrayBuffer(0);
	}
	glVertexPointer(3, GL_FLOAT, sizeof(FGLVertex), UTGLR_STREAM_OFFSET(VERTEX_STREAM_VERTEX, &VertexArray[0].x));
	//Recorded so a later reset can tell.
	m_vertexArrayByteOffset = m_vboActive ? m_vertexStreams[VERTEX_STREAM_VERTEX].BaseByteOffset : 0;

	for (u = 0; u < (UseMultiTexture ? (unsigned int)TMUnits : 1u); u++) {
		SetClientActiveTexUnit(u);
		if (m_vboActive) {
			BindArrayBuffer(m_vertexStreams[VERTEX_STREAM_TEXCOORD0 + u].BufferId);
		}
		glTexCoordPointer(2, GL_FLOAT, sizeof(FGLTexCoord),
			UTGLR_STREAM_OFFSET(VERTEX_STREAM_TEXCOORD0 + u, &TexCoordArray[u][0].u));
	}
	SetClientActiveTexUnit(0);

	SetColorArrayPointer();

	if (UseVertexSpecular) {
		//The pointer skips to the specular half.
		if (m_vboActive) {
			BindArrayBuffer(m_vertexStreams[VERTEX_STREAM_COLOR].BufferId);
			glSecondaryColorPointerEXT(3, GL_UNSIGNED_BYTE, sizeof(FGLDoubleColor),
				(GLvoid *)((BYTE *)NULL + m_vertexStreams[VERTEX_STREAM_COLOR].BaseByteOffset + offsetof(FGLDoubleColor, specular)));
		} else {
			BindArrayBuffer(0);
			glSecondaryColorPointerEXT(3, GL_UNSIGNED_BYTE, sizeof(FGLDoubleColor),
				(GLvoid *)&DoubleColorArray[0].specular);
		}
	}

#undef UTGLR_STREAM_OFFSET
}

void UOpenGLRenderDevice::SetColorArrayPointer(void) {
	//Same base, different stride.
	const void *pOffset;
	if (m_vboActive) {
		BindArrayBuffer(m_vertexStreams[VERTEX_STREAM_COLOR].BufferId);
		pOffset = (const void *)((BYTE *)NULL + m_vertexStreams[VERTEX_STREAM_COLOR].BaseByteOffset);
	} else {
		BindArrayBuffer(0);
		pOffset = m_colorStreamIsDouble
			? (const void *)&DoubleColorArray[0].color
			: (const void *)&SingleColorArray[0].color;
	}

	if (m_colorStreamIsDouble) {
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FGLDoubleColor), pOffset);
	} else {
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FGLSingleColor), pOffset);
	}
}

void UOpenGLRenderDevice::UploadVertexStream(DWORD stream, bool repoint) {
	FGLVertexStream &s = m_vertexStreams[stream];

	//An unstaged stream is still valid from the last draw.
	if (s.pCpuData == NULL || s.PendingCount == 0) {
		return;
	}

	DWORD sizeBytes = s.PendingCount * s.PendingStride;

	BindArrayBuffer(s.BufferId);

	//Wrap before writing past the end.
	//Reallocating orphans storage the GPU may still be reading.
	if (s.RingPosBytes + sizeBytes > s.CapacityBytes) {
		s.RingPosBytes = 0;
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, (GLsizeiptrARB)s.CapacityBytes, NULL, GL_STREAM_DRAW_ARB);
	}

	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB,
		(GLintptrARB)s.RingPosBytes, (GLsizeiptrARB)sizeBytes, s.pCpuData);

	s.BaseByteOffset = s.RingPosBytes;
	/*
	Keeping the next write 16 byte aligned costs at most 47 bytes out of a 384 KB ring and
	one divide per upload, because the vertex size is no power of two and positions have to
	round to a whole vertex, and tracking a separate element count alongside the byte offset
	was tried and came out slower than the divide it was meant to replace.
	*/
	const DWORD alignBytes = (stream == VERTEX_STREAM_VERTEX) ? (DWORD)VERTEX_STREAM_POS_ALIGN : 16u;
	s.RingPosBytes += ((sizeBytes + (alignBytes - 1)) / alignBytes) * alignBytes;

	//Consumed.
	s.pCpuData = NULL;

	//The data moved.
	if (repoint) {
		RepointVertexStream(stream);
	}
}

void UOpenGLRenderDevice::SetVertexArrayBaseZero(void) {
	if (!m_vboActive || (m_vertexArrayByteOffset == 0)) {
		return;
	}

	//Only the batched paths move it.
	m_vertexArrayByteOffset = 0;
	BindArrayBuffer(m_vertexStreams[VERTEX_STREAM_VERTEX].BufferId);
	glVertexPointer(3, GL_FLOAT, sizeof(FGLVertex), (const void *)NULL);
}

void UOpenGLRenderDevice::RepointVertexStream(DWORD stream) {
	const void *pOffset = (const void *)((BYTE *)NULL + m_vertexStreams[stream].BaseByteOffset);

	BindArrayBuffer(m_vertexStreams[stream].BufferId);

	switch (stream) {
		case VERTEX_STREAM_VERTEX:
			glVertexPointer(3, GL_FLOAT, sizeof(FGLVertex), pOffset);
			m_vertexArrayByteOffset = m_vertexStreams[VERTEX_STREAM_VERTEX].BaseByteOffset;
			break;

		case VERTEX_STREAM_COLOR:
			//The secondary color rides in the specular member.
			SetColorArrayPointer();
			if (UseVertexSpecular) {
				glSecondaryColorPointerEXT(3, GL_UNSIGNED_BYTE, sizeof(FGLDoubleColor),
					(GLvoid *)((BYTE *)NULL + m_vertexStreams[VERTEX_STREAM_COLOR].BaseByteOffset + offsetof(FGLDoubleColor, specular)));
			}
			break;

		default: {
			//Back to unit 0.
			const DWORD unit = stream - VERTEX_STREAM_TEXCOORD0;
			SetClientActiveTexUnit(unit);
			glTexCoordPointer(2, GL_FLOAT, sizeof(FGLTexCoord), pOffset);
			SetClientActiveTexUnit(0);
			break;
		}
	}
}

void UOpenGLRenderDevice::UploadComplexSurfaceStreams(DWORD numTexCoordStreams) {
	if (!m_vboActive) {
		return;
	}

	DWORD numVerts = (DWORD)m_csPtCount;
	unsigned int u;

	//Positions are uploaded once for the whole chunk.
	//Texture coordinates are regenerated per pass.
	//No color stream, because one colour covers the surface.
	for (u = 0; u < numTexCoordStreams; u++) {
		StageVertexStream(VERTEX_STREAM_TEXCOORD0 + u, TexCoordArray[u], sizeof(FGLTexCoord), numVerts);
		UploadVertexStream(VERTEX_STREAM_TEXCOORD0 + u);
	}
}

void UOpenGLRenderDevice::UploadGouraudStreams(DWORD numVerts) {
	if (!m_vboActive || numVerts == 0) {
		return;
	}

	StageVertexStream(VERTEX_STREAM_VERTEX, VertexArray, sizeof(FGLVertex), numVerts);
	UploadVertexStream(VERTEX_STREAM_VERTEX);

	StageVertexStream(VERTEX_STREAM_TEXCOORD0, TexCoordArray[0], sizeof(FGLTexCoord), numVerts);
	UploadVertexStream(VERTEX_STREAM_TEXCOORD0);

	//One upload covers both specular halves.
	if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
		StageVertexStream(VERTEX_STREAM_COLOR, DoubleColorArray, sizeof(FGLDoubleColor), numVerts);
		UploadVertexStream(VERTEX_STREAM_COLOR);
	} else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
		StageVertexStream(VERTEX_STREAM_COLOR, SingleColorArray, sizeof(FGLSingleColor), numVerts);
		UploadVertexStream(VERTEX_STREAM_COLOR);
	}
}

void UOpenGLRenderDevice::UploadTileStreams(DWORD numVerts) {
	if (!m_vboActive || numVerts == 0) {
		return;
	}

	StageVertexStream(VERTEX_STREAM_VERTEX, VertexArray, sizeof(FGLVertex), numVerts);
	UploadVertexStream(VERTEX_STREAM_VERTEX);

	StageVertexStream(VERTEX_STREAM_TEXCOORD0, TexCoordArray[0], sizeof(FGLTexCoord), numVerts);
	UploadVertexStream(VERTEX_STREAM_TEXCOORD0);

	if (m_requestedColorFlags & CF_COLOR_ARRAY) {
		StageVertexStream(VERTEX_STREAM_COLOR, SingleColorArray, sizeof(FGLSingleColor), numVerts);
		UploadVertexStream(VERTEX_STREAM_COLOR);
	}
}
