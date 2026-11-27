#include "core/nerve_trace.h"

#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkNew.h>
#include <vtkParametricFunctionSource.h>
#include <vtkParametricSpline.h>
#include <vtkPoints.h>
#include <vtkTriangleFilter.h>
#include <vtkTubeFilter.h>

#include <algorithm>

namespace strata3d {

namespace {
constexpr int kTubeSides = 24;
constexpr int kMinCurveResolution = 50;
constexpr int kCurveResolutionPerPoint = 20;
} // namespace

void NerveTrace::addPoint(const Point3 &point) {
  if (m_paths.empty())
    m_paths.emplace_back();
  m_paths.back().push_back(point);
}

void NerveTrace::undo() {
  if (m_paths.empty())
    return;
  if (m_paths.back().empty() && m_paths.size() > 1)
    m_paths.pop_back();
  if (!m_paths.back().empty())
    m_paths.back().pop_back();
}

void NerveTrace::startNewPath() {
  if (!m_paths.empty() && !m_paths.back().empty())
    m_paths.emplace_back();
}

void NerveTrace::clear() { m_paths.clear(); }

int NerveTrace::pointCount() const {
  int n = 0;
  for (const NervePath &p : m_paths)
    n += static_cast<int>(p.size());
  return n;
}

int NerveTrace::canalCount() const {
  return static_cast<int>(
      std::count_if(m_paths.begin(), m_paths.end(),
                    [](const NervePath &p) { return p.size() >= 2; }));
}

vtkSmartPointer<vtkPolyData> buildNerveTube(const std::vector<NervePath> &paths,
                                            double diameter) {
  vtkNew<vtkAppendPolyData> all;
  int used = 0;
  for (const NervePath &path : paths) {
    if (path.size() < 2)
      continue;
    vtkNew<vtkPoints> points;
    for (const Point3 &p : path)
      points->InsertNextPoint(p.data());
    vtkNew<vtkParametricSpline> spline;
    spline->SetPoints(points);
    vtkNew<vtkParametricFunctionSource> curve;
    curve->SetParametricFunction(spline);
    curve->SetUResolution(
        std::max<int>(kMinCurveResolution,
                      kCurveResolutionPerPoint * static_cast<int>(path.size())));

    vtkNew<vtkTubeFilter> tube;
    tube->SetInputConnection(curve->GetOutputPort());
    tube->SetRadius(diameter / 2.0);
    tube->SetNumberOfSides(kTubeSides);
    tube->CappingOn();
    vtkNew<vtkTriangleFilter> triangles;
    triangles->SetInputConnection(tube->GetOutputPort());
    all->AddInputConnection(triangles->GetOutputPort());
    ++used;
  }
  if (used == 0)
    return nullptr;

  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(all->GetOutputPort());
  clean->Update();
  vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
  mesh->DeepCopy(clean->GetOutput());
  return mesh;
}

} // namespace strata3d
