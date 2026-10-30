# Replacement policies (owner: configuration track). Appends to ccsim_core.
target_sources(ccsim_core PRIVATE
  policy_factory.cpp
  lru.cpp
  fifo.cpp
  random.cpp)
