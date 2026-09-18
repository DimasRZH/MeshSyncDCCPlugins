#include "pch.h"

#include "BlenderUtility.h"

#include "MeshSync/MeshSyncConstants.h" //msConstants::MAX_UV
#include "MeshSync/SceneGraph/msMesh.h"
#include "MeshUtils/muMath.h" //mu::float2
#include "MeshUtils/muRawVector.h" //SharedVector

#include "msblenBinder.h" //BMesh

namespace blender {

void BlenderUtility::ApplyBMeshUVToMesh(const blender::BlenderMesh* bMesh, const size_t numIndices, ms::Mesh* dest) {

    const uint32_t numUVs = std::min(bMesh->GetNumUVs(), ms::MeshSyncConstants::MAX_UV);

    for (uint32_t uvIndex=0;uvIndex<numUVs;++uvIndex) {
        auto* loopUV = bMesh->GetUV(uvIndex);
        if (nullptr == loopUV)
            continue;

        SharedVector<mu::float2>& curUV = dest->m_uv[uvIndex];
        curUV.resize_discard(numIndices);
        for (size_t ii = 0; ii < numIndices; ++ii) {
#if BLENDER_VERSION >= 405
            curUV[ii] = { loopUV->x, loopUV->y };
#else
            curUV[ii] = (mu::float2&)loopUV->uv;
#endif
            ++loopUV;
        }
    }

}

//----------------------------------------------------------------------------------------------------------------------

Material** BlenderUtility::GetMaterials(Object* obj) {
    Material*** matPointers = BKE_object_material_array_p(obj);
    return matPointers? *matPointers: NULL;
}

//----------------------------------------------------------------------------------------------------------------------

short BlenderUtility::GetNumMaterials(Object* obj) {
    const short* numMaterials = BKE_object_material_len_p(obj);
    return (nullptr == numMaterials) ? 0 : *numMaterials;
}



} //end namespace


