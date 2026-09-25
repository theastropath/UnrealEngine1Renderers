/*=============================================================================
	ComplexSurface.cpp: world geometry.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

void UD3D9RenderDevice::DrawComplexSurface(FSceneNode *Frame, FSurfaceInfo &Surface, FSurfaceFacet &Facet) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: DrawComplexSurface = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::DrawComplexSurface);

	if (m_frameSkipped) {
		return;
	}

	//May be the first surface of a node not yet opened, including a mirror's invisible surface.
	EndDeferredGouraudPolysForFrame(Frame);

	EndBufferingExcept(BV_TYPE_COMPLEX_SURFACE);

	//The engine gives no guarantee of scene setup before a node's first draw call.
	//The frustum comes from the node's own projection.
	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	SetDefaultAAState();
	SetDefaultProjectionState();

	check(Surface.Texture);

	if (m_HitData) {
		EndComplexSurfaceBuffering();
		for (FSavedPoly *Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			INT NumPts = Poly->NumPts;
			CGClip::vec3_t triPts[3];
			INT i;
			const FTransform *Pt;

			if (NumPts < 3) {
				continue;
			}

			Pt = Poly->Pts[0];
			triPts[0].x = Pt->Point.X;
			triPts[0].y = Pt->Point.Y;
			triPts[0].z = Pt->Point.Z;

			for (i = 2; i < NumPts; i++) {
				Pt = Poly->Pts[i - 1];
				triPts[1].x = Pt->Point.X;
				triPts[1].y = Pt->Point.Y;
				triPts[1].z = Pt->Point.Z;

				Pt = Poly->Pts[i];
				triPts[2].x = Pt->Point.X;
				triPts[2].y = Pt->Point.Y;
				triPts[2].z = Pt->Point.Z;

				m_gclip.SelectDrawTri(triPts);
			}
		}

		return;
	}

	clock(ComplexCycles);

	m_csUDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	m_csVDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	//A mover's own faces are never CSG'd against the level, so the level geometry buried under one
	//is still drawn and z-fights with it, which pushing the uncut level geometry away from the
	//camera settles, where biasing every complex surface would move both of them and fix nothing,
	//and the bias is asked for per surface here because only the lightmap's cache id says which
	//model a surface belongs to.
	bool biasDepth = ((m_moverDepthBias != 0.0f) || (m_moverSlopeScaleDepthBias != 0.0f)) &&
		IsNonWorldModelSurface(Surface);

	//Ahead of the choice of path below: both cache this lightmap and only one of them runs this frame.
	if (Surface.LightMap && TexInfoRealtimeChanged(*Surface.LightMap)) {
		NoteLightmapChanged(*Surface.LightMap);
	}

	if (TryBatchComplexSurface(Surface, Facet, biasDepth)) {
	} else {
		EndComplexSurfaceBuffering();
		FlushDeferred();

		if (biasDepth) {
			SetDepthBias(m_moverDepthBias);
			SetSlopeScaleDepthBias(m_moverSlopeScaleDepthBias);
		}

		//A surface too big for the arrays is split across several draws. Discarding it leaves a hole.
		FSavedPoly *pNextPoly = Facet.Polys;
		do {
			INT numVerts;
			if (UseFragmentProgram) {
				numVerts = BufferStaticComplexSurfaceGeometry_VP(Facet, pNextPoly);
			} else {
				numVerts = BufferStaticComplexSurfaceGeometry(Facet, pNextPoly);
			}

			if (numVerts == 0) {
				break;
			}

			DrawComplexSurfaceChunk(Surface, Facet, numVerts);

			//The next chunk's first pass hasn't written depth yet.
			if (m_rpSetDepthEqual == true) {
				m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
				m_rpSetDepthEqual = false;
			}
		} while (pNextPoly != NULL);

		DrawComplexSurfaceSelection(Surface, Facet);

		if (biasDepth) {
			SetDepthBias(0.0f);
			SetSlopeScaleDepthBias(0.0f);
		}
	}

	unclock(ComplexCycles);
	unguard;
}

void UD3D9RenderDevice::DrawComplexSurfaceChunk(FSurfaceInfo &Surface, const FSurfaceFacet &Facet, INT numVerts) {
	const DWORD PolyFlags = Surface.PolyFlags;

	m_csPtCount = numVerts;

	BuildComplexSurfaceIndices();

	//Mutually exclusive.
	bool drawDetailTexture = false;
	if ((DetailTextures != 0) && Surface.DetailTexture && !Surface.FogMap) {
		drawDetailTexture = true;
	}

	if (drawDetailTexture == true) {
		DWORD anyIsNearBits;

		anyIsNearBits = (this->*m_pBufferDetailTextureDataProc)(380.0f);

		//The near-vertex mask only covers a polygon's first 32 points; past that, earlier bits shift out.
		if (anyIsNearBits == 0) {
			drawDetailTexture = false;
			for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
				if (MultiDrawCountArray[PolyNum] > 32) {
					drawDetailTexture = true;
					break;
				}
			}
		}
	}


	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	//Valid only if the first pass wrote depth for the depth-equal test.
	m_rpMasked = ((PolyFlags & PF_Masked) != 0) &&
		(((PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted)) == 0) || ((PolyFlags & PF_Occlude) != 0));
	m_rpSetDepthEqual = false;
	m_rpColor = 0xFFFFFFFF;


	if (UseFragmentProgram) {
		const FVector &XAxis = Facet.MapCoords.XAxis;
		const FVector &YAxis = Facet.MapCoords.YAxis;
		FLOAT vsParams[8] = { XAxis.X, XAxis.Y, XAxis.Z, m_csUDot,
			YAxis.X, YAxis.Y, YAxis.Z, m_csVDot };

		m_d3dDevice->SetVertexShaderConstantF(4, vsParams, 2);
	}

	AddRenderPass(Surface.Texture, PolyFlags & ~PF_FlatShaded, 0.0f, false);

	if (Surface.MacroTexture) {
		AddRenderPass(Surface.MacroTexture, PF_Modulated, -0.5f, false);
	}

	//Both are seven bit layers.
	if (Surface.LightMap) {
		AddRenderPass(Surface.LightMap, PF_Modulated, -0.5f, true);
	}

	if (Surface.FogMap) {
		if (!SinglePassFog) {
			RenderPasses();
		}

		AddRenderPass(Surface.FogMap, PF_Highlighted, -0.5f, true);
	}

	if (drawDetailTexture == true) {
		bool singlePassDetail = false;

		if (SinglePassDetail) {
			if (!m_rpForceSingle) {
				if ((m_rpPassCount == 1) || (m_rpPassCount == 2)) {
					singlePassDetail = true;
				}
			}
		}

		if (singlePassDetail) {
			RenderPasses_SingleOrDualTextureAndDetailTexture(*Surface.DetailTexture);
		} else {
			RenderPasses();

			bool clipDetailTexture = (DetailClipping != 0);

			if (m_rpMasked) {
				//Not with depth-equal.
				clipDetailTexture = false;

				if (m_rpSetDepthEqual == false) {
					m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
					m_rpSetDepthEqual = true;
				}
			}

			if (UseFragmentProgram) {
				DrawDetailTexture_FP(*Surface.DetailTexture);
			} else {
				DrawDetailTexture(*Surface.DetailTexture, clipDetailTexture);
			}
		}
	} else {
		RenderPasses();
	}
}

//The facet's own list.
void UD3D9RenderDevice::DrawComplexSurfaceSelection(FSurfaceInfo &Surface, const FSurfaceFacet &Facet) {
	const DWORD PolyFlags = Surface.PolyFlags;

	if (GIsEditor && (PolyFlags & (PF_Selected | PF_FlatShaded))) {
		DWORD polyColor;

		SetDefaultStreamState();
		SetDefaultTextureState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);

		if (PolyFlags & PF_FlatShaded) {
			FPlane Color;

			Color.X = Surface.FlatColor.R / 255.0f;
			Color.Y = Surface.FlatColor.G / 255.0f;
			Color.Z = Surface.FlatColor.B / 255.0f;
			Color.W = 0.85f;
			if (PolyFlags & PF_Selected) {
				Color.X *= 1.5f;
				Color.Y *= 1.5f;
				Color.Z *= 1.5f;
				Color.W = 1.0f;
			}

			polyColor = FPlaneTo_BGRAClamped(&Color);
		} else {
			polyColor = 0x7F00007F;
		}

		for (FSavedPoly *Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			INT NumPts = Poly->NumPts;
			if ((NumPts < 3) || (NumPts > VERTEX_ARRAY_SIZE)) {
				continue;
			}

			if ((m_curVertexBufferPos + NumPts) >= VERTEX_RING_SIZE) {
				FlushVertexBuffers();
			}

			LockVertexColorBuffer();
			LockTexCoordBuffer(0);

			FGLTexCoord *pTexCoordArray = m_pTexCoordArray[0];
			FGLVertexColor *pVertexColorArray = m_pVertexColorArray;

			FTransform **pPts = &Poly->Pts[0];
			for (INT i = 0; i < NumPts; i++) {
				const FVector &Point = pPts[i]->Point;

				pTexCoordArray[i].u = 0.5f;
				pTexCoordArray[i].v = 0.5f;

				pVertexColorArray[i].x = Point.X;
				pVertexColorArray[i].y = Point.Y;
				pVertexColorArray[i].z = Point.Z;
				pVertexColorArray[i].color = polyColor;
			}

			UnlockVertexColorBuffer();
			UnlockTexCoordBuffer(0);

			m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

			m_curVertexBufferPos += NumPts;
		}
	}
}
