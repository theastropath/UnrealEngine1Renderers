/**
\file unreal_pom.fx

Parallax occlusion mapping, from D3D9 example code.
*/


/* 21 samples resolve 512 steps. */
static const int g_nMinSamples = 5;
static const int g_nMaxSamples = 16;
static const int POM_REFINE_STEPS = 5;
/** A divisor, so float. */
static const float g_nLODThreshold = UNREAL_DETAIL_NEAR_Z;

static const float POM_MIN_VIEW_Z = 0.05f;
static const float POM_MAX_PARALLAX_LENGTH = 4.0f;
static const float POM_MIN_UV_AREA = 1e-8f;
static const float POM_MIN_CROSSING_DENOMINATOR = 1e-6f;

/**
Tangent space matrix for the pixel being shaded,
http://wiki.gamedev.net/index.php/D3DBook:(Lighting)_Per-Pixel_Lighting#Moving_From_Per-Vertex_To_Per-Pixel

\param pos View space position, so -pos is the view vector.
\param tc Coordinate of the layer being displaced.
*/
float3x3 tangentSpaceFromDerivatives(float3 pos, float2 tc)
{
	float3 A = ddx( pos );
	float3 B = ddy( pos );

	float2 P = ddx( tc );
	float2 Q = ddy( tc );

	// No UV area, no divide.
	float uvArea = P.x * Q.y - Q.x * P.y;
	if( abs( uvArea ) < POM_MIN_UV_AREA )
		uvArea = ( uvArea < 0.0f ) ? -POM_MIN_UV_AREA : POM_MIN_UV_AREA;
	float fraction = 1.0f / uvArea;

	float3 normal = normalize( cross( A, B ) );

	//The winding can leave the normal facing away from the eye.
	if( dot( normal, -pos ) < 0.0f )
		normal = -normal;

	float3 tangent = ( Q.y * A - P.y * B ) * fraction;
	float3 bitangent = ( P.x * B - Q.x * A ) * fraction;

	// Apply Gram-Schmidt orthogonalization
	tangent = tangent - dot( normal, tangent ) * normal;

//KENTIE: using cross product instead of Gram-Schmidt for binormal prevents seams/glitches
	bool rightHanded = (dot(cross(tangent, bitangent), normal) >= 0);
	bitangent = cross(normal,tangent);
	if(!rightHanded)
		bitangent*=-1;
//END KENTIE

	float3x3 tsMatrix;

	tsMatrix[0] = normalize(tangent);
	tsMatrix[1] = normalize(bitangent);
	tsMatrix[2] = normal;

	return tsMatrix;
}

/** Per pixel, alongside its tangent frame. */
float2 calcPOMVector(float3 vViewTS,float g_fHeightMapScale)
{
	float2 vParallaxOffsetTS;
	// Nothing to normalize.
	float2 vParallaxDirection = float2( 0, 0 );
	float fDirectionLength = length( vViewTS.xy );
	if( fDirectionLength > 1e-6f )
		vParallaxDirection = vViewTS.xy / fDirectionLength;

	float fLength         = length( vViewTS );
	float fViewZ          = max( vViewTS.z, POM_MIN_VIEW_Z );
	float fParallaxLength = sqrt( max( fLength * fLength - vViewTS.z * vViewTS.z, 0.0f ) ) / fViewZ;
	fParallaxLength       = min( fParallaxLength, POM_MAX_PARALLAX_LENGTH );

	// Compute the actual reverse parallax displacement vector:
	vParallaxOffsetTS = vParallaxDirection * fParallaxLength;

	vParallaxOffsetTS *= g_fHeightMapScale;
	return vParallaxOffsetTS;
}

/** Simplified POM shader */
float2 POM(float4 pos, float3 viewTS, float3 normal, float2 texCoord, float2 vParallaxOffsetTS, Texture2D tex)
{ 

   float3 vViewWS   = normalize( -pos.xyz );
   float3 vNormalWS = normalize( normal );

	  int nNumSteps = (int) lerp( g_nMaxSamples, g_nMinSamples, dot( vViewWS, vNormalWS ) );
	  float fDistanceFade = saturate( pos.z / max( g_nLODThreshold, 1.0f ) );
	  nNumSteps = (int) lerp( (float) nNumSteps, (float) g_nMinSamples, fDistanceFade );
	  nNumSteps = clamp( nNumSteps, 1, g_nMaxSamples );

	  float fCurrHeight = 0.0;
	  float fStepSize   = 1.0 / (float) nNumSteps;
	  float fPrevHeight = 1.0;

	  float2 vTexOffsetPerStep = fStepSize * vParallaxOffsetTS;
	  float2 vTexCurrentOffset = texCoord;
	  float  fCurrentBound     = 1.0;
	  float  fParallaxAmount   = 0.0;

	  float2 pt1 = 0;
	  float2 pt2 = 0;
	  float2 vOffset1 = 0;
	  float2 vOffset2 = 0;
	  bool   bFound = false;

	  // Coarse walk to the crossing
	  [loop] for ( int nStepIndex = 0; nStepIndex < nNumSteps; nStepIndex++ )
	  {
		 vTexCurrentOffset -= vTexOffsetPerStep;

		 // A .height texture, or detail.
		fCurrHeight = tex.SampleLevel(samLinear,vTexCurrentOffset, 0).r;
		

		 fCurrentBound -= fStepSize;

		 if ( fCurrHeight > fCurrentBound ) 
		 {   
			pt1 = float2( fCurrentBound, fCurrHeight );
			pt2 = float2( fCurrentBound + fStepSize, fPrevHeight );

			vOffset1 = vTexCurrentOffset;
			vOffset2 = vTexCurrentOffset + vTexOffsetPerStep;

			bFound = true;
			break;
		 }
		 fPrevHeight = fCurrHeight;
	  }   

	  if ( bFound )
	  {
		 [unroll] for ( int nRefineIndex = 0; nRefineIndex < POM_REFINE_STEPS; nRefineIndex++ )
		 {
			float2 vMidOffset = 0.5f * ( vOffset1 + vOffset2 );
			float  fMidBound  = 0.5f * ( pt1.x + pt2.x );
			float  fMidHeight = tex.SampleLevel(samLinear,vMidOffset, 0).r;

			if ( fMidHeight > fMidBound )	//Crossing is above this
			{
				pt1 = float2( fMidBound, fMidHeight );
				vOffset1 = vMidOffset;
			}
			else
			{
				pt2 = float2( fMidBound, fMidHeight );
				vOffset2 = vMidOffset;
			}
		 }
	  }

	  float fDelta2 = pt2.x - pt2.y;
	  float fDelta1 = pt1.x - pt1.y;
	  
	  float fDenominator = fDelta2 - fDelta1;
	  
	  if ( abs( fDenominator ) < POM_MIN_CROSSING_DENOMINATOR )
	  {
		 fParallaxAmount = 0.0f;
	  }
	  else
	  {
		 fParallaxAmount = (pt1.x * fDelta2 - pt2.x * fDelta1 ) / fDenominator;
	  }
	  
	  float2 vParallaxOffset = vParallaxOffsetTS * (1 - fParallaxAmount );

	return texCoord - vParallaxOffset;
}

/**
Displace one layer's texture coordinate, tangent frame included.
\param origPos View space position.
\param texCoord The layer.
\param heightMapScale Displacement scale.
\param tex The height field, red channel.
*/
float2 parallaxOcclusionMap(float4 origPos, float2 texCoord, float heightMapScale, Texture2D tex)
{
	float3x3 tangentSpace = tangentSpaceFromDerivatives(origPos.xyz,texCoord);
	float3 viewTS = mul(tangentSpace,-origPos.xyz);
	float2 vParallaxOffsetTS = calcPOMVector(viewTS,heightMapScale);
	return POM(origPos,viewTS,tangentSpace[2],texCoord,vParallaxOffsetTS,tex);
}

