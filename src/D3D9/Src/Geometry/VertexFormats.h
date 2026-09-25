#pragma once


struct FGLVertex {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

struct FGLVertexColor {
	FLOAT x;
	FLOAT y;
	FLOAT z;
	DWORD color;
};

struct FGLTexCoord {
	FLOAT u;
	FLOAT v;
};

struct FGLSecondaryColor {
	DWORD specular;
};

struct FGLMapDot {
	FLOAT u;
	FLOAT v;
};
