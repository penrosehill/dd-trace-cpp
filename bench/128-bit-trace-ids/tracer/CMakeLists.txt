add_executable(tracer tracer.cpp)

target_link_libraries(tracer dd_trace_cpp)
