//Instanced screen-space quad and line vertex shader for the deferred path. Compiled offline into the
//DWORD array form this driver stores shaders in:
//
//fxc /nologo /T vs_3_0 /E main /O3 /Fo quadinstance.fxo quadinstance.vs.hlsl
//blobify.ps1 -Path quadinstance.fxo -Name g_vpQuadInstance
//
//The /Fo flag is required.

row_major float4x4 proj : register(c0);

struct VS_IN {
	//Per-vertex unit topology: (u, v, 0, 1) for a quad corner; a line's two ends carry v == u.
	float4 corner : POSITION0;
	//Per-instance data
	float4 rect   : POSITION1;   //X1 Y1 X2 Y2
	float4 uvRect : TEXCOORD1;   //U1 V1 U2 V2
	float4 zp     : TEXCOORD2;   //Z1 Z2 - a 2D line's ends carry their own depth
	float4 color  : COLOR0;
};

struct VS_OUT {
	float4 pos   : POSITION;
	float4 color : COLOR0;
	float2 t0    : TEXCOORD0;
};

VS_OUT main(VS_IN i) {
	VS_OUT o;

	//w comes from the stream, there being nowhere on this target for a literal but a constant register.
	float4 p;
	p.x = i.rect.x + i.corner.x * (i.rect.z - i.rect.x);
	p.y = i.rect.y + i.corner.y * (i.rect.w - i.rect.y);
	//Along the segment for a line. Constant for a quad, whose two are equal
	p.z = i.zp.x + i.corner.x * (i.zp.y - i.zp.x);
	p.w = i.corner.w;

	o.pos = mul(proj, p);
	o.color = i.color;
	o.t0.x = i.uvRect.x + i.corner.x * (i.uvRect.z - i.uvRect.x);
	o.t0.y = i.uvRect.y + i.corner.y * (i.uvRect.w - i.uvRect.y);
	return o;
}
