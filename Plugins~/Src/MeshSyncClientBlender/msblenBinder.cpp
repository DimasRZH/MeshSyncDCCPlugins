#include "pch.h"
#include "msblenContext.h"
#include "msblenUtils.h"
#include "msblenBinder.h"

#include "MeshUtils/muLog.h"

#include "BlenderPyObjects/BlenderPyDepsgraph.h"
#include "BlenderPyObjects/BlenderPyDepsgraphObjectInstance.h"
#include "BlenderPyObjects/BlenderPyContext.h"
#include "BlenderPyObjects/BlenderPyScene.h"
#include "BlenderPyObjects/BlenderPyCommon.h" //call, etc
#if BLENDER_VERSION >= 501
#include <BLI_listbase.h>
#endif

namespace blender {

bContext *g_context;

extern PropertyRNA* BlenderPyID_is_updated;
extern PropertyRNA* BlenderPyID_is_updated_data;
extern FunctionRNA* BlenderPyID_evaluated_get;
extern FunctionRNA* BlenderPyID_update_tag;

StructRNA* BObject::s_type;
static PropertyRNA* BObject_matrix_local;
static PropertyRNA* BObject_matrix_world;
static PropertyRNA* BObject_hide;
static PropertyRNA* BObject_hide_viewport;
static PropertyRNA* BObject_hide_render;
static PropertyRNA* BObject_select;
static FunctionRNA* BObject_select_get;
static FunctionRNA* BObject_to_mesh;
static FunctionRNA* BObject_to_mesh_clear;
static FunctionRNA* BObject_modifiers_clear;

StructRNA* BlenderMesh::s_type;
static FunctionRNA* BMesh_calc_normals_split;
static FunctionRNA* BMesh_update;
static FunctionRNA* BMesh_clear_geometry;
static FunctionRNA* BMesh_vertices_add;
static FunctionRNA* BMesh_polygons_add;
static FunctionRNA* BMesh_loops_add;
static FunctionRNA* BMesh_edges_add;
static FunctionRNA* BMesh_normals_add;
static PropertyRNA* BMesh_vertices;
static PropertyRNA* BMesh_polygons;
static PropertyRNA* BMesh_loops;
static PropertyRNA* BMesh_corner_normals;
static PropertyRNA* BMesh_uv_layers;
static PropertyRNA* UVLoopLayers_active;
static PropertyRNA* LoopColors_active;
#if BLENDER_VERSION >= 405
static StructRNA* MeshLoop_s_type;
static PropertyRNA* MeshLoop_normal;
static StructRNA* MeshPolygon_s_type;
static PropertyRNA* MeshPolygon_material_index;
static PropertyRNA* MeshUVLoopLayer_uv;
#if BLENDER_VERSION >= 501
static PropertyRNA* MeshLoopColorLayer_data;
#endif
#endif

StructRNA* BCurve::s_type;
static PropertyRNA* BCurve_splines;
static FunctionRNA* BCurve_splines_clear;
static FunctionRNA* BCurve_splines_new;

StructRNA* BNurb::s_type;
static FunctionRNA* BNurb_splines_bezier_add;

StructRNA* BMaterial::s_type;
static PropertyRNA* BMaterial_use_nodes;
static PropertyRNA* BMaterial_active_node_material;

StructRNA* BCamera::s_type;
static PropertyRNA* BCamera_clip_start;
static PropertyRNA* BCamera_clip_end;
static PropertyRNA* BCamera_angle_x;
static PropertyRNA* BCamera_angle_y;
static PropertyRNA* BCamera_lens;
static PropertyRNA* BCamera_sensor_fit;
static PropertyRNA* BCamera_sensor_width;
static PropertyRNA* BCamera_sensor_height;
static PropertyRNA* BCamera_shift_x;
static PropertyRNA* BCamera_shift_y;

extern PropertyRNA* BlenderPyScene_frame_start;
extern PropertyRNA* BlenderPyScene_frame_end;
extern PropertyRNA* BlenderPyScene_frame_current;
extern FunctionRNA* BlenderPyScene_frame_set;

StructRNA* BData::s_type;
static PropertyRNA* BlendDataObjects_is_updated;
static FunctionRNA* BlendDataMeshes_remove;

extern PropertyRNA* BlenderPyContext_blend_data;
extern PropertyRNA* BlenderPyContext_scene;
extern FunctionRNA* BlenderPyContext_evaluated_depsgraph_get;
extern FunctionRNA* BlenderPyContext_depsgraph_update;
extern PropertyRNA* BlenderPyContext_view_layer;

extern PropertyRNA* BlenderPyDepsgraphObjectInstance_instance_object;
extern PropertyRNA* BlenderPyDepsgraphObjectInstance_is_instance;
extern PropertyRNA* BlenderPyDepsgraphObjectInstance_world_matrix;
extern PropertyRNA* BlenderPyDepsgraphObjectInstance_parent;
extern PropertyRNA* BlenderPyDepsgraphObjectInstance_object;

extern PropertyRNA* BlenderPyDepsgraph_object_instances;

bool ready()
{
    return g_context != nullptr;
}

// context: bpi.context in python
void setup(py::object bpy_context)
{
    static bool initialized = false;
    if (initialized)
        return;

#if BLENDER_VERSION >= 501
    std::vector<StructRNA*> types;
    auto bpy_types = py::module_::import("bpy").attr("types");
    const char* type_names[] = {
        "ID", "Object", "ObjectModifiers", "Mesh", "MeshVertices", "MeshPolygons",
        "MeshLoops", "MeshLoop", "MeshPolygon", "MeshUVLoopLayer", "MeshLoopColorLayer", "MeshEdges", "Curve", "CurveSplines",
        "SplineBezierPoints", "UVLoopLayers", "LoopColors", "Camera", "Material", "Scene",
        "BlendData", "BlendDataObjects", "BlendDataMeshes", "Context", "Depsgraph",
        "DepsgraphObjectInstance"};
    for (const char* name : type_names) {
        if (!py::hasattr(bpy_types, name))
            continue;
        auto bl_rna = bpy_types.attr(name).attr("bl_rna");
        types.push_back(reinterpret_cast<StructRNA*>(
            bl_rna.attr("as_pointer")().cast<uintptr_t>()));
    }
#elif BLENDER_VERSION >= 405
    BPy_StructRNA* rna = (BPy_StructRNA*)bpy_context.ptr();
    if (!rna->ptr)
        return;
    auto first_type = (StructRNA*)&rna->ptr->type->cont;
    while (first_type->cont.prev) {
        first_type = (StructRNA*)first_type->cont.prev;
    }
#else
    BPy_StructRNA* rna = (BPy_StructRNA*)bpy_context.ptr();
    auto first_type = (StructRNA*)&rna->ptr.type->cont;
    while (first_type->cont.prev) {
        first_type = (StructRNA*)first_type->cont.prev;
    }
#endif
    rna_sdata(bpy_context, g_context);

    // resolve blender types and functions
#define match_type(N) strcmp(type->identifier, N) == 0
#define match_func(N) strcmp(func->identifier, N) == 0
#define match_prop(N) strcmp(prop->identifier, N) == 0
#if BLENDER_VERSION >= 501
#define each_func for (const auto& function : type->functions) if (auto* func = function.get())
#define each_type for (auto* type : types)
#else
#define each_func for (auto *func : list_range((FunctionRNA*)type->functions.first))
#define each_type for (auto *type : list_range((StructRNA*)first_type))
#endif
#if BLENDER_VERSION >= 501
#define each_prop for (auto &property : type->cont.properties) if (auto* prop = &property)
#else
#define each_prop for (auto *prop : list_range((PropertyRNA*)type->cont.properties.first))
#endif

    each_type {
        if (match_type("ID")) {
            BlenderPyID::s_type = type;
            each_prop{
                if (match_prop("is_updated")) BlenderPyID_is_updated = prop;
                if (match_prop("is_updated_data")) BlenderPyID_is_updated_data = prop;
            }
            each_func {
                if (match_func("evaluated_get")) BlenderPyID_evaluated_get = func;
                if (match_func("update_tag")) BlenderPyID_update_tag = func;
            }
        }
        else if (match_type("Object")) {
            BObject::s_type = type;
            each_prop{
                if (match_prop("matrix_local")) BObject_matrix_local = prop;
                if (match_prop("matrix_world")) BObject_matrix_world = prop;
                if (match_prop("hide")) BObject_hide = prop;
                if (match_prop("hide_viewport")) BObject_hide_viewport = prop;
                if (match_prop("hide_render")) BObject_hide_render = prop;
                if (match_prop("select")) BObject_select = prop;
            }
            each_func {
                if (match_func("select_get")) BObject_select_get = func;
                if (match_func("to_mesh")) BObject_to_mesh = func;
                if (match_func("to_mesh_clear")) BObject_to_mesh_clear = func;
            }
        }
        else if (match_type("ObjectModifiers")) {
            each_func{
                if (match_func("clear")) BObject_modifiers_clear = func;
            }
        }        
        else if (match_type("Mesh")) {
            BlenderMesh::s_type = type;
            each_prop {
                if (match_prop("vertices")) BMesh_vertices = prop;
                if (match_prop("polygons")) BMesh_polygons = prop;
                if (match_prop("loops")) BMesh_loops = prop;
                if (match_prop("corner_normals")) BMesh_corner_normals = prop;
                if (match_prop("uv_layers")) BMesh_uv_layers = prop;
            }
            each_func {
                if (match_func("calc_normals_split")) BMesh_calc_normals_split = func;
                if (match_func("update")) BMesh_update = func;
                if (match_func("clear_geometry")) BMesh_clear_geometry = func;
            }
        }
        else if (match_type("MeshVertices")) {
            each_func{
                if (match_func("add")) BMesh_vertices_add = func;
            }
        }     
        else if (match_type("MeshPolygons")) {
            each_func{
                if (match_func("add")) BMesh_polygons_add = func;
            }
        }        
        else if (match_type("MeshLoops")) {
            each_func{
                if (match_func("add")) BMesh_loops_add = func;
            }
        }
#if BLENDER_VERSION >= 405
        else if (match_type("MeshLoop")) {
            MeshLoop_s_type = type;
            each_prop{
                if (match_prop("normal")) MeshLoop_normal = prop;
            }
        }
        else if (match_type("MeshPolygon")) {
            MeshPolygon_s_type = type;
            each_prop{
                if (match_prop("material_index")) MeshPolygon_material_index = prop;
            }
        }
        else if (match_type("MeshUVLoopLayer")) {
            each_prop {
                if (match_prop("uv")) MeshUVLoopLayer_uv = prop;
            }
        }
#if BLENDER_VERSION >= 501
        else if (match_type("MeshLoopColorLayer")) {
            each_prop {
                if (match_prop("data")) MeshLoopColorLayer_data = prop;
            }
        }
#endif
#endif
        else if (match_type("MeshEdges")) {
            each_func{
                if (match_func("add")) BMesh_edges_add = func;
            }
        }        
        else if (match_type("Curve")) {
            BCurve::s_type = type;
            each_prop{
                if (match_prop("splines")) BCurve_splines = prop;
            }
        }
        else if (match_type("CurveSplines")) {
            each_func{
                if (match_func("clear")) BCurve_splines_clear = func;
                if (match_func("new")) BCurve_splines_new = func;            
            }
        }     
        else if (match_type("SplineBezierPoints")) {
            BNurb::s_type = type;
            each_func{
                if (match_func("add")) BNurb_splines_bezier_add = func;
            }
        }        
        else if (match_type("UVLoopLayers")) {
            each_prop{
                if (match_prop("active")) UVLoopLayers_active = prop;
            }
        }
        else if (match_type("LoopColors")) {
            each_prop{
                if (match_prop("active")) LoopColors_active = prop;
            }
        }
        else if (match_type("Camera")) {
            BCamera::s_type = type;
            each_prop{
                if (match_prop("clip_start")) BCamera_clip_start = prop;
                if (match_prop("clip_end")) BCamera_clip_end = prop;
                if (match_prop("angle_x")) BCamera_angle_x = prop;
                if (match_prop("angle_y")) BCamera_angle_y = prop;
                if (match_prop("lens")) BCamera_lens = prop;
                if (match_prop("sensor_fit")) BCamera_sensor_fit = prop;
                if (match_prop("sensor_width")) BCamera_sensor_width = prop;
                if (match_prop("sensor_height")) BCamera_sensor_height = prop;
                if (match_prop("shift_x")) BCamera_shift_x = prop;
                if (match_prop("shift_y")) BCamera_shift_y = prop;
            }
        }
        else if (match_type("Material")) {
            BMaterial::s_type = type;
            each_prop{
                if (match_prop("use_nodes")) BMaterial_use_nodes = prop;
                if (match_prop("active_node_material")) BMaterial_active_node_material = prop;
            }
        }
        else if (match_type("Scene")) {
            BlenderPyScene::s_type = type;
            each_prop{
                if (match_prop("frame_start")) BlenderPyScene_frame_start = prop;
                if (match_prop("frame_end")) BlenderPyScene_frame_end = prop;
                if (match_prop("frame_current")) BlenderPyScene_frame_current = prop;
            }
            each_func{
                if (match_func("frame_set")) BlenderPyScene_frame_set = func;
            }
        }
        else if (match_type("BlendData")) {
            BData::s_type = type;
        }
        else if (match_type("BlendDataObjects")) {
            each_prop{
                if (match_prop("is_updated")) BlendDataObjects_is_updated = prop;
            }
        }
        else if (match_type("BlendDataMeshes")) {
            each_func{
                if (match_func("remove")) BlendDataMeshes_remove = func;
            }
        }
        else if (match_type("Context")) {
            BlenderPyContext::s_type = type;
            each_prop{
                if (match_prop("blend_data")) BlenderPyContext_blend_data = prop;
                if (match_prop("scene")) BlenderPyContext_scene = prop;
                if (match_prop("view_layer")) BlenderPyContext_view_layer = prop;
            }
            each_func{
                if (match_func("evaluated_depsgraph_get")) BlenderPyContext_evaluated_depsgraph_get = func;
            }
        }
        else if (match_type("Depsgraph")) {
            each_prop{
                if (match_prop("object_instances")) {
                    BlenderPyDepsgraph_object_instances = prop;
                }
            }
            each_func{
                if (match_func("update")) {
                    BlenderPyContext_depsgraph_update = func;
                }
            }
        }
        else if (match_type("DepsgraphObjectInstance")) {
            each_prop{
                if (match_prop("instance_object")) {
                    BlenderPyDepsgraphObjectInstance_instance_object = prop;
                }

                if (match_prop("is_instance")) {
                    BlenderPyDepsgraphObjectInstance_is_instance = prop;
                }
                if (match_prop("matrix_world")) {
                    BlenderPyDepsgraphObjectInstance_world_matrix = prop;
                }
                if (match_prop("parent")) {
                    BlenderPyDepsgraphObjectInstance_parent = prop;
                }
                if (match_prop("object")) {
                    BlenderPyDepsgraphObjectInstance_object = prop;
                }
            }
        }
    }
    initialized = BlenderPyContext_scene != nullptr;
#undef each_iprop
#undef each_nprop
#undef each_func
#undef match_prop
#undef match_func
#undef match_type

    // test
    //auto scene = BlenderPyContext::get().scene();
}




template<typename Self>
static inline bool get_bool(Self *self, PropertyRNA *prop)
{
    PointerRNA ptr;
	ptr.data = self;
	PointerRNA_OWNER_ID(ptr) = PointerRNA_OWNER_ID_CAST(self);
	
    return ((BoolPropertyRNA*)prop)->get(&ptr) != 0;
}
template<typename Self>
static inline float get_float(Self *self, PropertyRNA *prop)
{
    PointerRNA ptr;
	ptr.data = self;
	PointerRNA_OWNER_ID(ptr) = PointerRNA_OWNER_ID_CAST(self);
	
    return ((FloatPropertyRNA*)prop)->get(&ptr);
}
template<typename Self>
static inline void get_float_array(Self *self, float *dst, PropertyRNA *prop)
{
    PointerRNA ptr;
	ptr.data = self;
	PointerRNA_OWNER_ID(ptr) = PointerRNA_OWNER_ID_CAST(self);

    ((FloatPropertyRNA*)prop)->getarray(&ptr, dst);
}


const char *BObject::name() const { return ((BlenderPyID)*this).name(); }
void* BObject::data() { return m_ptr->data; }

mu::float4x4 BObject::matrix_local() const
{
    mu::float4x4 ret;
    get_float_array(m_ptr, (float*)&ret, BObject_matrix_local);
    return ret;
}

mu::float4x4 BObject::matrix_world() const
{
    mu::float4x4 ret;
    get_float_array(m_ptr, (float*)&ret, BObject_matrix_world);
    return ret;
}
bool BObject::hide_viewport() const
{
    return get_bool(m_ptr, BObject_hide_viewport);
}
bool BObject::hide_render() const
{
    return get_bool(m_ptr, BObject_hide_render);
}


bool blender::BObject::is_selected() const
{
#if BLENDER_VERSION >= 304
    PointerRNA ptr;
    ptr.data = nullptr;
    ptr.owner_id = nullptr;
    ptr.type = nullptr;
    return call<Object, bool, PointerRNA>(g_context, m_ptr, BObject_select_get, ptr);
#else
    return call<Object, bool, ViewLayer*>(g_context, m_ptr, BObject_select_get, nullptr);
#endif
}

Mesh* BObject::to_mesh() const
{
#if BLENDER_VERSION >= 303
    // In blender 3.3 the pointer to interpret the bool moves by 8 instead of 1 for some reason so this needs to be a type of that size:
    return call<Object, Mesh*, bool*, Depsgraph*>(g_context, m_ptr, BObject_to_mesh, nullptr, nullptr);
#else
    return call<Object, Mesh*, bool, Depsgraph*>(g_context, m_ptr, BObject_to_mesh, false, nullptr);
#endif
}

void BObject::to_mesh_clear()
{
    call<Object, Mesh*>(g_context, m_ptr, BObject_to_mesh_clear);
}

void BObject::modifiers_clear() 
{
    call<Object, void>(g_context, m_ptr, BObject_modifiers_clear);
}

blist_range<ModifierData> BObject::modifiers()
{
    return list_range((ModifierData*)m_ptr->modifiers.first);
}
blist_range<bDeformGroup> BObject::deform_groups()
{
    return list_range((bDeformGroup*)m_ptr->defbase.first);
}


#if BLENDER_VERSION >= 405
barray_range<int> BlenderMesh::indices()
{
#if BLENDER_VERSION >= 501
    auto* prop = reinterpret_cast<CollectionPropertyRNA*>(BMesh_loops);
    if (!prop)
        return { nullptr, 0 };
    PointerRNA ptr{};
    ptr.owner_id = &m_ptr->id;
    ptr.type = s_type;
    ptr.data = m_ptr;
    CollectionPropertyIterator iter{};
    prop->begin(&iter, &ptr);
    if (!iter.valid || !iter.internal.array.ptr || iter.internal.array.length <= 0)
        return { nullptr, 0 };
    return { reinterpret_cast<int*>(iter.internal.array.ptr), static_cast<size_t>(iter.internal.array.length) };
#else
    auto* data = static_cast<int*>(msCustomData_get_layer_named(
        &m_ptr->corner_data, CD_PROP_INT32, ".corner_vert"));
    return { data, data ? static_cast<size_t>(m_ptr->corners_num) : 0 };
#endif
}

barray_range<int> BlenderMesh::face_offsets()
{
    return { m_ptr->face_offset_indices, static_cast<size_t>(m_ptr->faces_num + 1) };
}

barray_range<mu::float3> BlenderMesh::vertices()
{
#if BLENDER_VERSION >= 501
    auto* prop = reinterpret_cast<CollectionPropertyRNA*>(BMesh_vertices);
    if (!prop)
        return { nullptr, 0 };
    PointerRNA ptr{};
    ptr.owner_id = &m_ptr->id;
    ptr.type = s_type;
    ptr.data = m_ptr;
    CollectionPropertyIterator iter{};
    prop->begin(&iter, &ptr);
    if (!iter.valid || !iter.internal.array.ptr || iter.internal.array.length <= 0)
        return { nullptr, 0 };
    return { reinterpret_cast<mu::float3*>(iter.internal.array.ptr), static_cast<size_t>(iter.internal.array.length) };
#else
    auto* data = static_cast<mu::float3*>(msCustomData_get_layer_named(
        &m_ptr->vert_data, CD_PROP_FLOAT3, "position"));
    return { data, data ? static_cast<size_t>(m_ptr->verts_num) : 0 };
#endif
}

barray_range<MDeformVert> BlenderMesh::deform_vertices()
{
    auto* data = static_cast<MDeformVert*>(msCustomData_get_layer_n(
        &m_ptr->vert_data, CD_MDEFORMVERT, 0));
    return { data, data ? static_cast<size_t>(m_ptr->verts_num) : 0 };
}

mu::float3 BlenderMesh::normal(int index) const
{
    mu::float3 ret{};
#if BLENDER_VERSION >= 501
    if (!BMesh_corner_normals || index < 0)
        return ret;
    auto* prop = reinterpret_cast<CollectionPropertyRNA*>(BMesh_corner_normals);
    PointerRNA ptr{};
    ptr.owner_id = &m_ptr->id;
    ptr.type = s_type;
    ptr.data = m_ptr;
    CollectionPropertyIterator iter{};
    prop->begin(&iter, &ptr);
    if (iter.valid && iter.internal.array.ptr && index < iter.internal.array.length)
        ret = *reinterpret_cast<mu::float3*>(iter.internal.array.ptr + index * iter.internal.array.itemsize);
    if (prop->end)
        prop->end(&iter);
#else
    auto indices = const_cast<BlenderMesh*>(this)->indices();
    if (!MeshLoop_normal || index < 0 || static_cast<size_t>(index) >= indices.size())
        return ret;

    PointerRNA ptr{};
    ptr.data = &indices[index];
    PointerRNA_OWNER_ID(ptr) = &m_ptr->id;
    ptr.type = MeshLoop_s_type;
    reinterpret_cast<FloatPropertyRNA*>(MeshLoop_normal)->getarray(&ptr, &ret.x);
#endif
    return ret;
}

void BlenderMesh::set_material_index(int index, int value)
{
    if (!MeshPolygon_material_index || index < 0 || index >= m_ptr->faces_num)
        return;

    PointerRNA ptr{};
    ptr.data = &m_ptr->face_offset_indices[index];
    PointerRNA_OWNER_ID(ptr) = &m_ptr->id;
    ptr.type = MeshPolygon_s_type;
    reinterpret_cast<IntPropertyRNA*>(MeshPolygon_material_index)->set(&ptr, value);
}

int BlenderMesh::material_index(int index) const
{
    if (!MeshPolygon_material_index || index < 0 || index >= m_ptr->faces_num)
        return 0;
    PointerRNA ptr{};
    ptr.data = &m_ptr->face_offset_indices[index];
    ptr.owner_id = &m_ptr->id;
    ptr.type = MeshPolygon_s_type;
    return reinterpret_cast<IntPropertyRNA*>(MeshPolygon_material_index)->get(&ptr);
}
#else
barray_range<MLoop> BlenderMesh::indices()
{
#if BLENDER_VERSION >= 304
    return{ (MLoop*)CustomData_get(m_ptr->ldata, CD_MLOOP), (size_t)m_ptr->totloop };
#else
    return { m_ptr->mloop, (size_t)m_ptr->totloop };
#endif
}
barray_range<MEdge> BlenderMesh::edges()
{
    return { m_ptr->medge, (size_t)m_ptr->totedge };
}
barray_range<MPoly> BlenderMesh::polygons()
{
#if BLENDER_VERSION >= 304
    return { (MPoly*)CustomData_get(m_ptr->pdata, CD_MPOLY), (size_t)m_ptr->totpoly };
#else
    return { m_ptr->mpoly, (size_t)m_ptr->totpoly };
#endif
}

barray_range<MVert> BlenderMesh::vertices()
{
#if BLENDER_VERSION >= 304
    return { (MVert*)CustomData_get(m_ptr->vdata, CD_MVERT),(size_t) m_ptr->totvert};
#else
    return { m_ptr->mvert, (size_t)m_ptr->totvert };
#endif
}
#endif
barray_range<mu::float3> BlenderMesh::normals()
{
#if BLENDER_VERSION >= 405
    if (msCustomData_number_of_layers(&m_ptr->corner_data, CD_NORMAL) > 0) {
        auto data = (mu::float3*)CustomData_get(m_ptr->corner_data, CD_NORMAL);
        if (data != nullptr)
            return { data, (size_t)m_ptr->corners_num };
    }
#else
    if (msCustomData_number_of_layers(&m_ptr->ldata, CD_NORMAL) > 0) {
        auto data = (mu::float3*)CustomData_get(m_ptr->ldata, CD_NORMAL);
        if (data != nullptr)
            return { data, (size_t)m_ptr->totloop };
    }
#endif
    return { nullptr, (size_t)0 };
}

#if BLENDER_VERSION >= 304
barray_range<int> BlenderMesh::material_indices()
{
#if BLENDER_VERSION >= 501
    return { nullptr, 0 };
#elif BLENDER_VERSION >= 405
    auto layer = (int*)msCustomData_get_layer_named(&m_ptr->face_data, CD_PROP_INT32, "material_index");
    if (layer)
        return { layer, (size_t)m_ptr->faces_num };
#else
    auto layer = (int*)msCustomData_get_layer_named(&m_ptr->pdata, CD_PROP_INT32, "material_index");
    if (layer)
        return { layer, (size_t)m_ptr->totpoly };
#endif

    return { nullptr, (size_t)0 };
}
#endif

//----------------------------------------------------------------------------------------------------------------------

uint32_t BlenderMesh::GetNumUVs() const
{
#if BLENDER_VERSION >= 501
    if (!BMesh_uv_layers)
        return 0;
    PointerRNA ptr{};
    ptr.owner_id = &m_ptr->id;
    ptr.type = s_type;
    ptr.data = m_ptr;
    auto* prop = reinterpret_cast<CollectionPropertyRNA*>(BMesh_uv_layers);
    return prop->length ? static_cast<uint32_t>(prop->length(&ptr)) : 0;
#elif BLENDER_VERSION >= 405
    return msCustomData_number_of_layers(&m_ptr->corner_data, CD_PROP_FLOAT2);
#else
    return msCustomData_number_of_layers(&m_ptr->ldata, CD_MLOOPUV);
#endif
}

#if BLENDER_VERSION >= 501
const ::blender::float2* BlenderMesh::GetUV(const int index) const {
    if (!BMesh_uv_layers || index < 0)
        return nullptr;

    auto* layers = reinterpret_cast<CollectionPropertyRNA*>(BMesh_uv_layers);
    PointerRNA mesh_ptr{};
    mesh_ptr.owner_id = &m_ptr->id;
    mesh_ptr.type = s_type;
    mesh_ptr.data = m_ptr;
    CollectionPropertyIterator layer_iter{};
    layers->begin(&layer_iter, &mesh_ptr);

    const ::blender::float2* result = nullptr;
    for (int i = 0; layer_iter.valid && i <= index; ++i) {
        if (i == index) {
            PointerRNA layer = layers->get(&layer_iter);
            auto* data = reinterpret_cast<CollectionPropertyRNA*>(MeshUVLoopLayer_uv);
            if (data) {
                CollectionPropertyIterator data_iter{};
                data->begin(&data_iter, &layer);
                result = reinterpret_cast<const ::blender::float2*>(data_iter.internal.array.ptr);
            }
            break;
        }
        layers->next(&layer_iter);
    }
    if (layers->end)
        layers->end(&layer_iter);
    return result;
}
#elif BLENDER_VERSION >= 405
const ::blender::float2* BlenderMesh::GetUV(const int index) const {
    return static_cast<const ::blender::float2 *>(
        msCustomData_get_layer_n(&m_ptr->corner_data, CD_PROP_FLOAT2, index));
}
#else
MLoopUV* BlenderMesh::GetUV(const int index) const {
    return static_cast<MLoopUV *>(msCustomData_get_layer_n(&m_ptr->ldata, CD_MLOOPUV, index));
}
#endif

//----------------------------------------------------------------------------------------------------------------------


barray_range<MLoopCol> BlenderMesh::colors()
{
#if BLENDER_VERSION >= 501
    if (!LoopColors_active || !MeshLoopColorLayer_data)
        return { nullptr, 0 };

    PointerRNA mesh_ptr{&m_ptr->id, s_type, m_ptr};
    PointerRNA layer = reinterpret_cast<PointerPropertyRNA*>(LoopColors_active)->get(&mesh_ptr);
    if (!layer.data)
        return { nullptr, 0 };

    auto* data = reinterpret_cast<CollectionPropertyRNA*>(MeshLoopColorLayer_data);
    CollectionPropertyIterator iter{};
    data->begin(&iter, &layer);
    if (!iter.valid || !iter.internal.array.ptr || iter.internal.array.length <= 0)
        return { nullptr, 0 };
    return { reinterpret_cast<MLoopCol*>(iter.internal.array.ptr),
             static_cast<size_t>(iter.internal.array.length) };
#else
    auto layer_data = (CustomDataLayer*)get_pointer(m_ptr, LoopColors_active);
    if (layer_data && layer_data->data)
#if BLENDER_VERSION >= 405
        return { (MLoopCol*)layer_data->data, (size_t)m_ptr->corners_num };
#else
        return { (MLoopCol*)layer_data->data, (size_t)m_ptr->totloop };
#endif
    else
        return { nullptr, (size_t)0 };
#endif
}

void BlenderMesh::calc_normals_split()
{
#if BLENDER_VERSION >= 405
    if (!BMesh_calc_normals_split)
        return;
#endif
    call<Mesh, void>(g_context, m_ptr, BMesh_calc_normals_split);
}

void BlenderMesh::update()
{
#if BLENDER_VERSION >= 405
    // Mesh.update(calc_edges=False, calc_edges_loose=False). RNA pads every parameter
    // to 8 bytes (rna_parameter_size_pad), so pass the bools as 8-byte slots.
    call<Mesh, void, uint64_t, uint64_t>(g_context, m_ptr, BMesh_update, 0, 0);
#else
    call<Mesh, void>(g_context, m_ptr, BMesh_update);    
#endif
}

void BlenderMesh::clear_geometry()
{
    call<Mesh, void>(g_context, m_ptr, BMesh_clear_geometry);
}

void BlenderMesh::add_vertices(int count) {
    call<Mesh, void, int>(g_context, m_ptr, BMesh_vertices_add, count);
}

void BlenderMesh::add_polygons(int count) {
    call<Mesh, void, int>(g_context, m_ptr, BMesh_polygons_add, count);
}

void BlenderMesh::add_loops(int count) {
    call<Mesh, void, int>(g_context, m_ptr, BMesh_loops_add, count);
}

void BlenderMesh::add_edges(int count) {
    call<Mesh, void, int>(g_context, m_ptr, BMesh_edges_add, count);
}

void BlenderMesh::add_normals(int count) {
    call<Mesh, void, int>(g_context, m_ptr, BMesh_normals_add, count);
}

barray_range<BMFace*> BEditMesh::polygons()
{
    return { m_ptr->bm->ftable, (size_t)m_ptr->bm->ftable_tot };
}

barray_range<BMVert*> BEditMesh::vertices()
{
    return { m_ptr->bm->vtable, (size_t)m_ptr->bm->vtable_tot };
}

barray_range<BMTriangle> BEditMesh::triangles()
{
#if BLENDER_VERSION >= 405
    return { m_ptr->looptris.data(), static_cast<size_t>(m_ptr->looptris.size()) };
#else
    return barray_range<BMTriangle> { m_ptr->looptris, (size_t)m_ptr->tottri };
#endif
}

int BEditMesh::uv_data_offset(int index) const
{
#if BLENDER_VERSION >= 405
    int layer_index = msCustomData_get_layer_index_n(&m_ptr->bm->ldata, CD_PROP_FLOAT2, index);
#else
    int layer_index = msCustomData_get_layer_index_n(&m_ptr->bm->ldata, CD_MLOOPUV, index);
#endif
    if (layer_index == -1) {
        return NULL;
    }

    auto layer = m_ptr->bm->ldata.layers[layer_index];
    return layer.offset;
}

void BNurb::add_bezier_points(int count, Object* obj) {
    call<Nurb, void, int>(g_context, m_ptr, BNurb_splines_bezier_add, count, obj->id);
}

void BCurve::clear_splines() {
    call<Curve, void>(g_context, m_ptr, BCurve_splines_clear);
}

Nurb* BCurve::new_spline() {
// In blender 3.3, the pointer moves 8 instead of 4 bytes:
#if BLENDER_VERSION >= 303
    return call<Curve, Nurb*, long long>(g_context, m_ptr, BCurve_splines_new, CU_BEZIER);
#else
    return call<Curve, Nurb*, int>(g_context, m_ptr, BCurve_splines_new, CU_BEZIER);
#endif
}

const char *BMaterial::name() const
{
    return m_ptr->id.name + 2;
}
const mu::float3& BMaterial::color() const
{
    return (mu::float3&)m_ptr->r;
}
bool BMaterial::use_nodes() const
{
    return get_bool(m_ptr, BMaterial_use_nodes);
}
Material * BMaterial::active_node_material() const
{
    return (Material*)get_pointer(m_ptr, BMaterial_active_node_material);
}

float BCamera::clip_start() const { return get_float(m_ptr, BCamera_clip_start); }
float BCamera::clip_end() const { return get_float(m_ptr, BCamera_clip_end); }
float BCamera::angle_y() const { return get_float(m_ptr, BCamera_angle_y); }
float BCamera::angle_x() const { return get_float(m_ptr, BCamera_angle_x); }
float BCamera::lens() const { return get_float(m_ptr, BCamera_lens); }
int   BCamera::sensor_fit() const { return GetInt(m_ptr, BCamera_sensor_fit); }
float BCamera::sensor_width() const { return get_float(m_ptr, BCamera_sensor_width); }
float BCamera::sensor_height() const { return get_float(m_ptr, BCamera_sensor_height); }
float BCamera::shift_x() const { return get_float(m_ptr, BCamera_shift_x); }
float BCamera::shift_y() const { return get_float(m_ptr, BCamera_shift_y); }

blist_range<Object> BData::objects() {
    return list_range((Object*)m_ptr->objects.first);
}

blist_range<Mesh> BData::meshes(){
    return list_range((Mesh*)m_ptr->meshes.first);
}

blist_range<Material> BData::materials(){
    return list_range((Material*)m_ptr->materials.first);
}

blist_range<Collection> BData::collections() {
    return list_range((Collection*)m_ptr->collections.first);
}

bool BData::objects_is_updated() {
    return true; //before 2.80: get_bool(m_ptr, BlendDataObjects_is_updated);
}

void BData::remove(Mesh * v)
{
    PointerRNA t = {};
    t.data = v;
    call<Main, void, PointerRNA*>(g_context, m_ptr, BlendDataMeshes_remove, &t);
}

const void* CustomData_get(const CustomData& data, int type)
{
    int layer_index = data.typemap[type];
    if (layer_index == -1)
        return nullptr;
    layer_index = layer_index + data.layers[layer_index].active;
    return data.layers[layer_index].data;
}

int CustomData_get_offset(const CustomData& data, int type)
{
    int layer_index = data.typemap[type];
    if (layer_index == -1)
        return -1;

    return data.layers[layer_index].offset;
}


mu::float3 BM_loop_calc_face_normal(const BMLoop& l)
{
    float r_normal[3];
    float v1[3], v2[3];
    sub_v3_v3v3(v1, l.prev->v->co, l.v->co);
    sub_v3_v3v3(v2, l.next->v->co, l.v->co);

    cross_v3_v3v3(r_normal, v1, v2);
    const float len = normalize_v3(r_normal);
    if (UNLIKELY(len == 0.0f)) {
        copy_v3_v3(r_normal, l.f->no);
    }
    return (mu::float3&)r_normal;
}

std::string abspath(const std::string& path, const std::string& libName)
{
    try {
        auto global = py::dict();
        auto local = py::dict();
        local["path"] = py::str(path);
        local["libName"] = py::str(libName);
        py::eval<py::eval_mode::eval_statements>(
            "import bpy.path\n"
            "lib = bpy.data.libraries[libName] if libName else None\n"
            "ret = bpy.path.abspath(path, library=lib)\n"
            , global, local);
        return (py::str)local["ret"];
    }
    catch (py::error_already_set& e) {
        muLogError("%s\n", e.what());
        return path;
    }
}

std::string getBlenderVersion()
{
    try {
        auto global = py::dict();
        auto local = py::dict();
        py::eval<py::eval_mode::eval_statements>(
            "import bpy\n"
            "ret = bpy.app.version_string"
            , global, local);
        return (py::str)local["ret"];
    }
    catch (py::error_already_set& e) {
        muLogError("%s\n", e.what());
        return "";
    }
}

/**
 * Calls a python method that takes no arguments.
 */
void callPythonMethod(const char* name) {
    py::gil_scoped_acquire acquire;

    try {
        auto statement = Format("import MeshSyncClientBlender\n" 
            "from MeshSyncClientBlender.unity_mesh_sync_common import *\n"
            "try: %s()\n"
            "except Exception as e: print(e)", name);
        
        py::eval<py::eval_mode::eval_statements>(
            statement.c_str());
    }
    catch (py::error_already_set& e) {
        muLogError("%s\n", e.what());
    }

    py::gil_scoped_release release;
}

} // namespace blender
