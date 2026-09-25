//Complex surface pixel shaders for ReduceBanding's seven bit map reconstruction: the same combiners as the
//hand written ones, with the lightmap and the fog map reconstructed and dithered.
//See the reconstruction below for why.
//
//Compiled offline, one entry point at a time:
//
//fxc /nologo /T ps_3_0 /E PS_DualModulatedLight /O3 /Fo out.fxo complexsurface.recon.ps.hlsl
//blobify.ps1 -Path out.fxo -Name g_fpComplexSurfaceDualTextureModulatedRecon
//
//The /Fo flag is required. The full set is in regen-recon-shaders.ps1 beside this file.
//
//There are no literal constants anywhere below, and there must not be: this target has nowhere to put
//one but a constant register.

//c0.x is the alpha test level and c0.a the lightmap blend scale factor, both set per frame; the rest of c0
//is unused.
float4 blendInfo : register(c0);

//Texel dimensions of the seven bit map bound to layers 1, 2 and 3, as (width, height, 1/width, 1/height).
//Set where a batch binds its layers.
float4 mapSize1 : register(c7);
float4 mapSize2 : register(c8);
float4 mapSize3 : register(c9);

/*
The reconstruction's own constants, in the order the driver writes them: the cubic B-spline's coefficients
(0.5, 1.0, 2/3, 1/6); then its last coefficient, the far tap's base offset, the dither's doubling and
fade range (1/3, 1.5, 2.0, 1/oneCode); then interleaved gradient noise as the gamma pass uses it, with
the step these maps quantize to; then the expansion left over after the upload's own, with a zero
beside it; and last the detail fade distance, threshold and layer scale (1/380, 0.999, 4.223).
*/
float4 splineA : register(c10);
float4 splineB : register(c11);
float4 ditherInfo : register(c12);
float4 expandInfo : register(c13);
float4 detailInfo : register(c14);

sampler2D texLayer0 : register(s0);
sampler2D texLayer1 : register(s1);
sampler2D texLayer2 : register(s2);
sampler2D texLayer3 : register(s3);

struct PS_IN {
	float2 t0 : TEXCOORD0;
	float2 t1 : TEXCOORD1;
	float2 t2 : TEXCOORD2;
	float2 t3 : TEXCOORD3;
	//Pixel position, for the dither. Deliberately not animated, so it does not shimmer.
	float2 vPos : VPOS;
};

//The detail combiners want the vertex colour and the vertex's own position.
struct PS_IN_DETAIL {
	float4 color : COLOR0;
	float2 t0 : TEXCOORD0;
	float2 t1 : TEXCOORD1;
	float4 t2 : TEXCOORD2;
	float4 objPos : TEXCOORD4;
	float2 vPos : VPOS;
};

/*
Reconstructs a lightmap or fog map the engine hands over at roughly one texel per 32 world units and
seven bits per channel. Both of those read as banding: bilinear magnification is continuous but creases at
every texel centre, where the gradient changes from one pair of texels to the next, and seven bits leaves
the smallest step the light can take at twice the one the frame buffer could have shown, so a gradient the
engine quantized flat arrives as a plateau.

A cubic B-spline answers the first, being smooth across texel borders, and a triangular dither of one source
code answers the second.
*/
float4 sampleSevenBitMap(sampler2D tex, float2 uv, float4 mapSize, float2 vPos) {
	const float half = splineA.x;
	const float one = splineA.y;
	const float2 size = mapSize.xy;
	const float2 invSize = mapSize.zw;

	//Texel centres land on whole numbers in this space.
	const float2 coord = uv * size - half;
	const float2 base = floor(coord);
	const float2 f = coord - base;
	const float2 f2 = f * f;
	const float2 f3 = f2 * f;

	//The B-spline's second and fourth weights and the sums of each neighbouring pair.
	const float2 w1 = half * f3 - f2 + splineA.z;
	const float2 w3 = splineA.w * f3;
	const float2 g1 = splineA.w + half * (f2 + f) - splineB.x * f3;
	const float2 g0 = one - g1;

	/*
	Each neighbouring pair is fetched by one bilinear tap placed between them, so the hardware's own
	interpolation produces the pair's weighted sum and the pair weights then blend the two taps, all
	landing in the same texels and so the same cache lines at these magnifications. Both pair weights stay
	at or above a sixth over the whole range of f, so neither division is by zero.

	Clamped to the first and last texel centres, since the sampler addresses these wrapped. A tap reaches
	BICUBIC_TEXEL_REACH texels past the coordinate it was given.
	*/
	const float2 lo = half;
	const float2 hi = size - half;
	const float2 tap0 = clamp(base - half + w1 / g0, lo, hi) * invSize;
	const float2 tap1 = clamp(base + splineB.y + w3 / g1, lo, hi) * invSize;

	const float4 row0 = lerp(tex2D(tex, float2(tap0.x, tap0.y)), tex2D(tex, float2(tap1.x, tap0.y)), g1.x);
	const float4 row1 = lerp(tex2D(tex, float2(tap0.x, tap1.y)), tex2D(tex, float2(tap1.x, tap1.y)), g1.x);
	const float4 reconstructed = lerp(row0, row1, g1.y);

	/*
	One source code. Applied here and not to the finished surface, which is about to be multiplied into a
	texture and run through the brightness curve, since dithering after either of those would have to undo
	what they did to the step size.

	Triangular, by the inverse CDF of a triangular distribution. This target's conditional move tests
	for free. Faded in over the first code so an unlit surface is not lifted into speckle, and saturated.
	*/
	const float noise = frac(ditherInfo.z * frac(dot(vPos, ditherInfo.xy) + expandInfo.y));
	const float offset = noise - half;
	const float root = sqrt(saturate(one - splineB.z * abs(offset)));
	const float triangular = (offset >= 0) ? (one - root) : (root - one);
	const float4 dithered = saturate(reconstructed + triangular * ditherInfo.w);
	const float brightest = max(reconstructed.r, max(reconstructed.g, reconstructed.b));

	const float4 result = lerp(reconstructed, dithered, saturate(brightest * splineB.w));

	/*
	The expansion left over after the upload's own, which doubling is not: 127 is everything a seven bit
	channel can carry, so it has to arrive as 255 where doubling leaves it at 254. Applied once per map.
	*/
	return saturate(result * expandInfo.x);
}

//The alpha test. Applied after the modulated layers and before the fog map in every combiner.
void alphaTest(float alpha) {
	clip(alpha - blendInfo.x);
}

//The fog map layer, which replaces and not brightens: the highlighted blend of the fixed function path.
float3 applyFog(float3 color, float4 fog) {
	return color * (splineA.y - fog.a) + fog.rgb;
}


//--- Base texture and a reconstructed lightmap -------------------------------
float4 PS_DualModulatedLight(PS_IN i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 light = sampleSevenBitMap(texLayer1, i.t1, mapSize1, i.vPos);

	color = color * light;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);
	return color;
}

//--- Base texture, a macro texture and a reconstructed lightmap --------------
float4 PS_TripleModulatedLight(PS_IN i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 macro = tex2D(texLayer1, i.t1);
	const float4 light = sampleSevenBitMap(texLayer2, i.t2, mapSize2, i.vPos);

	color = color * macro;
	color.rgb = color.rgb * blendInfo.a;

	color = color * light;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);
	return color;
}

//--- Base texture and a reconstructed fog map --------------------------------
float4 PS_SingleWithFog(PS_IN i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 fog = sampleSevenBitMap(texLayer1, i.t1, mapSize1, i.vPos);

	alphaTest(color.a);

	color.rgb = applyFog(color.rgb, fog);
	return color;
}

//--- Base texture, a reconstructed lightmap and a reconstructed fog map ------
float4 PS_DualModulatedLightWithFog(PS_IN i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 light = sampleSevenBitMap(texLayer1, i.t1, mapSize1, i.vPos);
	const float4 fog = sampleSevenBitMap(texLayer2, i.t2, mapSize2, i.vPos);

	color = color * light;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);

	color.rgb = applyFog(color.rgb, fog);
	return color;
}

//--- Base texture, a macro texture and a reconstructed fog map ---------------
//The macro texture here keeps bilinear sampling: it is minified.
float4 PS_DualModulatedMacroWithFog(PS_IN i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 macro = tex2D(texLayer1, i.t1);
	const float4 fog = sampleSevenBitMap(texLayer2, i.t2, mapSize2, i.vPos);

	color = color * macro;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);

	color.rgb = applyFog(color.rgb, fog);
	return color;
}

//--- Base, macro, reconstructed lightmap and reconstructed fog map -----------
float4 PS_TripleModulatedLightWithFog(PS_IN i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 macro = tex2D(texLayer1, i.t1);
	const float4 light = sampleSevenBitMap(texLayer2, i.t2, mapSize2, i.vPos);
	const float4 fog = sampleSevenBitMap(texLayer3, i.t3, mapSize3, i.vPos);

	color = color * macro;
	color.rgb = color.rgb * blendInfo.a;

	color = color * light;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);

	color.rgb = applyFog(color.rgb, fog);
	return color;
}

//One layer of the detail pass, which fades into the vertex colour with distance.
//Doubled afterwards for the fixed function modulation.
float3 applyDetail(float3 color, float3 vertexColor, float3 detail, float fade) {
	const float3 blended = lerp(detail, vertexColor, fade);
	return (color * blended) + (color * blended);
}

//--- Base texture, a reconstructed lightmap and a detail texture -------------
//Detail and a fog map are mutually exclusive.
float4 PS_DualModulatedLightAndDetail(PS_IN_DETAIL i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 light = sampleSevenBitMap(texLayer1, i.t1, mapSize1, i.vPos);

	color = color * light;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);

	const float fade = saturate(i.objPos.z * detailInfo.x);
	if (fade <= detailInfo.y) {
		color.rgb = applyDetail(color.rgb, i.color.rgb, tex2D(texLayer2, i.t2.xy).rgb, fade);
	}
	return color;
}

//--- The same, with the detail texture's second, coarser layer ---------------
float4 PS_DualModulatedLightAndDetailTwoLayer(PS_IN_DETAIL i) : COLOR0 {
	float4 color = tex2D(texLayer0, i.t0);
	const float4 light = sampleSevenBitMap(texLayer1, i.t1, mapSize1, i.vPos);

	color = color * light;
	color.rgb = color.rgb * blendInfo.a;

	alphaTest(color.a);

	float fade = saturate(i.objPos.z * detailInfo.x);
	if (fade <= detailInfo.y) {
		color.rgb = applyDetail(color.rgb, i.color.rgb, tex2D(texLayer2, i.t2.xy).rgb, fade);

		fade = saturate(fade * detailInfo.z);
		color.rgb = applyDetail(color.rgb, i.color.rgb, tex2D(texLayer2, i.t2.zw).rgb, fade);
	}
	return color;
}
