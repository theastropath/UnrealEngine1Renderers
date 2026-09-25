#pragma once

struct FGLVertex {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

struct FGLTexCoord {
	FLOAT u;
	FLOAT v;
};

struct FGLSingleColor {
	DWORD color;
};
struct FGLDoubleColor {
	DWORD color;
	DWORD specular;
};
struct FGLColorAlloc {
	union {
		FGLSingleColor singleColor;
		FGLDoubleColor doubleColor;
	};
};

struct FGLMapDot {
	FLOAT u;
	FLOAT v;
};
