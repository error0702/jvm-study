# 内存池初始化

## 1. 内存池初始化步骤
`Threads::create_vm() thread.cpp` </br>
`init_globals() init.cpp` </br>
`universe_init() universe.cpp` </br>
`Universe::initialize_heap() universe.cpp` </br>
###


// Compute hard-coded offsets
// Invoked before SystemDictionary::initialize, so pre-loaded classes
// are not available to determine the offset_of_static_fields.
void JavaClasses::compute_hard_coded_offsets()