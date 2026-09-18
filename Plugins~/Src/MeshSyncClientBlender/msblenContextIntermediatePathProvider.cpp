#include "msblenContextIntermediatePathProvider.h"
#include "msblenUtils.h"
#include <BLI_listbase.h>

std::string msblenContextIntermediatePathProvider::append_id(std::string path, const Object* obj) {
    auto data = (ID*)obj->data;

    path += "_" + std::string(data->name);

    // If we already have an object with this name but a different session ID, append it as well.
    auto it = mappedNames.find(data->name);
    const auto session_id = msblenUtils::get_session_id(data);

    if (it == mappedNames.end()) {
        mappedNames.insert(std::make_pair(data->name, session_id));
    }
    else if (it->second != session_id)
    {
        path += "_" + std::to_string(session_id);
    }

    return path;
}

std::string msblenContextIntermediatePathProvider::get_path(const Object* obj, const Bone* bone)
{
    std::string path;
    if (bone) {
        path = msblenUtils::get_path(obj, bone);
    }
    else {
        path = "/" + msblenUtils::get_name(obj);
    }

    return append_id(path, obj);
}
