#include "software_renderer.h"

#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>
#include <numeric>

#include "triangulation.h"

using namespace std;

namespace CMU462
{

  // Implements SoftwareRenderer //

  void SoftwareRendererImp::draw_svg(SVG &svg)
  {

    // set top level transformation
    transformation = svg_2_screen;

    // draw all elements
    for (size_t i = 0; i < svg.elements.size(); ++i)
    {
      draw_element(svg.elements[i]);
    }

    // draw canvas outline
    Vector2D a = transform(Vector2D(0, 0));
    a.x--;
    a.y--;
    Vector2D b = transform(Vector2D(svg.width, 0));
    b.x++;
    b.y--;
    Vector2D c = transform(Vector2D(0, svg.height));
    c.x--;
    c.y++;
    Vector2D d = transform(Vector2D(svg.width, svg.height));
    d.x++;
    d.y++;

    rasterize_line(a.x, a.y, b.x, b.y, Color::Black);
    rasterize_line(a.x, a.y, c.x, c.y, Color::Black);
    rasterize_line(d.x, d.y, b.x, b.y, Color::Black);
    rasterize_line(d.x, d.y, c.x, c.y, Color::Black);

    // resolve and send to render target
    resolve();
  }

  void SoftwareRendererImp::set_sample_rate(size_t sample_rate)
  {

    // Task 4:
    // You may want to modify this for supersampling support
    switch (sample_rate)
    {
    case 1:
      this->sample_rate = 1;
      this->sample_selection_map = {};
      break;
    case 2:
      this->sample_rate = 4;
      this->sample_selection_map = {-.25, .25};
      break;
    case 3:
      this->sample_rate = 9;
      this->sample_selection_map = {-.25, 0, .25};
      break;
    case 4:
      this->sample_rate = 16;
      this->sample_selection_map = {-.75, -.15, .15, .75};
      break;
    }
    this->sample_buffer.resize(this->sample_rate);
  }

  void SoftwareRendererImp::set_render_target(unsigned char *render_target,
                                              size_t width, size_t height)
  {

    // Task 4:
    // You may want to modify this for supersampling support
    this->render_target = render_target;
    this->target_w = width;
    this->target_h = height;
  }

  Vector2D SoftwareRendererImp::transform_point(float x, float y, Matrix3x3 matrix)
  {
    Vector3D homogeneous_point(x, y, 1);
    Vector3D homogeneous_point_prime = matrix * homogeneous_point;
    Vector2D pair(homogeneous_point_prime[0], homogeneous_point_prime[1]);
    return pair;
  }

  Vector2D SoftwareRendererImp::transform_vector(float x, float y, Matrix3x3 matrix)
  {
    Vector3D homogeneous_vector(x, y, 0);
    Vector3D homogeneous_vector_prime = matrix * homogeneous_vector;
    Vector2D pair(homogeneous_vector[0], homogeneous_vector[1]);
    return pair;
  }

  void SoftwareRendererImp::apply_transform(SVGElement *element)
  {
    Vector2D point;

    if (element->type == POINT)
    {
      point = SoftwareRendererImp::transform_point(static_cast<Point &>(*element).position.x, static_cast<Point &>(*element).position.y, static_cast<Point &>(*element).transform);
      static_cast<Point &>(*element).position = point;
    }
    else if (element->type == LINE)
    {
      point = SoftwareRendererImp::transform_point(static_cast<Line &>(*element).from.x, static_cast<Line &>(*element).from.y, static_cast<Line &>(*element).transform);
      static_cast<Line &>(*element).from = point;
      point = SoftwareRendererImp::transform_point(static_cast<Line &>(*element).to.x, static_cast<Line &>(*element).to.y, static_cast<Line &>(*element).transform);
      static_cast<Line &>(*element).to = point;
    }
    else if (element->type == POLYLINE || element->type == POLYGON)
    {
      for (int i = 0; i < static_cast<Polyline &>(*element).points.size(); i++)
      {
        point = SoftwareRendererImp::transform_point(static_cast<Polyline &>(*element).points[i].x, static_cast<Polyline &>(*element).points[i].y, static_cast<Polyline &>(*element).transform);
        static_cast<Polyline &>(*element).points[i] = point;
      }
    }
    else if (element->type == RECT)
    {
      point = SoftwareRendererImp::transform_point(static_cast<Rect &>(*element).position.x, static_cast<Rect &>(*element).position.y, static_cast<Rect &>(*element).transform);
      static_cast<Rect &>(*element).position = point;
      point = SoftwareRendererImp::transform_vector(static_cast<Rect &>(*element).dimension.x, static_cast<Rect &>(*element).dimension.y, static_cast<Rect &>(*element).transform);
      static_cast<Rect &>(*element).dimension = point;
    }
    else if (element->type == ELLIPSE)
    {
      point = SoftwareRendererImp::transform_point(static_cast<Ellipse &>(*element).center.x, static_cast<Ellipse &>(*element).center.y, static_cast<Ellipse &>(*element).transform);
      static_cast<Ellipse &>(*element).center = point;
      point = SoftwareRendererImp::transform_vector(static_cast<Ellipse &>(*element).radius.x, static_cast<Ellipse &>(*element).radius.y, static_cast<Ellipse &>(*element).transform);
      static_cast<Ellipse &>(*element).radius = point;
    }
    else if (element->type == IMAGE)
    {
    }
  }

  void SoftwareRendererImp::draw_element(SVGElement *element)
  {

    // Task 5 (part 1):
    // Modify this to implement the transformation stack
    if (element->type == GROUP)
    {
      for (int i = 0; i < static_cast<Group &>(*element).elements.size(); i++)
      {
        static_cast<Group &>(*element).elements[i]->transform = static_cast<Group &>(*element).elements[i]->transform * static_cast<Group &>(*element).transform;
        SoftwareRendererImp::apply_transform(static_cast<Group &>(*element).elements[i]);
      }
    }
    else
    {
      SoftwareRendererImp::apply_transform(element);
    }

    switch (element->type)
    {
    case POINT:
      draw_point(static_cast<Point &>(*element));
      break;
    case LINE:
      draw_line(static_cast<Line &>(*element));
      break;
    case POLYLINE:
      draw_polyline(static_cast<Polyline &>(*element));
      break;
    case RECT:
      draw_rect(static_cast<Rect &>(*element));
      break;
    case POLYGON:
      draw_polygon(static_cast<Polygon &>(*element));
      break;
    case ELLIPSE:
      draw_ellipse(static_cast<Ellipse &>(*element));
      break;
    case IMAGE:
      draw_image(static_cast<Image &>(*element));
      break;
    case GROUP:
      draw_group(static_cast<Group &>(*element));
      break;
    default:
      break;
    }
  }

  // Primitive Drawing //

  void SoftwareRendererImp::draw_point(Point &point)
  {

    Vector2D p = transform(point.position);
    rasterize_point(p.x, p.y, point.style.fillColor);
  }

  void SoftwareRendererImp::draw_line(Line &line)
  {

    Vector2D p0 = transform(line.from);
    Vector2D p1 = transform(line.to);
    rasterize_line(p0.x, p0.y, p1.x, p1.y, line.style.strokeColor);
  }

  void SoftwareRendererImp::draw_polyline(Polyline &polyline)
  {

    Color c = polyline.style.strokeColor;

    if (c.a != 0)
    {
      int nPoints = polyline.points.size();
      for (int i = 0; i < nPoints - 1; i++)
      {
        Vector2D p0 = transform(polyline.points[(i + 0) % nPoints]);
        Vector2D p1 = transform(polyline.points[(i + 1) % nPoints]);
        rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
      }
    }
  }

  void SoftwareRendererImp::draw_rect(Rect &rect)
  {

    Color c;

    // draw as two triangles
    float x = rect.position.x;
    float y = rect.position.y;
    float w = rect.dimension.x;
    float h = rect.dimension.y;

    Vector2D p0 = transform(Vector2D(x, y));
    Vector2D p1 = transform(Vector2D(x + w, y));
    Vector2D p2 = transform(Vector2D(x, y + h));
    Vector2D p3 = transform(Vector2D(x + w, y + h));

    // draw fill
    c = rect.style.fillColor;
    if (c.a != 0)
    {
      rasterize_triangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c);
      rasterize_triangle(p2.x, p2.y, p1.x, p1.y, p3.x, p3.y, c);
    }

    // draw outline
    c = rect.style.strokeColor;
    if (c.a != 0)
    {
      rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
      rasterize_line(p1.x, p1.y, p3.x, p3.y, c);
      rasterize_line(p3.x, p3.y, p2.x, p2.y, c);
      rasterize_line(p2.x, p2.y, p0.x, p0.y, c);
    }
  }

  void SoftwareRendererImp::draw_polygon(Polygon &polygon)
  {

    Color c;

    // draw fill
    c = polygon.style.fillColor;
    if (c.a != 0)
    {

      // triangulate
      vector<Vector2D> triangles;
      triangulate(polygon, triangles);

      // draw as triangles
      for (size_t i = 0; i < triangles.size(); i += 3)
      {
        Vector2D p0 = transform(triangles[i + 0]);
        Vector2D p1 = transform(triangles[i + 1]);
        Vector2D p2 = transform(triangles[i + 2]);
        rasterize_triangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c);
      }
    }

    // draw outline
    c = polygon.style.strokeColor;
    if (c.a != 0)
    {
      int nPoints = polygon.points.size();
      for (int i = 0; i < nPoints; i++)
      {
        Vector2D p0 = transform(polygon.points[(i + 0) % nPoints]);
        Vector2D p1 = transform(polygon.points[(i + 1) % nPoints]);
        rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
      }
    }
  }

  void SoftwareRendererImp::draw_ellipse(Ellipse &ellipse)
  {

    // Extra credit
  }

  void SoftwareRendererImp::draw_image(Image &image)
  {

    Vector2D p0 = transform(image.position);
    Vector2D p1 = transform(image.position + image.dimension);

    rasterize_image(p0.x, p0.y, p1.x, p1.y, image.tex);
  }

  void SoftwareRendererImp::draw_group(Group &group)
  {

    for (size_t i = 0; i < group.elements.size(); ++i)
    {
      draw_element(group.elements[i]);
    }
  }

  // Rasterization //

  // The input arguments in the rasterization functions
  // below are all defined in screen space coordinates

  void SoftwareRendererImp::rasterize_point(float x, float y, Color color)
  {

    // fill in the nearest pixel
    int sx = (int)floor(x);
    int sy = (int)floor(y);
    int alpha = 255;
    if (this->sample_rate > 1)
    {
      alpha *= (std::accumulate(this->sample_buffer.begin(), this->sample_buffer.end(), 0) / ((float)this->sample_buffer.size()));
    }

    // check bounds
    if (sx < 0 || sx >= target_w)
      return;
    if (sy < 0 || sy >= target_h)
      return;

    // fill sample - NOT doing alpha blending!
    render_target[4 * (sx + sy * target_w)] = (uint8_t)(color.r * 255);
    render_target[4 * (sx + sy * target_w) + 1] = (uint8_t)(color.g * 255);
    render_target[4 * (sx + sy * target_w) + 2] = (uint8_t)(color.b * 255);
    render_target[4 * (sx + sy * target_w) + 3] = (uint8_t)(color.a * alpha);
  }

  std::vector<std::pair<float, float>> SoftwareRendererImp::bresenham_low(float x0, float y0,
                                                                          float x1, float y1)
  {
    float dx = x1 - x0,
          dy = y1 - y0,
          yi = 1, D, y;

    std::vector<pair<float, float>> points;

    if (dy < 0)
    {
      yi = -1;
      dy = -dy;
    }

    D = (2 * dy) - dx;
    y = y0;

    for (int x = (int)floor(x0); x < x1; x++)
    {
      std::pair<float, float> point(x, y);
      points.push_back(point);

      if (D > 0)
      {
        y = y + yi;
        D = D + (2 * (dy - dx));
      }
      else
      {
        D = D + 2 * dy;
      }
    }

    return points;
  }

  std::vector<std::pair<float, float>> SoftwareRendererImp::bresenham_high(float x0, float y0,
                                                                           float x1, float y1)
  {
    float dx = x1 - x0,
          dy = y1 - y0,
          xi = 1, D, x;

    std::vector<pair<float, float>> points;

    if (dx < 0)
    {
      xi = -1;
      dx = -dx;
    }

    D = (2 * dx) - dy;
    x = x0;

    for (int y = (int)floor(y0); y < y1; y++)
    {
      std::pair<float, float> point(x, y);
      points.push_back(point);

      if (D > 0)
      {
        x = x + xi;
        D = D + (2 * (dx - dy));
      }
      else
      {
        D = D + 2 * dx;
      }
    }

    return points;
  }

  std::vector<std::pair<float, float>> SoftwareRendererImp::bresenham(float x0, float y0,
                                                                      float x1, float y1)
  {
    std::fill(this->sample_buffer.begin(), this->sample_buffer.end(), 1);

    if (abs(y1 - y0) < abs(x1 - x0))
    {
      if (x0 > x1)
      {
        return SoftwareRendererImp::bresenham_low(x1, y1, x0, y0);
      }
      else
      {
        return SoftwareRendererImp::bresenham_low(x0, y0, x1, y1);
      }
    }
    else
    {
      if (y0 > y1)
      {
        return SoftwareRendererImp::bresenham_high(x1, y1, x0, y0);
      }
      else
      {
        return SoftwareRendererImp::bresenham_high(x0, y0, x1, y1);
      }
    }
  }

  void SoftwareRendererImp::rasterize_line(float x0, float y0,
                                           float x1, float y1,
                                           Color color)
  {

    // Task 2:
    // Implement line rasterization
    std::vector<std::pair<float, float>> points = SoftwareRendererImp::bresenham(x0, y0, x1, y1);
    for (int i = 0; i < points.size(); i++)
    {
      SoftwareRendererImp::rasterize_point(points[i].first, points[i].second, color);
    }
  }

  bool SoftwareRendererImp::edgeFunction(float xa, float ya,
                                         float xb, float yb,
                                         float xp, float yp)
  {
    return ((yp - ya) * (xb - xa) - (xp - xa) * (yb - ya) >= 0);
  }

  void SoftwareRendererImp::rasterize_triangle(float x0, float y0,
                                               float x1, float y1,
                                               float x2, float y2,
                                               Color color)
  {
    // Task 3:
    // Implement triangle rasterization
    float minX = std::min(x0, x1);
    minX = std::min(minX, x2);

    float minY = std::min(y0, y1);
    minY = std::min(minY, y2);

    float maxX = std::max(x0, x1);
    maxX = std::max(maxX, x2);

    float maxY = std::max(y0, y1);
    maxY = std::max(maxY, y2);

    bool inside;

    for (float x = minX; x < maxX; x++)
    {
      for (float y = minY; y < maxY; y++)
      {
        inside = true;

        inside &= SoftwareRendererImp::edgeFunction(x0, y0, x1, y1, x, y);
        inside &= SoftwareRendererImp::edgeFunction(x1, y1, x2, y2, x, y);
        inside &= SoftwareRendererImp::edgeFunction(x2, y2, x0, y0, x, y);
        if (inside)
        {
          if (this->sample_rate > 1)
          {
            // cout << "\n";
            for (int ix = 0; ix < this->sample_selection_map.size(); ix++)
            {
              for (int iy = 0; iy < this->sample_selection_map.size(); iy++)
              {
                inside = true;

                inside &= SoftwareRendererImp::edgeFunction(x0, y0, x1, y1, x + sample_selection_map[ix], y + sample_selection_map[iy]);
                inside &= SoftwareRendererImp::edgeFunction(x1, y1, x2, y2, x + sample_selection_map[ix], y + sample_selection_map[iy]);
                inside &= SoftwareRendererImp::edgeFunction(x2, y2, x0, y0, x + sample_selection_map[ix], y + sample_selection_map[iy]);
                if (inside)
                {
                  this->sample_buffer[ix + iy] = 1;
                }
                else
                {
                  this->sample_buffer[ix + iy] = 0;
                }
              }
            }
          }
          SoftwareRendererImp::rasterize_point(x, y, color);
          std::fill(this->sample_buffer.begin(), this->sample_buffer.end(), 1);
        }
      }
    }
  }

  void SoftwareRendererImp::rasterize_image(float x0, float y0,
                                            float x1, float y1,
                                            Texture &tex)
  {
    // Task 6:
    // Implement image rasterization
    int minX = std::min(x0, x1),
        minY = std::min(y0, y1),
        maxX = std::max(x0, x1),
        maxY = std::max(y0, y1);
    float normalizedX,
        normalizedY;
    Color color;

    this->sampler->generate_mips(tex, 0);

    for (int x = minX; x < maxX; x++)
    {
      normalizedX = (x - minX) / (maxX - minX);
      for (int y = minY; y < maxY; y++)
      {
        normalizedY = (y - minY) / (maxY - minY);
        color = sampler->sample_nearest(tex, normalizedX, normalizedY);
        rasterize_point(x, y, color);
      }
    }
  }

  // resolve samples to render target
  void SoftwareRendererImp::resolve(void)
  {

    // Task 4:
    // Implement supersampling
    // You may also need to modify other functions marked with "Task 4".
    return;
  }

} // namespace CMU462
