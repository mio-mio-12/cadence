#pragma once
namespace render {
struct CullCounters { int queries{}, enables{}, faces{}; };
// One render invocation only. Enabled starts known after the renderer's reset;
// face remains unknown because that reset deliberately preserves inherited mode.
struct CullState {
  bool cache{}, known{true}, enabled{};
  int face{-1};
  CullCounters& counters;
  void invalidate() { known=false;face=-1; }
  template<class Query> bool get(Query query) {
    if (!cache || !known) { ++counters.queries;enabled=query();known=true; }
    return enabled;
  }
  template<class Write> void set(bool value, Write write) {
    if (!cache || !known || value!=enabled) { ++counters.enables;write(value); }
    enabled=value;known=true;
  }
  template<class Write> void setFace(int value, Write write) {
    if (!cache || face!=value) { ++counters.faces;write(value); }
    face=value;
  }
};
}
