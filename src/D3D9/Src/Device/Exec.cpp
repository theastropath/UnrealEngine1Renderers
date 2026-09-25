/*=============================================================================
	Exec.cpp: console commands, and the resolution list the options window uses.
=============================================================================*/

#include "../D3D9Drv.h"
#include "../D3D9.h"

UBOOL UD3D9RenderDevice::Exec(const TCHAR *Cmd, FOutputDevice &Ar) {
	guard(UD3D9RenderDevice::Exec);

	//The base render device's command handler is pure virtual on this engine generation,
	//so there is nothing to chain to.
#ifndef UTGLR_OLD_URENDERDEVICE
	if (URenderDevice::Exec(Cmd, Ar)) {
		return 1;
	}
#endif
	if (ParseCommand(&Cmd, TEXT("DGL"))) {
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS"))) {
			BufferActorTris = !BufferActorTris;
			if (!UseVertexSpecular) BufferActorTris = 0;
			debugf(TEXT("BUFFERTRIS [%i]"), BufferActorTris);
			return 1;
		} else if (ParseCommand(&Cmd, TEXT("BUILD"))) {
			debugf(TEXT("D3D9 renderer built: %s"), appFromAnsi(__DATE__ " " __TIME__));
			return 1;
		} else if (ParseCommand(&Cmd, TEXT("AA"))) {
			if (m_usingAA) {
				m_defAAEnable = !m_defAAEnable;
				debugf(TEXT("AA Enable [%u]"), (m_defAAEnable) ? 1 : 0);
			}
			return 1;
		}

		return 0;
	} else if (ParseCommand(&Cmd, TEXT("GetRes"))) {
		TArray<FPlane> Relevant;
		INT i;
		for (i = 0; i < Modes.Num(); i++) {
			if (Modes(i).Z == (Viewport->ColorBytes * 8))
				if ((Modes(i).X != 320 || Modes(i).Y != 200) && (Modes(i).X != 640 || Modes(i).Y != 400))
					Relevant.AddUniqueItem(FPlane(Modes(i).X, Modes(i).Y, 0, 0));
		}

		//A windowed viewport can be any size, and leaving it out costs the menu its current-resolution entry
		if ((Viewport->SizeX > 0) && (Viewport->SizeY > 0)) {
			Relevant.AddUniqueItem(FPlane(Viewport->SizeX, Viewport->SizeY, 0, 0));
		}

		if (Relevant.Num() == 0) {
			return 1;
		}
		appQsort(&Relevant(0), Relevant.Num(), sizeof(FPlane), (QSORT_COMPARE)CompareRes);

		//Gated on the game, because Nerf, Unreal 224 and Klingon have older menus.
		//FIXME: unverified against the real games.
#if defined UTGLR_DX_BUILD || defined UTGLR_UNREAL_226_BUILD || defined UTGLR_NERF_BUILD || defined UTGLR_UNREAL_224_BUILD || defined UTGLR_KLINGON_BUILD
		/*
		Those games read only the first sixteen resolutions offered and the list is ascending, so
		on a modern display all sixteen would be legacy modes from 640x350 up with the size in use
		missing and reopening the options screen would drop the game to 640x350, while offering
		the sixteen largest keeps it reachable and caps only the list handed out.
		*/
		const INT MaxRelevant = 16;
		if (Relevant.Num() > MaxRelevant) {
			INT Cull = Relevant.Num() - MaxRelevant;
			INT Current = -1;
			for (i = 0; i < Cull; i++) {
				if (((INT)Relevant(i).X == Viewport->SizeX) && ((INT)Relevant(i).Y == Viewport->SizeY)) {
					Current = i;
					break;
				}
			}
			if (Current < 0) {
				Relevant.Remove(0, Cull);
			} else {
				Relevant.Remove(Current + 1, Cull - Current);
				if (Current > 0) {
					Relevant.Remove(0, Current);
				}
			}
		}
#endif

		//A manual separator before each entry but the first,
		//through an older formatting call:
		//one target build's engine headers have neither the modern call nor the trim helper it would need.
		FString Str;
		for (i = 0; i < Relevant.Num(); i++) {
			TCHAR mode[32];
			appSprintf(mode, (i == 0) ? TEXT("%ix%i") : TEXT(" %ix%i"), (INT)Relevant(i).X, (INT)Relevant(i).Y);
			Str += mode;
		}
		Ar.Log(*Str);
		return 1;
	}

	return 0;
	unguard;
}
