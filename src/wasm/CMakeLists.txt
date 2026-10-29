# C ABI over ccsim_core (docs/ARCHITECTURE.md section 8).
add_library(ccsim_api STATIC ccsim_api.cpp)
target_link_libraries(ccsim_api PUBLIC ccsim_core)
target_compile_definitions(ccsim_api PRIVATE CCSIM_VERSION="${PROJECT_VERSION}")
ccsim_set_warnings(ccsim_api)

# Browser module: only with the Emscripten toolchain (emcmake cmake ...). "node" is in
# ENVIRONMENT so CI can load the same artefact in tests/wasm/smoke.mjs. Not tested locally.
if(EMSCRIPTEN)
  add_executable(ccsim_wasm wasm_main.cpp)
  target_link_libraries(ccsim_wasm PRIVATE ccsim_api)
  if(CCSIM_HAS_PRODUCTION_POLICIES)
    target_compile_definitions(ccsim_wasm PRIVATE CCSIM_HAS_PRODUCTION_POLICIES=1)
  endif()
  ccsim_set_warnings(ccsim_wasm)
  set(CCSIM_WASM_EXPORTS
    _ccsim_api_version _ccsim_last_error _ccsim_alloc _ccsim_free
    _ccsim_trace_begin _ccsim_trace_feed _ccsim_trace_end _ccsim_trace_size
    _ccsim_trace_fingerprint _ccsim_trace_release
    _ccsim_session_create _ccsim_session_release _ccsim_session_position
    _ccsim_session_reset _ccsim_session_seek _ccsim_session_step _ccsim_session_run
    _ccsim_event_buffer _ccsim_event_buffer_words
    _ccsim_session_slot_states _ccsim_session_slot_tags
    _ccsim_session_report_json _ccsim_compare_json _ccsim_compare_csv _main)
  list(JOIN CCSIM_WASM_EXPORTS "," CCSIM_WASM_EXPORT_LIST)
  set_target_properties(ccsim_wasm PROPERTIES OUTPUT_NAME "ccsim" SUFFIX ".mjs")
  target_link_options(ccsim_wasm PRIVATE
    "-sMODULARIZE=1" "-sEXPORT_ES6=1" "-sENVIRONMENT=worker,node"
    "-sALLOW_MEMORY_GROWTH=1" "-sMAXIMUM_MEMORY=4GB" "-sWASM_BIGINT=1"
    "-sEXPORTED_FUNCTIONS=${CCSIM_WASM_EXPORT_LIST}"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,HEAPU8,HEAPU32,HEAPU64")
endif()
