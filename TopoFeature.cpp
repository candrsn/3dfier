
#include "TopoFeature.h"
#include "io.h"

TopoFeature::TopoFeature(Polygon2* p, std::string pid) {
  _id = pid;
  _p2 = p;
  _velevations.resize(bg::num_points(*(_p2)));
}

TopoFeature::~TopoFeature() {
  // TODO: clear memory properly
  std::cout << "I am dead" << std::endl;
}

Box TopoFeature::get_bbox2d() {
  return bg::return_envelope<Box>(*_p2);
}

std::string TopoFeature::get_id() {
  return _id;
}

Polygon2* TopoFeature::get_Polygon2() {
    return _p2;
}


//-------------------------------
//-------------------------------


Block::Block(Polygon2* p, std::string pid, std::string heightref) : TopoFeature(p, pid) 
{
  _heightref = heightref;
  _is3d = false;
  _vertexelevations.assign(bg::num_points(*(_p2)), 9999);
}

int Block::get_number_vertices() {
  return int(2 * _vertices.size());
}

bool Block::threeDfy() {
  build_CDT();
  _floorheight = this->get_floor_height();
  _roofheight = this->get_roof_height();
  _is3d = true;
  _zvaluesinside.clear();
  _zvaluesinside.shrink_to_fit();
  _vertexelevations.clear();
  _vertexelevations.shrink_to_fit();
  return true;
}

std::string Block::get_citygml() {
  //-- TODO: CityGML implementation for TIN type
  return "EMTPY";
}


std::string Block::get_obj_v() {
  std::stringstream ss;
  for (auto& v : _vertices)
    ss << std::setprecision(2) << std::fixed << "v " << bg::get<0>(v) << " " << bg::get<1>(v) << " " << this->get_roof_height() << std::endl;
  for (auto& v : _vertices)
    ss << std::setprecision(2) << std::fixed << "v " << bg::get<0>(v) << " " << bg::get<1>(v) << " " << this->get_floor_height() << std::endl;
  return ss.str();
}

std::string Block::get_obj_f(int offset, bool floor) {
  std::stringstream ss;
  ss << "usemtl block" << std::endl;
  //-- roof
  for (auto& t : _triangles)
    ss << "f " << (t.v0 + 1 + offset) << " " << (t.v1 + 1 + offset) << " " << (t.v2 + 1 + offset) << std::endl;
  //-- side walls
  for (auto& s : _segments) {
    ss << "f " << (s.v1 + 1 + offset) << " " << (s.v0 + 1 + offset) << " " << (s.v0 + 1 + offset + _vertices.size()) << std::endl;  
    ss << "f " << (s.v0 + 1 + offset + _vertices.size()) << " " << (s.v1 + 1 + offset + _vertices.size()) << " " << (s.v1 + 1 + offset) << std::endl;  
  }
  //-- floor
  if (floor == true) {
    for (auto& t : _triangles)
      ss << "f " << (t.v0 + 1 + offset + _vertices.size()) << " " << (t.v1 + 1 + offset + _vertices.size()) << " " << (t.v2 + 1 + offset + _vertices.size()) << std::endl;  
  }
  return ss.str();
}


float Block::get_roof_height() {
  if (_is3d == true)
    return _roofheight;
  if (_zvaluesinside.size() == 0)
    return -9999;
  if (_heightref == "max") {
    double v = -9999;
    for (auto z : _zvaluesinside) {
      if (z > v)
        v = z;
    }
    return v;
  }
  else if (_heightref == "min") {
    double v = 9999;
    for (auto z : _zvaluesinside) {
      if (z < v)
        v = z;
    }
    return v;
  }
  else if (_heightref == "avg") {
    double sum = 0.0;
    for (auto z : _zvaluesinside) 
      sum += z;
    return (sum / _zvaluesinside.size());
  }
  else if (_heightref == "median") {
    std::nth_element(_zvaluesinside.begin(), _zvaluesinside.begin() + (_zvaluesinside.size() / 2), _zvaluesinside.end());
    return _zvaluesinside[_zvaluesinside.size() / 2];
  }
  else {
    if (_heightref.substr(0, _heightref.find_first_of("-")) == "percentile") {
      double p = (std::stod(_heightref.substr(_heightref.find_first_of("-") + 1)) / 100);
      std::nth_element(_zvaluesinside.begin(), _zvaluesinside.begin() + (_zvaluesinside.size() / p), _zvaluesinside.end());
      return _zvaluesinside[_zvaluesinside.size() / p];
    }
    else
      std::cerr << "ERROR: height reference '" << _heightref << "' unknown." << std::endl;
  }
  return -9999;
}


float Block::get_floor_height() {
  if (_is3d == true)
    return _floorheight;
  return *(std::min_element(std::begin(_vertexelevations), std::end(_vertexelevations)));
}

bool Block::add_elevation_point(double x, double y, double z) {
  Point2 p(x, y);
  //-- 1. assign to polygon if inside
  if (bg::within(p, *(_p2)) == true)
    _zvaluesinside.push_back(z);
  //-- 2. add to the vertices of the pgn to find their heights
  assign_elevation_to_vertex(x, y, z);
  return true;
}


bool Block::assign_elevation_to_vertex(double x, double y, double z) {
  //-- TODO: okay here brute-force, use of flann is points>200 (to benchmark perhaps?)  
  Point2 p(x, y);
  Ring2 oring = bg::exterior_ring(*(_p2));
  for (int i = 0; i < oring.size(); i++) {
    if (bg::distance(p, oring[i]) <= 2.0)
      _velevations[i] = z;
  }
  int offset = int(bg::num_points(oring));
  auto irings = bg::interior_rings(*(_p2));
  for (Ring2& iring: irings) {
    for (int i = 0; i < iring.size(); i++) {
      if (bg::distance(p, iring) <= 2.0)
        _velevations[i + offset] = z;
    }
    offset += bg::num_points(iring);
  }
  return true;
}
// //-- TODO: okay here brute-force, use of flann is points>200 (to benchmark perhaps?)  
//   Point2 p(x, y);
//   Ring2 oring = bg::exterior_ring(*(_p2));
//   for (int i = 0; i < oring.size(); i++) {
//     if (bg::distance(p, oring[i]) <= 2.0)
//       if (z < _vertexelevations[i])
//         _vertexelevations[i] = z;
//   }
//   int offset = int(bg::num_points(oring));
//   auto irings = bg::interior_rings(*(_p2));
//   for (Ring2& iring: irings) {
//     for (int i = 0; i < iring.size(); i++) {
//       if (bg::distance(p, iring) <= 2.0)
//         if (z < _vertexelevations[i + offset])
//         _vertexelevations[i + offset] = z;
//     }
//     offset += bg::num_points(iring);
//   }
//   return true;
// }


bool Block::build_CDT() {
  std::stringstream ss;
  ss << bg::wkt(*(_p2));
  Polygon3 p3;
  bg::read_wkt(ss.str(), p3);
  getCDT(&p3, _vertices, _triangles, _segments);
  return true;
}


//-------------------------------
//-------------------------------

Boundary3D::Boundary3D(Polygon2* p, std::string pid) : TopoFeature(p, pid) 
{
}


bool Boundary3D::threeDfy() {
  std::stringstream ss;
  ss << bg::wkt(*(_p2));
  Polygon3 p3;
  bg::read_wkt(ss.str(), p3);
  getCDT(&p3, _vertices, _triangles, _segments);
  return true;
}

int Boundary3D::get_number_vertices() {
  return int(_vertices.size());
}


std::string Boundary3D::get_citygml() {
  return "EMPTY"; 
}


std::string Boundary3D::get_obj_v() {
  std::stringstream ss;
  for (auto& v : _vertices)
    ss << std::setprecision(2) << std::fixed << "v " << bg::get<0>(v) << " " << bg::get<1>(v) << " " << bg::get<2>(v) << std::endl;
  return ss.str();
}

std::string Boundary3D::get_obj_f(int offset, bool floor) {
  std::stringstream ss;
  ss << "usemtl boundary3d" << std::endl;
  for (auto& t : _triangles)
    ss << "f " << (t.v0 + 1 + offset) << " " << (t.v1 + 1 + offset) << " " << (t.v2 + 1 + offset) << std::endl;
  return ss.str();
}

bool Boundary3D::add_elevation_point(double x, double y, double z) {
  _lidarpts.push_back(Point3(x, y, z));
  return true;
}


//-------------------------------
//-------------------------------

TIN::TIN(Polygon2* p, std::string pid, int simplification) : TopoFeature(p, pid) 
{
  _simplification = simplification;
  _vertexelevations.resize(bg::num_points(*(_p2)));
}


bool TIN::threeDfy() {
  std::stringstream ss;
  ss << bg::wkt(*(_p2));
  Polygon3 p3;
  bg::read_wkt(ss.str(), p3);
  add_elevations_to_boundary(p3);
  getCDT(&p3, _vertices, _triangles, _segments, _lidarpts);
  return true;
}


void TIN::add_elevations_to_boundary(Polygon3 &p3) {
  float avg = 0.0;
  int count = 0;
  for (int i = 0; i < _vertexelevations.size(); i++) {
    if (std::get<0>(_vertexelevations[i]) != 0) {
      float z = std::get<1>(_vertexelevations[i]) / (float(std::get<0>(_vertexelevations[i])));
      avg += z;
      count++;  
    }
  }
  avg = avg / count;
  Ring3 oring = bg::exterior_ring(p3);
  for (int i = 0; i < oring.size(); i++) {
    if (std::get<0>(_vertexelevations[i]) != 0) {
      float z = std::get<1>(_vertexelevations[i]) / (float(std::get<0>(_vertexelevations[i])));
      bg::set<2>(bg::exterior_ring(p3)[i], z);
    }
    else {
      bg::set<2>(bg::exterior_ring(p3)[i], avg);
    }
  }
  auto irings = bg::interior_rings(p3);
  std::size_t offset = bg::num_points(p3);
  for (Ring3& iring: irings) {
    for (int i = 0; i < iring.size(); i++) {
      if (std::get<0>(_vertexelevations[i]) != 0) {
        float z = std::get<1>(_vertexelevations[i]) / (float(std::get<0>(_vertexelevations[i])));
        bg::set<2>(bg::exterior_ring(p3)[i], z);
      }
      else {
        bg::set<2>(bg::exterior_ring(p3)[i], avg);
      }
    }
    offset += bg::num_points(iring);
  }
}

int TIN::get_number_vertices() {
  return int(_vertices.size());
}


std::string TIN::get_citygml() {
  //-- TODO: CityGML implementation for TIN type
  return "EMTPY";
}

std::string TIN::get_obj_v() {
  std::stringstream ss;
  for (auto& v : _vertices)
    ss << std::setprecision(2) << std::fixed << "v " << bg::get<0>(v) << " " << bg::get<1>(v) << " " << bg::get<2>(v) << std::endl;
  return ss.str();
}

std::string TIN::get_obj_f(int offset, bool floor) {
  std::stringstream ss;
  ss << "usemtl tin" << std::endl;
  for (auto& t : _triangles)
    ss << "f " << (t.v0 + 1 + offset) << " " << (t.v1 + 1 + offset) << " " << (t.v2 + 1 + offset) << std::endl;
  return ss.str();
}


bool TIN::add_elevation_point(double x, double y, double z) {
  Point2 p(x, y);
  //-- 1. add points to surface if inside
  if (bg::within(p, *(_p2)) == true) {
    if (_simplification == 0)
      _lidarpts.push_back(Point3(x, y, z));
    else {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<int> dis(1, _simplification);
      if (dis(gen) == 1)
        _lidarpts.push_back(Point3(x, y, z));
    }
  }
  //-- 2. add to the vertices of the pgn to find their heights
  assign_elevation_to_vertex(x, y, z);
  return true;
}


//-- TODO: brute-force == not good
bool TIN::assign_elevation_to_vertex(double x, double y, double z) {
  //-- assign the AVERAGE to each
  Point2 p(x, y);
  Ring2 oring = bg::exterior_ring(*(_p2));
  for (int i = 0; i < oring.size(); i++) {
    if (bg::distance(p, oring[i]) <= 2.0) {
      std::get<0>(_vertexelevations[i]) += 1;
      std::get<1>(_vertexelevations[i]) += z;  
    }
  }
  int offset = int(bg::num_points(oring));
  auto irings = bg::interior_rings(*(_p2));
  for (Ring2& iring: irings) {
    for (int i = 0; i < iring.size(); i++) {
      if (bg::distance(p, iring) <= 2.0) {
        std::get<0>(_vertexelevations[i + offset]) += 1;
        std::get<1>(_vertexelevations[i + offset]) += z;
      }
    }
    offset += bg::num_points(iring);
  }
  return true;
}

