#ifndef STRATA3D_CORE_NERVE_TRACE_H
#define STRATA3D_CORE_NERVE_TRACE_H

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <vector>

namespace strata3d {

using Point3 = std::array<double, 3>;
using NervePath = std::vector<Point3>;

constexpr double kNerveMinDiameter = 1.0;     // mm
constexpr double kNerveMaxDiameter = 6.0;     // mm
constexpr double kNerveDefaultDiameter = 2.5; // mm

// Points clicked along one or more nerve canals (e.g. left and right). The
// last path is the one being traced.
class NerveTrace {
public:
  void addPoint(const Point3 &point);
  // Removes the last point; from an empty path, steps back into the previous
  // one.
  void undo();
  // Starts the next canal; does nothing while the current one is empty.
  void startNewPath();
  void clear();

  const std::vector<NervePath> &paths() const { return m_paths; }
  int pointCount() const;
  // paths with at least two points, i.e. usable for a tube
  int canalCount() const;
  bool empty() const { return pointCount() == 0; }

private:
  std::vector<NervePath> m_paths;
};

// Closed tube of the given diameter (mm) along each path (smoothed by a
// spline); paths with fewer than two points are skipped. Null when no path is
// usable.
vtkSmartPointer<vtkPolyData> buildNerveTube(const std::vector<NervePath> &paths,
                                            double diameter);

} // namespace strata3d

#endif // STRATA3D_CORE_NERVE_TRACE_H
