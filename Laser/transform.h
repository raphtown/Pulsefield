#pragma once

#include <iostream>
#include "opencv2/core/core.hpp"
#include "point.h"
#include "cpoint.h"
#include "etherdream_bst.h"

class Color;

// Transformation between floor (meter) space and device (-32767:32767) space
class Transform {
    cv::Mat transform, invTransform; 
    std::vector<Point> floorpts, devpts;
    float hfov, vfov;
    static std::vector<cv::Point2f> convertPoints(const std::vector<Point> &pts);

    // Convert  device coord to flat space (i.e. laser projection grid)
    Point deviceToFlat(Point devPt) const;
    std::vector<Point> deviceToFlat(const std::vector<Point> &devPts) const;
    // Convert  flat space coord to device coords
    Point flatToDevice(Point flatPt) const;
    std::vector<Point> flatToDevice(const std::vector<Point> &flatPts) const;
 public:
    Transform();
    void clear();

    // Mapping, if out-of-range, return clipped point
    etherdream_point mapToDevice(CPoint floorPt) const;
    Point mapToDevice(Point floorPt) const;

    // Inverse mapping from laser to world
    Point mapToWorld(Point p) const;
    CPoint mapToWorld(etherdream_point p) const;

    std::vector<etherdream_point> mapToDevice(const std::vector<CPoint> &floorPts) const;
    std::vector<Point> mapToDevice(const std::vector<Point> &floorPts) const;
    std::vector<CPoint> mapToWorld(const std::vector<etherdream_point> &pts) const;

    // Compute transform matrix from set of points already provided
    void recompute();

    // Setup mapping
    // Point indices are order based on laser positions: BL, BR, TR, TL
    Point getFloorPoint(int i) const { assert(i>=0&&i<(int)floorpts.size()); return floorpts[i]; }
    Point getDevPoint(int i) const { assert(i>=0&&i<(int)floorpts.size()); return devpts[i]; }
    void setFloorPoint(int i, Point floorpt) { assert(i>=0&&i<(int)floorpts.size()); floorpts[i]=floorpt; }
    void setDevPoint(int i, Point devpt) { assert(i>=0&&i<(int)floorpts.size()); devpts[i]=devpt; }

    // Load/save 
    void save(std::ostream &s) const;
    void load(std::istream &s);
};

