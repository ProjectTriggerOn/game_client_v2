#pragma once
//=============================================================================
// model_catalog.h — scan + lazy-load + cache FBX models by asset name (spec §3.2).
// Shared by the editor palette (M4) and runtime .map model resolution.
//=============================================================================
#include "model.h"

// Scan `dir` (e.g. "resource/model") for *.fbx; record asset names as
// "<dir>/<file>.fbx" (the same string stored in a .map MapModelRef.asset).
void ModelCatalog_Init(const char* dir);

int         ModelCatalog_Count();
const char* ModelCatalog_Name(int i);            // asset path string, or "" if OOB

// Lazy-load + cache by asset name; nullptr if unknown or load failed.
MODEL* ModelCatalog_Get(const char* assetName);

void ModelCatalog_Finalize();                     // release cached MODEL*
