#pragma once

#if BLENDER_VERSION >= 405
#include <BKE_context.hh> //bContext
#else
#include <BKE_context.h> //bContext
#endif
#include "MeshUtils/muMath.h"

namespace blender
{
    class BlenderPyDepsgraphInstance
    {
        public:
            BlenderPyDepsgraphInstance(PointerRNA& instance) : m_instance(instance) {}

            Object* instance_object();
            bool is_instance();
            void world_matrix(mu::float4x4* world_matrix);
            Object* parent();
            Object* object();
    private:
        PointerRNA& m_instance;
    };

} // namespace blender
