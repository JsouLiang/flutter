// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/common/settings.h"

// BD ADD:
#include "third_party/rapidjson/include/rapidjson/prettywriter.h"
#include "third_party/rapidjson/include/rapidjson/document.h"
// END
#include <sstream>

namespace flutter {

constexpr FrameTiming::Phase FrameTiming::kPhases[FrameTiming::kCount];

Settings::Settings() = default;

Settings::Settings(const Settings& other) = default;

Settings::~Settings() = default;

// BD ADD:
void Settings::SetEntryPointArgsJson(std::string entryPointsArgsJson){
  if(entryPointsArgsJson.empty()){
    return;
  }
  rapidjson::Document document;
  if(document.Parse(entryPointsArgsJson.c_str()).HasParseError()){
    FML_LOG(ERROR)<< entryPointsArgsJson <<" is not a json string"<<std::endl;
    return;
  }
  rapidjson::Value key;
  rapidjson::Value value;
  rapidjson::Document::AllocatorType allocator;
  std::vector<std::string> tmp;
  for(rapidjson::Value::ConstMemberIterator iter = document.MemberBegin(); iter!=document.MemberEnd();iter++){
    key.CopyFrom(iter->name,allocator);
    value.CopyFrom(iter->value, allocator);
    if(!key.IsString() || !value.IsString()){
      FML_LOG(ERROR)<< entryPointsArgsJson <<" has a illegal key or value"<<std::endl;
      return;
    }
    tmp.push_back(key.GetString());
    tmp.push_back(value.GetString());
  }
  dart_entrypoint_args = tmp;
}
// END
std::string Settings::ToString() const {
  std::stringstream stream;
  stream << "Settings: " << std::endl;
  stream << "vm_snapshot_data_path: " << vm_snapshot_data_path << std::endl;
  stream << "vm_snapshot_instr_path: " << vm_snapshot_instr_path << std::endl;
  stream << "isolate_snapshot_data_path: " << isolate_snapshot_data_path
         << std::endl;
  stream << "isolate_snapshot_instr_path: " << isolate_snapshot_instr_path
         << std::endl;
  stream << "application_library_path:" << std::endl;
  for (const auto& path : application_library_path) {
    stream << "    " << path << std::endl;
  }
  stream << "temp_directory_path: " << temp_directory_path << std::endl;
  stream << "dart_flags:" << std::endl;
  for (const auto& dart_flag : dart_flags) {
    stream << "    " << dart_flag << std::endl;
  }
  stream << "start_paused: " << start_paused << std::endl;
  stream << "trace_skia: " << trace_skia << std::endl;
  stream << "trace_startup: " << trace_startup << std::endl;
  stream << "trace_systrace: " << trace_systrace << std::endl;
  stream << "dump_skp_on_shader_compilation: " << dump_skp_on_shader_compilation
         << std::endl;
  stream << "cache_sksl: " << cache_sksl << std::endl;
  stream << "purge_persistent_cache: " << purge_persistent_cache << std::endl;
  stream << "endless_trace_buffer: " << endless_trace_buffer << std::endl;
  stream << "enable_dart_profiling: " << enable_dart_profiling << std::endl;
  stream << "disable_dart_asserts: " << disable_dart_asserts << std::endl;
  stream << "enable_observatory: " << enable_observatory << std::endl;
  stream << "enable_observatory_publication: " << enable_observatory_publication
         << std::endl;
  stream << "observatory_host: " << observatory_host << std::endl;
  stream << "observatory_port: " << observatory_port << std::endl;
  stream << "use_test_fonts: " << use_test_fonts << std::endl;
  stream << "enable_software_rendering: " << enable_software_rendering
         << std::endl;
  stream << "log_tag: " << log_tag << std::endl;
  stream << "icu_initialization_required: " << icu_initialization_required
         << std::endl;
  stream << "icu_data_path: " << icu_data_path << std::endl;
  stream << "assets_dir: " << assets_dir << std::endl;
  stream << "assets_path: " << assets_path << std::endl;
  stream << "frame_rasterized_callback set: " << !!frame_rasterized_callback
         << std::endl;
  stream << "old_gen_heap_size: " << old_gen_heap_size << std::endl;

  // BD ADD
  stream << "package_dill_path: " << package_dill_path << std::endl;
  stream << "package_preload_libs: " << package_preload_libs << std::endl;

  return stream.str();
}

}  // namespace flutter
