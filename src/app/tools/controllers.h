// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "base/pi.h"

#include <cmath>

namespace app {
namespace tools {

using namespace gfx;

// Shared logic between controllers that can move/displace all points
// using the space bar.
class MoveOriginCapability : public Controller {
public:
  void pressButton(Stroke& stroke, const Point& point, float pressure) override {
    m_last = point;
  }

protected:
  bool isMovingOrigin(ToolLoop* loop, Stroke& stroke, const Point& point) {
    bool used = false;

    if (int(loop->getModifiers()) & int(ToolLoopModifiers::kMoveOrigin)) {
      Point delta = (point - m_last);
      stroke.offset(delta);

      onMoveOrigin(delta);
      used = true;
    }

    m_last = point;
    return used;
  }

  virtual void onMoveOrigin(const Point& delta) {
    // Do nothing
  }

private:
  // Last known mouse position used to calculate delta values (dx, dy)
  // with the new mouse position to displace all points.
  Point m_last;
};

// Controls clicks for tools like pencil
class FreehandController : public Controller {
public:
  bool isFreehand() override { return true; }

  void pressButton(Stroke& stroke, const gfx::Point& point, float pressure) override {
    stroke.addPoint({point.x, point.y, pressure});
  }

  bool releaseButton(Stroke& stroke, const Point& point) override {
    return false;
  }

  void movement(ToolLoop* loop, Stroke& stroke, const gfx::Point& point, float pressure) override {
    stroke.addPoint({point.x, point.y, pressure});
  }

  void getStrokeToInterwine(const Stroke& input, Stroke& output) override {
    if (input.size() == 1) {
      output.addPoint(input[0]);
    }
    else if (input.size() >= 2) {
      output.addPoint(input[input.size()-2]);
      output.addPoint(input[input.size()-1]);
    }
  }

  void getStatusBarText(const Stroke& stroke, std::string& text) override {
    ASSERT(!stroke.empty());
    if (stroke.empty())
      return;

    char buf[1024];
    snprintf(buf, sizeof(buf), ":start: %3d %3d :end: %3d %3d",
            stroke.firstPoint().x,
            stroke.firstPoint().y,
            stroke.lastPoint().x,
            stroke.lastPoint().y);
    text = buf;
  }

};

// Controls clicks for tools like line
class TwoPointsController : public MoveOriginCapability {
public:
  void pressButton(Stroke& stroke, const Point& point, float pressure) override {
    MoveOriginCapability::pressButton(stroke, point, pressure);
    Stroke::Point strokePoint{point.x, point.y, pressure};
    m_first = strokePoint;
    stroke.addPoint(strokePoint);
    stroke.addPoint(strokePoint);
  }

  bool releaseButton(Stroke& stroke, const Point& point) override {
    return false;
  }

  void movement(ToolLoop* loop, Stroke& stroke, const Point& point, float pressure) override {
    ASSERT(stroke.size() >= 2);
    if (stroke.size() < 2)
      return;

    if (MoveOriginCapability::isMovingOrigin(loop, stroke, point))
      return;

    stroke[1] = {point.x, point.y, pressure};

    if (int(loop->getModifiers()) & int(ToolLoopModifiers::kSquareAspect)) {
      int dx = stroke[1].x - m_first.x;
      int dy = stroke[1].y - m_first.y;
      int minsize = MIN(ABS(dx), ABS(dy));
      int maxsize = MAX(ABS(dx), ABS(dy));

      // Lines
      if (loop->getIntertwine()->snapByAngle()) {
        double angle = 180.0 * std::atan(static_cast<double>(-dy) /
                                         static_cast<double>(dx)) / PI;
        angle = ABS(angle);

        // Snap horizontally
        if (angle < 18.0) {
          stroke[1].y = m_first.y;
        }
        // Snap at 26.565
        else if (angle < 36.0) {
          stroke[1].x = m_first.x + SGN(dx)*maxsize;
          stroke[1].y = m_first.y + SGN(dy)*maxsize/2;
        }
        // Snap at 45
        else if (angle < 54.0) {
          stroke[1].x = m_first.x + SGN(dx)*minsize;
          stroke[1].y = m_first.y + SGN(dy)*minsize;
        }
        // Snap at 63.435
        else if (angle < 72.0) {
          stroke[1].x = m_first.x + SGN(dx)*maxsize/2;
          stroke[1].y = m_first.y + SGN(dy)*maxsize;
        }
        // Snap vertically
        else {
          stroke[1].x = m_first.x;
        }
      }
      // Rectangles and ellipses
      else {
        stroke[1].x = m_first.x + SGN(dx)*minsize;
        stroke[1].y = m_first.y + SGN(dy)*minsize;
      }
    }

    stroke[0] = m_first;

    if (int(loop->getModifiers()) & int(ToolLoopModifiers::kFromCenter)) {
      int rx = stroke[1].x - m_first.x;
      int ry = stroke[1].y - m_first.y;
      stroke[0].x = m_first.x - rx;
      stroke[0].y = m_first.y - ry;
      stroke[1].x = m_first.x + rx;
      stroke[1].y = m_first.y + ry;
    }

    // Adjust points for selection like tools (so we can select tiles)
    if (loop->getController()->canSnapToGrid() &&
        loop->getSnapToGrid() &&
        loop->getInk()->isSelection()) {
      if (stroke[0].x < stroke[1].x)
        stroke[1].x--;
      else if (stroke[0].x > stroke[1].x)
        stroke[0].x--;

      if (stroke[0].y < stroke[1].y)
        stroke[1].y--;
      else if (stroke[0].y > stroke[1].y)
        stroke[0].y--;
    }
  }

  void getStrokeToInterwine(const Stroke& input, Stroke& output) override {
    ASSERT(input.size() >= 2);
    if (input.size() < 2)
      return;

    output.addPoint(input[0]);
    output.addPoint(input[1]);
  }

  void getStatusBarText(const Stroke& stroke, std::string& text) override {
    ASSERT(stroke.size() >= 2);
    if (stroke.size() < 2)
      return;

    int w = ABS(stroke[1].x-stroke[0].x)+1;
    int h = ABS(stroke[1].y-stroke[0].y)+1;

    char buf[1024];
    snprintf(buf, sizeof(buf), ":start: %3d %3d :end: %3d %3d :size: %3d %3d :distance: %.1f :angle: %.1f",
            stroke[0].x, stroke[0].y,
            stroke[1].x, stroke[1].y,
            w, h,
            std::sqrt(w*w + h*h),
            180.0 * std::atan2(static_cast<double>(stroke[0].y-stroke[1].y),
                               static_cast<double>(stroke[1].x-stroke[0].x)) / PI);
    text = buf;
  }

private:
  void onMoveOrigin(const Point& delta) override {
    m_first += delta;
  }

  Stroke::Point m_first;
};

// Controls clicks for tools like polygon
class PointByPointController : public MoveOriginCapability {
public:

  void pressButton(Stroke& stroke, const Point& point, float pressure) override {
    MoveOriginCapability::pressButton(stroke, point, pressure);

    stroke.addPoint({point.x, point.y, pressure});
    stroke.addPoint({point.x, point.y, pressure});
  }

  bool releaseButton(Stroke& stroke, const Point& point) override {
    ASSERT(!stroke.empty());
    if (stroke.empty())
      return false;

    if (stroke[stroke.size()-2] == point &&
        stroke[stroke.size()-1] == point)
      return false;             // Click in the same point (no-drag), we are done
    else
      return true;              // Continue adding points
  }

  void movement(ToolLoop* loop, Stroke& stroke, const Point& point, float pressure) override {
    ASSERT(!stroke.empty());
    if (stroke.empty())
      return;

    if (MoveOriginCapability::isMovingOrigin(loop, stroke, point))
      return;

    stroke[stroke.size()-1] = {point.x, point.y, pressure};
  }

  void getStrokeToInterwine(const Stroke& input, Stroke& output) override {
    output = input;
  }

  void getStatusBarText(const Stroke& stroke, std::string& text) override {
    ASSERT(!stroke.empty());
    if (stroke.empty())
      return;

    char buf[1024];
    snprintf(buf, sizeof(buf), ":start: %3d %3d :end: %3d %3d",
            stroke.firstPoint().x,
            stroke.firstPoint().y,
            stroke.lastPoint().x,
            stroke.lastPoint().y);
    text = buf;
  }

};

class OnePointController : public Controller {
public:
  // Do not apply grid to "one point tools" (e.g. magic wand, flood fill, etc.)
  bool canSnapToGrid() override { return false; }
  bool isOnePoint() override { return true; }

  void pressButton(Stroke& stroke, const Point& point, float pressure) override {
    if (stroke.size() == 0)
      stroke.addPoint({point.x, point.y, pressure});
  }

  bool releaseButton(Stroke& stroke, const Point& point) override {
    return false;
  }

  void movement(ToolLoop* loop, Stroke& stroke, const Point& point, float pressure) override {
    // Do nothing
  }

  void getStrokeToInterwine(const Stroke& input, Stroke& output) override {
    output = input;
  }

  void getStatusBarText(const Stroke& stroke, std::string& text) override {
    ASSERT(!stroke.empty());
    if (stroke.empty())
      return;

    char buf[1024];
    snprintf(buf, sizeof(buf), ":pos: %3d %3d", stroke[0].x, stroke[0].y);
    text = buf;
  }

};

class FourPointsController : public MoveOriginCapability {
public:

  void pressButton(Stroke& stroke, const Point& point, float pressure) override {
    MoveOriginCapability::pressButton(stroke, point, pressure);

    if (stroke.size() == 0) {
      stroke.reset(4, {point.x, point.y, pressure});
      m_clickCounter = 0;
    }
    else
      m_clickCounter++;
  }

  bool releaseButton(Stroke& stroke, const Point& point) override {
    m_clickCounter++;
    return m_clickCounter < 4;
  }

  void movement(ToolLoop* loop, Stroke& stroke, const Point& point, float pressure) override {
    if (MoveOriginCapability::isMovingOrigin(loop, stroke, point))
      return;

    switch (m_clickCounter) {
      case 0:
        for (int i=1; i<stroke.size(); ++i)
          stroke[i] = {point.x, point.y, pressure};
        break;
      case 1:
      case 2:
        stroke[1] = {point.x, point.y, pressure};
        stroke[2] = {point.x, point.y, pressure};
        break;
      case 3:
        stroke[2] = {point.x, point.y, pressure};
        break;
    }
  }

  void getStrokeToInterwine(const Stroke& input, Stroke& output) override {
    output = input;
  }

  void getStatusBarText(const Stroke& stroke, std::string& text) override {
    ASSERT(stroke.size() >= 4);
    if (stroke.size() < 4)
      return;

    char buf[1024];
    snprintf(buf, sizeof(buf), ":start: %3d %3d :end: %3d %3d (%3d %3d - %3d %3d)",
            stroke[0].x, stroke[0].y,
            stroke[3].x, stroke[3].y,
            stroke[1].x, stroke[1].y,
            stroke[2].x, stroke[2].y);

    text = buf;
  }

private:
  int m_clickCounter;
};

} // namespace tools
} // namespace app
