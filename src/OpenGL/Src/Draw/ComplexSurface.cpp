/*=============================================================================
	ComplexSurface.cpp: the level's own BSP surfaces.

	Up to five texture layers per surface.
=============================================================================*/

#include "../OpenGLDrv.h"
#include "../OpenGL.h"

void UOpenGLRenderDevice::DrawComplexSurface(FSceneNode *Frame, FSurfaceInfo &Surface, FSurfaceFacet &Facet) {
	UTGLR_DEBUG_CALL_COUNT(DrawComplexSurface);
	guard(UOpenGLRenderDevice::DrawComplexSurface);

	//May be the first surface of a node not yet opened.
	EndDeferredGouraudPolysForFrame(Frame);

	EndBuffering();

	/*
	There is no guarantee of scene setup before a node's first draw call and dimensions alone
	cannot tell a child node from its parent because a mirror or a security camera shares the
	viewport's width and height while looking somewhere else entirely, so every entry point
	that can be reached first asks for itself.
	*/
	if (SceneNodeChanged(Frame)) {
		m_sceneNodeRefreshCount++;
		SetSceneNode(Frame);
	}

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();

	check(Surface.Texture);

	if (m_HitData) {
		for (FSavedPoly *Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			INT NumPts = Poly->NumPts;
			CGClip::vec3_t triPts[3];
			INT i;
			const FTransform *Pt;

			//Needs a first point.
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

	DWORD PolyFlags = Surface.PolyFlags;

	/*
	A mover's faces are never CSG'd against the level, so the buried level geometry is still
	drawn and fights for the same pixels until the bias settles it in the level's favour, and
	the bias is held around the whole facet so a facet drawn in several passes is biased
	throughout, the detail texture pass wanting the opposite offset and restoring its own.
	*/
	const bool biasDepth = IsNonWorldModelSurface(Surface);
	if (biasDepth) {
		SetPolygonOffset(m_moverPolyOffsetFactor, m_moverPolyOffsetUnits);
		SetPolygonOffsetEnabled(true);
	}

	//Chunked when the arrays fill.
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

		//The next chunk writes depth again.
		if (m_rpSetDepthEqual == true) {
			glDepthFunc((GLenum)ZTrickFunc);
			m_rpSetDepthEqual = false;
		}
	} while (pNextPoly != NULL);

	//Before the selection overlay.
	if (biasDepth) {
		SetPolygonOffsetEnabled(false);
		SetPolygonOffset(m_detailPolyOffsetFactor, m_detailPolyOffsetUnits);
	}

	if (GIsEditor && (PolyFlags & (PF_Selected | PF_FlatShaded))) {
		//Already set on entry.
		SetDefaultShaderState();
		SetDefaultTextureState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);
		if (PolyFlags & PF_FlatShaded) {
			if (PolyFlags & PF_Selected) {
				SetColor4f((Surface.FlatColor.R * 1.5f) / 255.0f, (Surface.FlatColor.G * 1.5f) / 255.0f, (Surface.FlatColor.B * 1.5f) / 255.0f, 1.0f);
			} else {
				SetColor4f(Surface.FlatColor.R / 255.0f, Surface.FlatColor.G / 255.0f, Surface.FlatColor.B / 255.0f, 0.85f);
			}
		} else {
			SetColor4f(0.0f, 0.0f, 0.5f, 0.5f);
		}

		for (FSavedPoly *Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			glBegin(GL_TRIANGLE_FAN);
			for (INT i = 0; i < Poly->NumPts; i++) {
				glVertex3fv(&Poly->Pts[i]->Point.X);
			}
			glEnd();
		}
	}

	unclock(ComplexCycles);
	unguard;
}

void UOpenGLRenderDevice::DrawComplexSurfaceChunk(const FSurfaceInfo &Surface, const FSurfaceFacet &Facet, INT numVerts) {
	const DWORD PolyFlags = Surface.PolyFlags;

	m_csPtCount = numVerts;

	//Once per chunk.
	StageVertexStream(VERTEX_STREAM_VERTEX, VertexArray, sizeof(FGLVertex), (DWORD)numVerts);

	/*
	Uploaded here so the ring offset folds into the draw offsets and saves a re-point per
	facet, which only the fragment program path can manage because a draw's first vertex
	applies to every enabled array at once and this is the one case where the position array
	is the only one enabled, anywhere else the offset having to be paid for by moving the
	other arrays with it.
	*/
	m_csBaseVertex = 0;
	if (m_vboActive) {
		if (UseFragmentProgram) {
			UploadVertexStream(VERTEX_STREAM_VERTEX, false);
			SetVertexArrayBaseZero();

			m_csBaseVertex = (INT)(m_vertexStreams[VERTEX_STREAM_VERTEX].BaseByteOffset / sizeof(FGLVertex));
			for (DWORD polyNum = 0; polyNum < m_csPolyCount; polyNum++) {
				MultiDrawFirstArray[polyNum] += m_csBaseVertex;
			}
		} else {
			UploadVertexStream(VERTEX_STREAM_VERTEX);
		}
	}

	//Never both at once.
	bool drawDetailTexture = false;
	if ((DetailTextures != 0) && Surface.DetailTexture && !Surface.FogMap) {
		drawDetailTexture = true;
	}

	//Too many points overruns the clipped pass's buffer.
	if (drawDetailTexture == true) {
		for (DWORD polyNum = 0; polyNum < m_csPolyCount; polyNum++) {
			if (MultiDrawCountArray[polyNum] > DETAIL_TEXTURE_MAX_POLY_PTS) {
				drawDetailTexture = false;
				break;
			}
		}
	}

	if (drawDetailTexture == true) {
		DWORD anyIsNearBits;

		anyIsNearBits = (this->*m_pBufferDetailTextureDataProc)(380.0f);

		//The near-vertex mask covers a polygon's first 32 points.
		//Earlier bits shift out.
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
	//Valid only if the first pass wrote depth.
	m_rpMasked = ((PolyFlags & PF_Masked) != 0) &&
		(((PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted)) == 0) || ((PolyFlags & PF_Occlude) != 0));
	m_rpSetDepthEqual = false;


	if (UseFragmentProgram) {
		const FVector &XAxis = Facet.MapCoords.XAxis;
		const FVector &YAxis = Facet.MapCoords.YAxis;

		glVertexAttrib4fARB(6, XAxis.X, XAxis.Y, XAxis.Z, -m_csUDot);
		glVertexAttrib4fARB(7, YAxis.X, YAxis.Y, YAxis.Z, -m_csVDot);
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
		bool useFragmentProgramSinglePassFog = false;

		//Fog stays last.
		if (UseFragmentProgram && DCV.SinglePassFog) {
			if ((m_rpPassCount == 1) || (m_rpPassCount == 2)) {
				useFragmentProgramSinglePassFog = true;
			}
		}

		if (!useFragmentProgramSinglePassFog) {
			//Forced into the first pass without combiner support.
			if (!SinglePassFog) {
				RenderPasses();
			}
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

			//Clipping appends vertices.
			if (numVerts > (VERTEX_ARRAY_SIZE / 2)) {
				clipDetailTexture = false;
			}

			if (m_rpMasked) {
				//Detail clipping conflicts with masked mode's depth-equal test.
				clipDetailTexture = false;

				if (m_rpSetDepthEqual == false) {
					glDepthFunc(GL_EQUAL);
					m_rpSetDepthEqual = true;
				}
			}

			//Something needs it.
			if (UseFragmentProgram) {
				DrawDetailTexture_FP(*Surface.DetailTexture);
			} else {
				DrawDetailTexture(*Surface.DetailTexture, numVerts, clipDetailTexture);
			}
		}
	} else {
		RenderPasses();
	}
}
