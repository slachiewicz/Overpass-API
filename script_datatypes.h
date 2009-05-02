#ifndef SCRIPT_DATATYPES
#define SCRIPT_DATATYPES

// temp
#include <iostream>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <math.h>

using namespace std;

typedef char int8;
typedef short int int16;
typedef int int32;
typedef long long int64;

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

//-----------------------------------------------------------------------------

struct Node
{
  public:
    Node(int id_, int lat_, int lon_) : id(id_), lat(lat_), lon(lon_) {}
    
    const int id;
    int lat, lon;
};

inline bool operator<(const Node& node_1, const Node& node_2)
{
  return (node_1.id < node_2.id);
}

struct Way
{
  public:
    Way(int id_) : id(id_) {}
    
    const uint32 id;
    vector< uint32 > members;
};

inline bool operator<(const Way& way_1, const Way& way_2)
{
  return (way_1.id < way_2.id);
}

struct Way_
{
public:
  typedef uint32 Index;
  typedef uint32 Id;
  typedef uint32 Data;
  
  Way_() : head(make_pair(0, 0)) {}
  Way_(Index index_, Id id_) : head(make_pair(index_, id_)) {}
  Way_(pair< Index, Id >& head_) : head(head_) {}
  
  pair< Index, Id > head;
  vector< Data > data;
};

inline bool operator<(const Way_& way_1, const Way_& way_2)
{
  return (way_1.head < way_2.head);
}

struct Relation
{
  public:
    Relation(int id_) : id(id_) {}
    
    const int id;
    set< pair< int, int > > node_members;
    set< pair< int, int > > way_members;
    set< pair< int, int > > relation_members;
};

inline bool operator<(const Relation& relation_1, const Relation& relation_2)
{
  return (relation_1.id < relation_2.id);
}

struct Relation_Member
{
  Relation_Member() : id(0), type(0), role(0) {}
  Relation_Member(uint32 id_, uint type_, uint32 role_) : id(id_), type(type_), role(role_) {}
  
  uint32 id;
  uint type;
  uint32 role;
  
  static const uint NODE = 1;
  static const uint WAY = 2;
  static const uint RELATION = 3;
};

struct Relation_
{
  public:
    typedef uint32 Id;
    typedef Id Index;
    typedef Relation_Member Data;
    
    Relation_() : head(0) {}
    Relation_(Id id_) : head(id_) {}
  
    Id head;
    vector< Data > data;
};

inline bool operator<(const Relation_& relation_1, const Relation_& relation_2)
{
  return (relation_1.head < relation_2.head);
}

struct Line_Segment
{
  public:
    Line_Segment(int west_lat_, int west_lon_, int east_lat_, int east_lon_)
  : west_lon(west_lon_), west_lat(west_lat_), east_lon(east_lon_),  east_lat(east_lat_) {}
    
    const int west_lon, west_lat;
    const int east_lon, east_lat;
};

inline bool operator<(const Line_Segment& line_segment_1, const Line_Segment& line_segment_2)
{
  if (line_segment_1.west_lon < line_segment_2.west_lon)
    return true;
  if (line_segment_1.west_lon > line_segment_2.west_lon)
    return false;
  if (line_segment_1.west_lat < line_segment_2.west_lat)
    return true;
  if (line_segment_1.west_lat > line_segment_2.west_lat)
    return false;
  if (line_segment_1.east_lon < line_segment_2.east_lon)
    return true;
  if (line_segment_1.east_lon > line_segment_2.east_lon)
    return false;
  return (line_segment_1.east_lat < line_segment_2.east_lat);
}

struct Area
{
  public:
    Area(uint32 id_) : id(id_) {}
    
    const uint32 id;
    //set< Line_Segment > segments;
};

const int REF_NODE = 1;
const int REF_WAY = 2;
const int REF_RELATION = 3;

inline bool operator<(const Area& area_1, const Area& area_2)
{
  return (area_1.id < area_2.id);
}

class Set
{
  public:
    Set() {}
    Set(const set< Node >& nodes_,
	const set< Way >& ways_,
        const set< Relation_ >& relations_)
  : nodes(nodes_), ways(ways_), relations(relations_) {}
    Set(const set< Node >& nodes_,
	const set< Way >& ways_,
	const set< Relation_ >& relations_,
	const set< Area >& areas_)
  : nodes(nodes_), ways(ways_), relations(relations_), areas(areas_) {}
    
    const set< Node >& get_nodes() const { return nodes; }
    const set< Way >& get_ways() const { return ways; }
    const set< Relation_ >& get_relations() const { return relations; }
    const set< Area >& get_areas() const { return areas; }
    
    set< Node >& get_nodes_handle() { return nodes; }
    set< Way >& get_ways_handle() { return ways; }
    set< Relation_ >& get_relations_handle() { return relations; }
    set< Area >& get_areas_handle() { return areas; }
  
  private:
    set< Node > nodes;
    set< Way > ways;
    set< Relation_ > relations;
    set< Area > areas;
};

//-----------------------------------------------------------------------------

inline int calc_idx(int a)
{
  return ((int)floor(((double)a)/10000000));
}

inline int ll_idx(int lat, int lon)
{
  int result(0);
  lat += 90*10000000;
  result |= ~(lon>>1)&0x40000000;
  result |= (lat>>1)&0x20000000;
  result |= (lon>>2)&0x10000000;
  result |= (lat>>2)&0x08000000;
  result |= (lon>>3)&0x04000000;
  result |= (lat>>3)&0x02000000;
  result |= (lon>>4)&0x01000000;
  result |= (lat>>4)&0x00800000;
  result |= (lon>>5)&0x00400000;
  result |= (lat>>5)&0x00200000;
  result |= (lon>>6)&0x00100000;
  result |= (lat>>6)&0x00080000;
  result |= (lon>>7)&0x00040000;
  result |= (lat>>7)&0x00020000;
  result |= (lon>>8)&0x00010000;
  result |= (lat>>8)&0x00008000;
  result |= (lon>>9)&0x00004000;
  result |= (lat>>9)&0x00002000;
  result |= (lon>>10)&0x00001000;
  result |= (lat>>10)&0x00000800;
  result |= (lon>>11)&0x00000400;
  result |= (lat>>11)&0x00000200;
  result |= (lon>>12)&0x00000100;
  result |= (lat>>12)&0x00000080;
  result |= (lon>>13)&0x00000040;
  result |= (lat>>13)&0x00000020;
  result |= (lon>>14)&0x00000010;
  result |= (lat>>14)&0x00000008;
  result |= (lon>>15)&0x00000004;
  result |= (lat>>15)&0x00000002;
  result |= (lon>>16)&0x00000001;
  return result;
}

inline int32 lat_of_ll(uint32 ll_idx_)
{
  int32 lat(0);
  lat |= (ll_idx_<<1)&0x40000000;
  lat |= (ll_idx_<<2)&0x20000000;
  lat |= (ll_idx_<<3)&0x10000000;
  lat |= (ll_idx_<<4)&0x08000000;
  lat |= (ll_idx_<<5)&0x04000000;
  lat |= (ll_idx_<<6)&0x02000000;
  lat |= (ll_idx_<<7)&0x01000000;
  lat |= (ll_idx_<<8)&0x00800000;
  lat |= (ll_idx_<<9)&0x00400000;
  lat |= (ll_idx_<<10)&0x00200000;
  lat |= (ll_idx_<<11)&0x00100000;
  lat |= (ll_idx_<<12)&0x00080000;
  lat |= (ll_idx_<<13)&0x00040000;
  lat |= (ll_idx_<<14)&0x00020000;
  lat |= (ll_idx_<<15)&0x00010000;
  return (lat -= 90*10*1000*1000);
}

inline int32 lon_of_ll(uint32 ll_idx_)
{
  int32 lon(0);
  lon |= ~(ll_idx_<<1)&0x80000000;
  lon |= (ll_idx_<<2)&0x40000000;
  lon |= (ll_idx_<<3)&0x20000000;
  lon |= (ll_idx_<<4)&0x10000000;
  lon |= (ll_idx_<<5)&0x08000000;
  lon |= (ll_idx_<<6)&0x04000000;
  lon |= (ll_idx_<<7)&0x02000000;
  lon |= (ll_idx_<<8)&0x01000000;
  lon |= (ll_idx_<<9)&0x00800000;
  lon |= (ll_idx_<<10)&0x00400000;
  lon |= (ll_idx_<<11)&0x00200000;
  lon |= (ll_idx_<<12)&0x00100000;
  lon |= (ll_idx_<<13)&0x00080000;
  lon |= (ll_idx_<<14)&0x00040000;
  lon |= (ll_idx_<<15)&0x00020000;
  lon |= (ll_idx_<<16)&0x00010000;
  return lon;
}

inline int in_lat_lon(const char* input)
{
  double val(atof(input));
  if (val < 0)
    return (int)(val*10000000 - 0.5);
  return (int)(val*10000000 + 0.5);
}

inline int in_lat_lon(const string& input)
{
  double val(atof(input.c_str()));
  if (val < 0)
    return (int)(val*10000000 - 0.5);
  return (int)(val*10000000 + 0.5);
}

// faster but less portable
//
// inline int in_lat_lon(const string& input)
// {
//   unsigned int i(0), j(10000000), size(input.size());
//   bool minus(false);
//   int res(0);
//   if (input[0] == '-')
//   {
//     minus = true;
//     ++i;
//   }
//   while ((i < size) && (input[i] >= '0') && (input[i] <= '9'))
//   {
//     res = 10*res + (input[i] - '0');
//     ++i;
//   }
//   if (input[i] == '.')
//     ++i;
//   while ((i < size) && (input[i] >= '0') && (input[i] <= '9'))
//   {
//     res = 10*res + (input[i] - '0');
//     ++i;
//     j = j/10;
//   }
//   res = res*j;
//   if ((i < size) || (j == 0))
//     return 200*10000000;
//   else
//     return (minus ? -res : res);
// }

#endif
