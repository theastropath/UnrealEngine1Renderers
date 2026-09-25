
#include "../D3D9Drv.h"
#include "../D3D9.h"


void UD3D9RenderDevice::LogTileOnce(const FTextureInfo &Info, DWORD polyFlags, DWORD blendPolyFlags) {
	const QWORD cacheID = (QWORD)Info.CacheID;
	for (DWORD u = 0; u < m_numLoggedTiles; u++) {
		if (m_loggedTileIds[u] == cacheID) {
			return;
		}
	}
	if (m_numLoggedTiles >= MAX_LOGGED_TILES) {
		return;
	}
	m_loggedTileIds[m_numLoggedTiles++] = cacheID;

	debugf(NAME_Log, TEXT("utd3d9r: DrawTile id %08X%08X format %i palette %i marker %i PolyFlags %08X drawn %08X"),
		(DWORD)(cacheID >> 32), (DWORD)cacheID,
		(INT)Info.Format,
		(Info.Palette != NULL) ? 1 : 0,
		(Info.Palette && (Info.Palette[128].A != 255)) ? 1 : 0,
		polyFlags, blendPolyFlags);
}

void UD3D9RenderDevice::DbgPrintInitParam(const TCHAR *pName, INT value) {
	dout << TEXT("utd3d9r: ") << pName << TEXT(" = ") << value << std::endl;
	return;
}

void UD3D9RenderDevice::DbgPrintInitParam(const TCHAR *pName, FLOAT value) {
	dout << TEXT("utd3d9r: ") << pName << TEXT(" = ") << value << std::endl;
	return;
}

void UD3D9RenderDevice::DbgPrintInitParam(const TCHAR *pName, BITFIELD value) {
	DbgPrintInitParam(pName, (INT)value);
	return;
}
