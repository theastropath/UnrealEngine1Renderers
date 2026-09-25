#pragma once

#include <windows.h>


struct Vec2 {
	float x, y;
};

struct Vec3 {
	float x, y, z;
};

struct Vec4 {
	Vec4(float x, float y, float z, float w) :
		x(x),
		y(y),
		z(z),
		w(w) {
	}
	float x, y, z, w;
};

struct Vec4_byte {
	BYTE x, y, z, w;
};

struct Vec2_int {
	int x, y;
};


struct Vertex_GouraudPolygon {
	Vec3 Pos;
	Vec4 Color;
	Vec4 Fog;
	Vec2 TexCoord;
	DWORD flags;
	char padding[4];
};

struct Vertex_ComplexSurface {
	Vec3 Pos;
	Vec2 TexCoord[5];
	DWORD flags;
	DWORD texturePasses;
};

struct Vertex_Tile {
	Vec4 XYWH;
	Vec4 UVWH;
	Vec4 Color;
	float z;
	DWORD flags;
	char padding[4];
};

struct Vertex_FogSurface {
	Vec3 Pos;
	Vec4 Color;
	DWORD flags;
	char padding[28];
};

struct Vertex_Line {
	Vec4 XYXY;
	Vec4 Color;
	float z;
	DWORD flags;
	DWORD isRect;
	char padding[16];
};

struct Vertex_Simple {
	Vec3 Pos;
	//Vec2 Tex;
};

static_assert(sizeof(Vertex_GouraudPolygon) == sizeof(Vertex_ComplexSurface), "Vertex_GouraudPolygon must match the shared geometry buffer's stride; adjust its padding.");
static_assert(sizeof(Vertex_Tile) == sizeof(Vertex_ComplexSurface), "Vertex_Tile must match the shared geometry buffer's stride; adjust its padding.");
static_assert(sizeof(Vertex_FogSurface) == sizeof(Vertex_ComplexSurface), "Vertex_FogSurface must match the shared geometry buffer's stride; adjust its padding.");
static_assert(sizeof(Vertex_Line) == sizeof(Vertex_ComplexSurface), "Vertex_Line must match the shared geometry buffer's stride; adjust its padding.");
