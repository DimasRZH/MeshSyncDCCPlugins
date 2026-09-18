#pragma once

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS
    #define NOMINMAX
    #include <windows.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <iostream>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <array>
#include <thread>
#include <future>

#include "pybind11/pybind11.h"
#include "pybind11/operators.h"
#include "pybind11/eval.h"
#include "pybind11/stl.h"
namespace py = pybind11;

#ifndef NDEBUG
    #define NDEBUG
#endif
#include "BKE_blender_version.h"
#if BLENDER_VERSION >= 501 && defined(_MSC_VER) && !defined(__clang__)
    // Blender 5.1 is built with clang-cl. Its empty allocator member is not compressed,
    // so match that PointerRNA/BLI::Vector layout when building this module with MSVC.
    #include "BLI_utildefines.h"
    #undef BLI_NO_UNIQUE_ADDRESS
    #define BLI_NO_UNIQUE_ADDRESS
#endif
#pragma warning( push )
#pragma warning( disable : 4200 ) // zero length array
#if BLENDER_VERSION >= 405
#include "BKE_main.hh"
#include "BKE_customdata.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_editmesh.hh"
#include "BKE_material.hh"
#include "BKE_mesh_types.hh"
#include "BKE_node.hh"
#include "RNA_define.hh"
#include "RNA_types.hh"
#else
#include "BKE_main.h"
#include "BKE_customdata.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_editmesh.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "RNA_define.h"
#include "RNA_types.h"
#endif

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#if BLENDER_VERSION >= 405
#include "DNA_grease_pencil_types.h"
#else
#include "DNA_gpencil_types.h" //bGPdata
#endif
#if BLENDER_VERSION < 302
#include "DNA_hair_types.h" //Hair
#endif
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h" //PointCloud
#include "DNA_scene_types.h"
#include "DNA_volume_types.h" //Volume
#include "DNA_packedFile_types.h" // PackedFile

#include "BLI_utildefines.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#if BLENDER_VERSION >= 405
#include "bmesh_class.hh"
#include "intern/rna_internal_types.hh"
#include "intern/bpy_rna.hh"
#include "intern/bmesh_structure.hh"
#else
#include "bmesh_class.h"
#include "intern/rna_internal_types.h"
#include "intern/bpy_rna.h"
#include "intern/bmesh_structure.h"
#endif
#pragma warning( pop ) 

#if BLENDER_VERSION >= 501
static_assert(sizeof(blender::PointerRNA) == 0x58,
              "Blender 5.1 PointerRNA ABI mismatch");
using blender::AnimData;
using blender::ArmatureModifierData;
using blender::BMesh;
using blender::BMEditMesh;
using blender::BMEdge;
using blender::BMFace;
using blender::BMLoop;
using blender::BMVert;
using blender::BPy_StructRNA;
using blender::BezTriple;
using blender::Bone;
using blender::Camera;
using blender::CAMERA_SENSOR_FIT_AUTO;
using blender::CAMERA_SENSOR_FIT_HOR;
using blender::CAMERA_SENSOR_FIT_VERT;
using blender::CAM_ORTHO;
using blender::Collection;
using blender::CollectionObject;
using blender::COLLECTION_HIDE_RENDER;
using blender::Curve;
using blender::CU_NURB_CYCLIC;
using blender::CustomData;
using blender::CustomDataLayer;
using blender::Depsgraph;
using blender::FCurve;
using blender::FunctionRNA;
using blender::ID;
using blender::Key;
using blender::KeyBlock;
using blender::LayerCollection;
using blender::LAYER_COLLECTION_EXCLUDE;
using blender::LA_AREA;
using blender::LA_SPOT;
using blender::LA_SUN;
using blender::Light;
using blender::ListBase;
using blender::Main;
using blender::Material;
using blender::Mesh;
using blender::MirrorModifierData;
using blender::ModifierData;
using blender::ModifierType;
using blender::MOD_MIR_AXIS_X;
using blender::MOD_MIR_AXIS_Y;
using blender::MOD_MIR_AXIS_Z;
using blender::MDeformVert;
using blender::MDeformWeight;
using blender::MLoopCol;
using blender::Nurb;
using blender::Object;
using blender::OB_ARMATURE;
using blender::OB_CAMERA;
using blender::OB_CURVES_LEGACY;
using blender::OB_FONT;
using blender::OB_LAMP;
using blender::OB_MBALL;
using blender::OB_MESH;
using blender::OB_SURF;
using blender::PointerRNA;
using blender::PARBONE;
using blender::PropertyRNA;
using blender::Scene;
using blender::StructRNA;
using blender::TimeMarker;
using blender::ViewLayer;
using blender::bContext;
using blender::bArmature;
using blender::bDeformGroup;
using blender::bNode;
using blender::bNodeLink;
using blender::bNodeSocket;
using blender::bNodeTree;
using blender::bPoseChannel;
using blender::BM_ELEM_SMOOTH;
using blender::eModifierType_Armature;
using blender::eModifierType_Mirror;
#endif
