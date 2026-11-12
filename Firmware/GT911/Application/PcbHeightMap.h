#ifndef OMID_PCB_HEIGHT_MAP_H
#define OMID_PCB_HEIGHT_MAP_H
#include <cstddef>
#include <cstdint>

namespace Pcb
{
struct Context
{
  int32_t offset[3] = {};
  uint32_t tool = 0;
  int32_t tlo = 0;
  uint32_t epoch = 0; // Runtime only, never trusted after a reboot.
  bool Matches(const Context& other, bool check_epoch = true) const;
};

class HeightMap
{
public:
  static constexpr unsigned MAX_AXIS = 21;
  static constexpr unsigned MAX_POINTS = MAX_AXIS * MAX_AXIS;
  int32_t x0 = 0, y0 = 0, span_x = 0, span_y = 0; // micrometres
  unsigned nx = 0, ny = 0;
  Context context;
  bool Configure(int32_t x, int32_t y, int32_t width, int32_t height,
                 unsigned cols, unsigned rows, const Context& ctx);
  void Invalidate();
  bool Set(unsigned x, unsigned y, int32_t surface_z);
  bool Complete();
  bool Valid() const { return valid; }
  unsigned Count() const { return nx * ny; }
  int32_t X(unsigned col) const;
  int32_t Y(unsigned row) const;
  int32_t Height(unsigned col, unsigned row) const;
  bool Contains(double x_mm, double y_mm) const;
  bool Correction(double x_mm, double y_mm, double& z_mm) const;
  double MinZ() const;
  double MaxZ() const;
  double MaxTwist() const;
  uint32_t Hash() const;
  // Ordered, bounded ASCII records. Last record authenticates all map data.
  unsigned RecordCount() const { return Count() + 4; }
  bool WriteRecord(unsigned record, char* out, size_t size) const;
  bool ReadRecord(unsigned record, const char* text);
private:
  int32_t z[MAX_POINTS] = {};
  uint8_t received[(MAX_POINTS + 7) / 8] = {};
  unsigned stored = 0;
  bool valid = false;
};

struct Position { double x = 0, y = 0, z = 0; };

// Bounded single-tool PCB preprocessor. Coordinates are absolute millimetres;
// XY arcs are linearized before height compensation. Never controls hardware.
class Compensator
{
public:
  explicit Compensator(const HeightMap& map, Position initial);
  bool BeginLine(const char* input);
  // 1 = output line, 0 = this source block is finished, -1 = error.
  int Next(char* output, size_t size);
  const char* Error() const { return error; }
  Position Current() const { return current; }
  static constexpr double Z_TOLERANCE = 0.001;
  static constexpr double ARC_TOLERANCE = 0.001;
private:
  const HeightMap& map;
  Position current, line_start, line_end, block_start, block_end;
  double t = 1, dt = 1, feed = 0;
  double cx = 0, cy = 0, radius = 0, angle = 0, sweep = 0;
  unsigned arc_step = 0, arc_steps = 0;
  int motion = -1, output_motion = 0;
  bool metric = false, absolute = false, ended = false;
  bool prefix_ready = false, linear_pending = false;
  char prefix[80] = {};
  const char* error = nullptr;
  bool Fail(const char* message);
  bool PrepareLinear(Position from, Position to);
  bool PrepareArc(Position from, Position to, bool clockwise,
                  bool has_i, double i, bool has_j, double j,
                  bool has_r, double r);
};
}
#endif
