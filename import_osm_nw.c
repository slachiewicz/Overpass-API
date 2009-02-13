#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "script_datatypes.h"
#include "expat_justparse_interface.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <mysql.h>

using namespace std;

typedef short int int16;
typedef int int32;
typedef long long int64;

typedef unsigned short int uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

//-----------------------------------------------------------------------------

const int NODE_FILE_BLOCK_SIZE = sizeof(int) + BLOCKSIZE*sizeof(Node);
const int WAY_FILE_BLOCK_SIZE = sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32);

//-----------------------------------------------------------------------------

int nodes_dat_fd;
multimap< int, unsigned int > block_index_nd;
int32 max_node_id(0);
int* wr_buf;
int* rd_buf;

void prepare_nodes()
{
  wr_buf = (int*) malloc(NODE_FILE_BLOCK_SIZE);
  rd_buf = (int*) malloc(NODE_FILE_BLOCK_SIZE);
  if ((!wr_buf) || (!rd_buf))
  {
    cerr<<"malloc: "<<errno<<'\n';
    exit(0);
  }
  
  nodes_dat_fd = open64(NODE_DATA, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  close(nodes_dat_fd);
  nodes_dat_fd = open64(NODE_DATA, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if (nodes_dat_fd < 0)
  {
    cerr<<"open64: "<<errno<<'\n';
    exit(0);
  }
}

void flush_nodes(const multimap< int, Node >& nodes)
{
  static int next_block_id(0);
  
  if (block_index_nd.empty())
  {
    unsigned int i(0);
    Node* nodes_buf((Node*)(&wr_buf[1]));
    
    block_index_nd.insert(make_pair< int, unsigned int >(nodes.begin()->first, next_block_id++));
    
    multimap< int, Node >::const_iterator it(nodes.begin());
    while (it != nodes.end())
    {
      if (i == BLOCKSIZE)
      {
	wr_buf[0] = i;
	write(nodes_dat_fd, wr_buf, NODE_FILE_BLOCK_SIZE);
	
	block_index_nd.insert(make_pair< int, unsigned int >(it->first, next_block_id++));
	i = 0;
      }
      
      new (&nodes_buf[i]) Node(it->second);
      if (it->second.id > max_node_id)
	max_node_id = it->second.id;
      ++it;
      ++i;
    }
    if (i > 0)
    {
      wr_buf[0] = i;
      write(nodes_dat_fd, wr_buf, NODE_FILE_BLOCK_SIZE);
    }
  }
  else
  {
    multimap< int, Node >::const_iterator nodes_it(nodes.begin());
    multimap< int, unsigned int >::const_iterator block_it(block_index_nd.begin());
    unsigned int cur_block((block_it++)->second);
    while (nodes_it != nodes.end())
    {
      while ((block_it != block_index_nd.end()) && (block_it->first <= nodes_it->first))
	cur_block = (block_it++)->second;
      
      unsigned int new_element_count(0);
      multimap< int, Node >::const_iterator nodes_it2(nodes_it);
      if (block_it != block_index_nd.end())
      {
	while ((nodes_it2 != nodes.end()) && (block_it->first > nodes_it2->first))
	{
	  if (nodes_it2->second.id > max_node_id)
	    max_node_id = nodes_it2->second.id;
	  ++new_element_count;
	  ++nodes_it2;
	}
      }
      else
      {
	while (nodes_it2 != nodes.end())
	{
	  if (nodes_it2->second.id > max_node_id)
	    max_node_id = nodes_it2->second.id;
	  ++new_element_count;
	  ++nodes_it2;
	}
      }
      
      lseek64(nodes_dat_fd, (int64)cur_block*(NODE_FILE_BLOCK_SIZE), SEEK_SET);
      read(nodes_dat_fd, rd_buf, NODE_FILE_BLOCK_SIZE);
      new_element_count += rd_buf[0];
      
      int i(0);
      while (new_element_count > BLOCKSIZE)
      {
	unsigned int blocksize(new_element_count/(new_element_count/BLOCKSIZE + 1));
	
	unsigned int j(0);
	wr_buf[0] = blocksize;
	Node* nodes_rd_buf((Node*)(&rd_buf[1]));
	Node* nodes_wr_buf((Node*)(&wr_buf[1]));
	while ((j < blocksize) && (nodes_it != nodes_it2) && (i < rd_buf[0]))
	{
	  if (nodes_it->first < ll_idx(nodes_rd_buf[i].lat, nodes_rd_buf[i].lon))
	    new (&nodes_wr_buf[j++]) Node((nodes_it++)->second);
	  else
	    new (&nodes_wr_buf[j++]) Node(nodes_rd_buf[i++]);
	}
	while ((j < blocksize) && (nodes_it != nodes_it2))
	  new (&nodes_wr_buf[j++]) Node((nodes_it++)->second);
	while ((j < blocksize) && (i < rd_buf[0]))
	  new (&nodes_wr_buf[j++]) Node(nodes_rd_buf[i++]);
	
	lseek64(nodes_dat_fd, (int64)cur_block*(NODE_FILE_BLOCK_SIZE), SEEK_SET);
	write(nodes_dat_fd, wr_buf, NODE_FILE_BLOCK_SIZE);
	
	cur_block = next_block_id;
	if ((i >= rd_buf[0]) || (nodes_it->first < ll_idx(nodes_rd_buf[i].lat, nodes_rd_buf[i].lon)))
	  block_index_nd.insert(make_pair< int, unsigned int >(nodes_it->first, next_block_id++));
	else
	  block_index_nd.insert(make_pair< int, unsigned int >
	      (ll_idx(nodes_rd_buf[i].lat, nodes_rd_buf[i].lon), next_block_id++));
	new_element_count -= blocksize;	
      }
      
      unsigned int j(0);
      wr_buf[0] = new_element_count;
      Node* nodes_rd_buf((Node*)(&rd_buf[1]));
      Node* nodes_wr_buf((Node*)(&wr_buf[1]));
      while ((nodes_it != nodes_it2) && (i < rd_buf[0]))
      {
	if (nodes_it->first < ll_idx(nodes_rd_buf[i].lat, nodes_rd_buf[i].lon))
	  new (&nodes_wr_buf[j++]) Node((nodes_it++)->second);
	else
	  new (&nodes_wr_buf[j++]) Node(nodes_rd_buf[i++]);
      }
      while (nodes_it != nodes_it2)
	new (&nodes_wr_buf[j++]) Node((nodes_it++)->second);
      while (i < rd_buf[0])
	new (&nodes_wr_buf[j++]) Node(nodes_rd_buf[i++]);
	
      lseek64(nodes_dat_fd, (int64)cur_block*(NODE_FILE_BLOCK_SIZE), SEEK_SET);
      write(nodes_dat_fd, wr_buf, NODE_FILE_BLOCK_SIZE);
      
      nodes_it = nodes_it2;
    }
  }
}

void nodes_make_block_index()
{
  pair< int, unsigned int >* buf = (pair< int, unsigned int >*)
      malloc(sizeof(int)*2*block_index_nd.size());
  if (!buf)
  {
    cerr<<"malloc: "<<errno<<'\n';
    exit(0);
  }
  
  unsigned int i(0);
  for (multimap< int, unsigned int >::const_iterator it(block_index_nd.begin());
       it != block_index_nd.end(); ++it)
    buf[i++] = *it;
  
  int nodes_idx_fd = open64(NODE_IDX, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  write(nodes_idx_fd, buf, sizeof(int)*2*block_index_nd.size());
  close(nodes_idx_fd);
  
  free(buf);
}

void nodes_make_id_index(int32* rd_buf)
{
  uint16* idx_buf = (uint16*) malloc(sizeof(uint16)*max_node_id);
  if (!idx_buf)
  {
    cerr<<"malloc: "<<errno<<'\n';
    exit(0);
  }
  
  lseek64(nodes_dat_fd, 0, SEEK_SET);
  for (unsigned int i(0); i < block_index_nd.size(); ++i)
  {
    read(nodes_dat_fd, rd_buf, NODE_FILE_BLOCK_SIZE);
    
    Node* nodes_rd_buf((Node*)(&rd_buf[1]));
    for (int j(0); j < rd_buf[0]; ++j)
      idx_buf[nodes_rd_buf[j].id-1] = i;
  }
  int nodes_idx_fd = open64(NODE_IDXA, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  write(nodes_idx_fd, idx_buf, sizeof(uint16)*max_node_id);
  close(nodes_idx_fd);
  
  free(idx_buf);
}

void postprocess_nodes()
{
  free(wr_buf);
  wr_buf = 0;
  
  nodes_make_block_index();
  nodes_make_id_index(rd_buf);
  
  free(rd_buf);
  rd_buf = 0;
  
  close(nodes_dat_fd);
}

//-----------------------------------------------------------------------------

const unsigned int WAY_FLUSH_INTERVAL = 32*1024*1024;
const unsigned int WAY_DOT_INTERVAL = 512*1024;

uint32 max_way_id(0);

struct C_Way
{
public:
  uint32 id;
  uint32 size;
  uint32* nodes;
};

multimap< int, C_Way > ways_by_first_idx;
uint32* way_rd_buf;
uint32* way_wr_buf;
multimap< int32, uint16 > block_index_way;

int ways_dat_fd;

int prepare_ways(uint32*& buf)
{
  int fd = open64(WAY_ALLTMP, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if (fd < 0)
  {
    cerr<<"open64: "<<errno<<'\n';
    exit(0);
  }
  
  ways_dat_fd = open64(WAY_DATA, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  close(ways_dat_fd);
  
  ways_dat_fd = open64(WAY_DATA, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if (ways_dat_fd < 0)
  {
    cerr<<"open64: "<<errno<<'\n';
    exit(0);
  }
  
  buf = (uint32*) malloc(sizeof(int32)*(MAXWAYNODES+2));
  return fd;
}

void flush_ways(const multimap< int, C_Way >& ways)
{
  static int next_block_id(0);
  
  if (block_index_way.empty())
  {
    unsigned int i(1);
    
    block_index_way.insert(make_pair< int, unsigned int >(ways.begin()->first, next_block_id++));
    
    multimap< int, C_Way >::const_iterator it(ways.begin());
    while (it != ways.end())
    {
      if (i >= WAY_BLOCKSIZE - it->second.size - 3)
      {
        way_wr_buf[0] = i-1;
        write(ways_dat_fd, way_wr_buf, WAY_FILE_BLOCK_SIZE);
        
        block_index_way.insert(make_pair< int, unsigned int >(it->first, next_block_id++));
        i = 1;
      }
      
      way_wr_buf[i++] = it->second.id;
      way_wr_buf[i++] = it->first;
      way_wr_buf[i++] = it->second.size;
      memcpy(&(way_wr_buf[i]), it->second.nodes, (it->second.size)*sizeof(uint32));
      i += it->second.size;
      ++it;
    }
    if (i > 1)
    {
      way_wr_buf[0] = i-1;
      write(ways_dat_fd, way_wr_buf, WAY_FILE_BLOCK_SIZE);
    }
  }
  else
  {
    multimap< int, C_Way >::const_iterator ways_it(ways.begin());
    multimap< int32, uint16 >::const_iterator block_it(block_index_way.begin());
    unsigned int cur_block((block_it++)->second);
    while (ways_it != ways.end())
    {
      while ((block_it != block_index_way.end()) && (block_it->first <= ways_it->first))
        cur_block = (block_it++)->second;

      unsigned int new_element_count(0);
      multimap< int, C_Way >::const_iterator ways_it2(ways_it);
      if (block_it != block_index_way.end())
      {
        while ((ways_it2 != ways.end()) && (block_it->first > ways_it2->first))
        {
	  new_element_count += ways_it2->second.size + 3;
          ++ways_it2;
        }
      }
      else
      {
        while (ways_it2 != ways.end())
        {
	  new_element_count += ways_it2->second.size + 3;
          ++ways_it2;
        }
      }
      
      lseek64(ways_dat_fd, (int64)cur_block*(WAY_FILE_BLOCK_SIZE), SEEK_SET);
      read(ways_dat_fd, way_rd_buf, WAY_FILE_BLOCK_SIZE);
      new_element_count += way_rd_buf[0];
      
      uint32 i(1);
      while (new_element_count > WAY_BLOCKSIZE)
      {
        uint32 blocksize(new_element_count/(new_element_count/WAY_BLOCKSIZE + 1));
        
        uint32 j(1);
        while ((j < blocksize) && (ways_it != ways_it2) && (i < way_rd_buf[0]) &&
		(j < WAY_BLOCKSIZE - ways_it->second.size - 3) && (j < WAY_BLOCKSIZE - way_rd_buf[i] - 3))
        {
          if (ways_it->first < (int)(way_rd_buf[i+1]))
          {
            way_wr_buf[j++] = ways_it->second.id;
            way_wr_buf[j++] = ways_it->first;
            way_wr_buf[j++] = ways_it->second.size;
            memcpy(&(way_wr_buf[j]), ways_it->second.nodes, (ways_it->second.size)*sizeof(uint32));
            j += ways_it->second.size;
	    ++ways_it;
	  }
          else
          {
            memcpy(&(way_wr_buf[j]), &(way_rd_buf[i]), (way_rd_buf[i+2]+3)*sizeof(uint32));
            j += way_rd_buf[i+2]+3;
            i += way_rd_buf[i+2]+3;
          }
        }
	while ((j < blocksize) && (ways_it != ways_it2) && (j < WAY_BLOCKSIZE - ways_it->second.size - 3))
        {
          way_wr_buf[j++] = ways_it->second.id;
          way_wr_buf[j++] = ways_it->first;
          way_wr_buf[j++] = ways_it->second.size;
          memcpy(&(way_wr_buf[j]), ways_it->second.nodes, (ways_it->second.size)*sizeof(uint32));
          j += ways_it->second.size;
	  ++ways_it;
	}
        while ((j < blocksize) && (i < way_rd_buf[0]) && (j < WAY_BLOCKSIZE - way_rd_buf[i] - 3))
        {
          memcpy(&(way_wr_buf[j]), &(way_rd_buf[i]), (way_rd_buf[i+2]+3)*sizeof(uint32));
          j += way_rd_buf[i+2]+3;
          i += way_rd_buf[i+2]+3;
        }
        
        lseek64(ways_dat_fd, (int64)cur_block*(WAY_FILE_BLOCK_SIZE), SEEK_SET);
        way_wr_buf[0] = j-1;
        write(ways_dat_fd, way_wr_buf, WAY_FILE_BLOCK_SIZE);
        
        cur_block = next_block_id;
        if ((i >= way_rd_buf[0]) || (ways_it->first < (int)(way_rd_buf[i+1])))
          block_index_way.insert(make_pair< int, unsigned int >(ways_it->first, next_block_id++));
        else
          block_index_way.insert(make_pair< int, unsigned int >(way_rd_buf[i+1], next_block_id++));
        new_element_count -= (j-1);
      }
      
      unsigned int j(1);
      while ((ways_it != ways_it2) && (i < way_rd_buf[0]))
      {
	if (ways_it->first < (int)(way_rd_buf[i+1]))
	{
          way_wr_buf[j++] = ways_it->second.id;
          way_wr_buf[j++] = ways_it->first;
          way_wr_buf[j++] = ways_it->second.size;
          memcpy(&(way_wr_buf[j]), ways_it->second.nodes, (ways_it->second.size)*sizeof(uint32));
          j += ways_it->second.size;
	  ++ways_it;
        }
        else
        {
          memcpy(&(way_wr_buf[j]), &(way_rd_buf[i]), (way_rd_buf[i+2]+3)*sizeof(uint32));
          j += way_rd_buf[i+2]+3;
          i += way_rd_buf[i+2]+3;
        }
      }
      while (ways_it != ways_it2)
      {
        way_wr_buf[j++] = ways_it->second.id;
        way_wr_buf[j++] = ways_it->first;
        way_wr_buf[j++] = ways_it->second.size;
        memcpy(&(way_wr_buf[j]), ways_it->second.nodes, (ways_it->second.size)*sizeof(uint32));
        j += ways_it->second.size;
	++ways_it;
      }
      while (i < way_rd_buf[0])
      {
        memcpy(&(way_wr_buf[j]), &(way_rd_buf[i]), (way_rd_buf[i+2]+3)*sizeof(uint32));
        j += way_rd_buf[i+2]+3;
        i += way_rd_buf[i+2]+3;
      }
      
      lseek64(ways_dat_fd, (int64)cur_block*(WAY_FILE_BLOCK_SIZE), SEEK_SET);
      way_wr_buf[0] = j-1;
      write(ways_dat_fd, way_wr_buf, WAY_FILE_BLOCK_SIZE);
      
      ways_it = ways_it2;
    }
  }
}

void prepare_nodes_chunk(uint32 offset, uint32 count, uint32* ll_idx_buf)
{
  int nodes_dat_fd = open64(NODE_DATA, O_RDONLY);
  
  int* rd_buf = (int*) malloc(NODE_FILE_BLOCK_SIZE);
  Node* nodes_rd_buf((Node*)(&rd_buf[1]));
  
  while (read(nodes_dat_fd, rd_buf, NODE_FILE_BLOCK_SIZE))
  {
    for (int j(0); j < rd_buf[0]; ++j)
    {
      if (((uint32)(nodes_rd_buf[j].id) >= offset) && (nodes_rd_buf[j].id - offset < count))
        ll_idx_buf[nodes_rd_buf[j].id - offset] = ll_idx(nodes_rd_buf[j].lat, nodes_rd_buf[j].lon);
    }
  }
  
  free(rd_buf);
  rd_buf = 0;
  
  close(nodes_dat_fd);
}

void postprocess_ways(int fd, uint32*& buf)
{
  close(fd);
  free(buf);
  buf = 0;
}

void postprocess_ways_2()
{
  const uint32 max_nodes_ram = 200*1000*1000;
  uint32 structure_count(0);
  way_wr_buf = (uint32*) malloc(WAY_FILE_BLOCK_SIZE);
  way_rd_buf = (uint32*) malloc(WAY_FILE_BLOCK_SIZE);
  
  ways_dat_fd = open64(WAY_DATA, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  close(ways_dat_fd);
  
  ways_dat_fd = open64(WAY_DATA, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if (ways_dat_fd < 0)
  {
    cerr<<"open64: "<<errno<<'\n';
    exit(0);
  }
  
  int ways_tmp_fd = open64(WAY_ALLTMP, O_RDONLY);
  int32 offset(0), count(max_nodes_ram);
  uint32* ll_idx = (uint32*) malloc(sizeof(int32)*count);
  
  while (offset < max_node_id)
  {
    C_Way way;
    cerr<<'n';
    prepare_nodes_chunk(offset, count, ll_idx);
    cerr<<'n';
    lseek64(ways_tmp_fd, 0, SEEK_SET);
    
    while (read(ways_tmp_fd, &(way.id), sizeof(uint32)))
    {
      read(ways_tmp_fd, &(way.size), sizeof(uint32));
      way.nodes = (uint32*) malloc(sizeof(uint32)*way.size);
      read(ways_tmp_fd, way.nodes, way.size*sizeof(uint32));
      
      if (((int32)way.nodes[0] >= offset) && ((int32)way.nodes[0] - offset < count))
      {
	ways_by_first_idx.insert
          (make_pair< uint32, C_Way >(ll_idx[way.nodes[0] - offset], way));
        structure_count += way.size + 3;
        
        if (structure_count >= WAY_FLUSH_INTERVAL)
        {
          flush_ways(ways_by_first_idx);
          
          for (multimap< int, C_Way >::iterator it(ways_by_first_idx.begin());
               it != ways_by_first_idx.end(); ++it)
            free(it->second.nodes);
          ways_by_first_idx.clear();
          
          cerr<<'w';
          structure_count = 0;
        }
      }
      else
        free(way.nodes);
    }
    
    flush_ways(ways_by_first_idx);
    
    for (multimap< int, C_Way >::iterator it(ways_by_first_idx.begin());
         it != ways_by_first_idx.end(); ++it)
      free(it->second.nodes);
    ways_by_first_idx.clear();
    
    cerr<<'w';
    offset += count;
  }
  
  free(ll_idx);
  free(way_rd_buf);
  free(way_wr_buf);
  way_rd_buf = 0;
  way_wr_buf = 0;
  
  close(ways_dat_fd);
  close(ways_tmp_fd);

  pair< int, unsigned int >* buf = (pair< int, unsigned int >*)
      malloc(sizeof(int)*2*block_index_way.size());
  
  uint32 i(0);
  for (multimap< int, uint16 >::const_iterator it(block_index_way.begin());
       it != block_index_way.end(); ++it)
    buf[i++] = *it;
  
  int ways_idx_fd = open64(WAY_IDX, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  write(ways_idx_fd, buf, sizeof(int)*2*block_index_way.size());
  close(ways_idx_fd);
  
  free(buf);
}

void postprocess_ways_3()
{
  way_rd_buf = (uint32*) malloc(WAY_FILE_BLOCK_SIZE);
  
  uint16* idx_buf = (uint16*) malloc(sizeof(uint16)*max_way_id);
  
  ways_dat_fd = open64(WAY_DATA, O_RDONLY);
  
  uint32 i(0);
  while (read(ways_dat_fd, way_rd_buf, WAY_FILE_BLOCK_SIZE))
  {
    for (uint32 j(1); j < way_rd_buf[0]; j += way_rd_buf[j+2]+3)
      idx_buf[way_rd_buf[j]-1] = i;
    ++i;
  }
  int ways_idx_fd = open64(WAY_IDXA, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  write(ways_idx_fd, idx_buf, sizeof(uint16)*max_way_id);
  close(ways_idx_fd);
  close(ways_dat_fd);
  
  free(idx_buf);
  free(way_rd_buf);
  way_rd_buf = 0;
}

void postprocess_ways_4()
{
  way_rd_buf = (uint32*) malloc(WAY_FILE_BLOCK_SIZE);
  
  const uint32 max_nodes_ram = 200*1000*1000;
  int32 offset(0), count(max_nodes_ram);
  uint32* ll_idx = (uint32*) malloc(sizeof(int32)*count);
  ways_dat_fd = open64(WAY_DATA, O_RDONLY);
  
  set< pair< int32, uint16 > > idx_block;
  
  while (offset < max_node_id)
  {
    cerr<<'n';
    prepare_nodes_chunk(offset, count, ll_idx);
    cerr<<'n';
    
    lseek64(ways_dat_fd, 0, SEEK_SET);
    uint16 i(0);
    while (read(ways_dat_fd, way_rd_buf, WAY_FILE_BLOCK_SIZE))
    {
      for (uint32 j(1); j < way_rd_buf[0]; j += way_rd_buf[j+2]+3)
      {
	for (uint32 k(j+3); k < j+way_rd_buf[j+2]+3; ++k)
	{
	  if (((int32)way_rd_buf[k] >= offset) && ((int32)way_rd_buf[k] - offset < count))
	    idx_block.insert(make_pair< int32, uint16 >
		((ll_idx[way_rd_buf[k] - offset]) & WAY_IDX_BITMASK, i));
	}
      }
      ++i;
    }
    
    offset += count;
  }
  
  int ways_idx_fd = open64(WAY_IDXSPAT, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  for (set< pair< int32, uint16 > >::const_iterator it(idx_block.begin()); it != idx_block.end(); ++it)
    write(ways_idx_fd, &(*it), sizeof(pair< int32, uint16 >));
  close(ways_idx_fd);
  
  free(ll_idx);
  free(way_rd_buf);
  way_rd_buf = 0;
  
  close(ways_dat_fd);
}

//-----------------------------------------------------------------------------

const int NODE = 1;
const int WAY = 2;
const int RELATION = 3;
// int tag_type(0);
// unsigned int current_id(0);

const unsigned int FLUSH_INTERVAL = 32*1024*1024;
const unsigned int FLUSH_TAGS_INTERVAL = 1024*1024;
const unsigned int DOT_INTERVAL = 512*1024;
unsigned int structure_count(0);
int state(0);
const int NODES = 1;
const int WAYS = 2;
int ways_fd;
uint32* way_buf;
uint32 way_buf_pos(0);

// unsigned int way_member_count(0);

multimap< int, Node > nodes;

//-----------------------------------------------------------------------------

struct KeyValue
{
  KeyValue(string key_, string value_) : key(key_), value(value_) {}
  
  string key, value;
};

inline bool operator<(const KeyValue& kv_1, const KeyValue& kv_2)
{
  if (kv_1.key < kv_2.key)
    return true;
  else if (kv_1.key > kv_2.key)
    return false;
  return (kv_1.value < kv_2.value);
}

struct NodeCollection
{
  NodeCollection() : position(0), bitmask(0) {}
  
  void insert(int32 node_id, int32 ll_idx)
  {
    if (nodes.empty())
    {
      position = ll_idx;
      bitmask = 0;
    }
    nodes.push_back(node_id);
    bitmask |= (position ^ ll_idx);
  }
  
  int32 position;
  uint32 bitmask;
  vector< int32 > nodes;
};

map< KeyValue, NodeCollection > node_tags;
int current_type(0);
int32 current_id;
int32 current_ll_idx;
set< string > allowed_node_tags;
bool no_tag_limit(true);
uint tag_count(0);

void start(const char *el, const char **attr)
{
  if (!strcmp(el, "tag"))
  {
    if (current_type != 0)
    {
      string key(""), value("");
      for (unsigned int i(0); attr[i]; i += 2)
      {
	if (!strcmp(attr[i], "k"))
	  key = attr[i+1];
	if (!strcmp(attr[i], "v"))
	  value = attr[i+1];
      }
      // patch cope with server power limits by ignoring uncommon node tags
      if ((current_type != NODE) || (no_tag_limit) ||
	   (allowed_node_tags.find(key) != allowed_node_tags.end()))
      {
	NodeCollection& nc(node_tags[KeyValue(key, value)]);
	nc.insert(current_id, current_ll_idx);
	if (++tag_count >= FLUSH_TAGS_INTERVAL)
	{
	  int spread_0(0), spread_8(0), spread_16(0), spread_24(0);
	  for (map< KeyValue, NodeCollection >::const_iterator it(node_tags.begin());
		      it != node_tags.end(); ++it)
	  {
	    cout<<'['<<it->first.key<<"]["<<it->first.value<<"]: "
		<<hex<<it->second.bitmask<<' '<<dec<<it->second.nodes.size()<<'\n';
	    if (it->second.bitmask>>24)
	      ++spread_0;
	    else if (it->second.bitmask>>16)
	      ++spread_8;
	    else if (it->second.bitmask>>8)
	      ++spread_16;
	    else
	      ++spread_24;
	  }
	  cout<<"Stats: "<<spread_0<<' '<<spread_8<<' '<<spread_16<<' '<<spread_24<<"\n\n";
	  node_tags.clear();
	  tag_count = 0;
	}
      }
    }
  }
  else if (!strcmp(el, "nd"))
  {
    if (way_buf_pos < MAXWAYNODES)
    {
      unsigned int ref(0);
      for (unsigned int i(0); attr[i]; i += 2)
      {
	if (!strcmp(attr[i], "ref"))
	  ref = atoi(attr[i+1]);
      }
      way_buf[way_buf_pos+2] = ref;
      ++way_buf_pos;
    }
  }
  else if (!strcmp(el, "node"))
  {
    unsigned int id(0);
    int lat(100*10000000), lon(200*10000000);
    for (unsigned int i(0); attr[i]; i += 2)
    {
      if (!strcmp(attr[i], "id"))
	id = atoi(attr[i+1]);
      if (!strcmp(attr[i], "lat"))
	lat = (int)in_lat_lon(attr[i+1]);
      if (!strcmp(attr[i], "lon"))
	lon = (int)in_lat_lon(attr[i+1]);
    }
    nodes.insert(make_pair< int, Node >
	(ll_idx(lat, lon), Node(id, lat, lon)));
    
    current_type = NODE;
    current_id = id;
    current_ll_idx = ll_idx(lat, lon);
  }
  else if (!strcmp(el, "way"))
  {
    if (state == NODES)
    {
      {
	int spread_0(0), spread_8(0), spread_16(0), spread_24(0);
	for (map< KeyValue, NodeCollection >::const_iterator it(node_tags.begin());
		    it != node_tags.end(); ++it)
	{
	  cout<<'['<<it->first.key<<"]["<<it->first.value<<"]: "
	      <<hex<<it->second.bitmask<<' '<<dec<<it->second.nodes.size()<<'\n';
	  if (it->second.bitmask>>24)
	    ++spread_0;
	  else if (it->second.bitmask>>16)
	    ++spread_8;
	  else if (it->second.bitmask>>8)
	    ++spread_16;
	  else
	    ++spread_24;
	}
	cout<<"Stats: "<<spread_0<<' '<<spread_8<<' '<<spread_16<<' '<<spread_24<<"\n\n";
	tag_count = 0;
      }
      exit(0);
      //flush_nodes(nodes);
      nodes.clear();
      //postprocess_nodes();
      
      state = WAYS;
      
      //ways_fd = prepare_ways(way_buf);
    }
    uint32 id(0);
    for (unsigned int i(0); attr[i]; i += 2)
    {
      if (!strcmp(attr[i], "id"))
	id = atoi(attr[i+1]);
    }
    way_buf[0] = id;
    way_buf_pos = 0;
    if (id > max_way_id)
      max_way_id = id;
  }
  else if (!strcmp(el, "relation"))
  {
    if (state == WAYS)
    {
/*      postprocess_ways(ways_fd, way_buf);
      postprocess_ways_2();
      postprocess_ways_3();
      postprocess_ways_4();*/
      
      state = 0;
    }
  }
}

void end(const char *el)
{
  if (!strcmp(el, "nd"))
  {
    ++structure_count;
    if (structure_count % DOT_INTERVAL == 0)
      cerr<<'.';
  }
  else if (!strcmp(el, "node"))
  {
    current_type = 0;
    ++structure_count;
    if (structure_count % DOT_INTERVAL == 0)
      cerr<<'.';
  }
  else if (!strcmp(el, "way"))
  {
    if (way_buf_pos < MAXWAYNODES)
    {
      way_buf[1] = way_buf_pos;
      //write(ways_fd, way_buf, sizeof(uint32)*(way_buf_pos+2));
    }
  }
  else if (!strcmp(el, "relation"))
  {
  }
  if (structure_count >= FLUSH_INTERVAL)
  {
    if (state == NODES)
    {
      //flush_nodes(nodes);
      nodes.clear();
    }
    
    structure_count = 0;
  }
}

int main(int argc, char *argv[])
{
  int i(0);
  while (++i < argc)
  {
    if (!strcmp(argv[i], "--limit-node-tags"))
      no_tag_limit = false;
    else
    {
      cout<<"Usage: "<<argv[0]<<" [--limit-node-tags]\n";
      return 0;
    }
  }
  
  // patch cope with server power limits by ignoring uncommon node tags
  allowed_node_tags.insert("highway");
  allowed_node_tags.insert("traffic-calming");
  allowed_node_tags.insert("barrier");
  allowed_node_tags.insert("waterway");
  allowed_node_tags.insert("lock");
  allowed_node_tags.insert("railway");
  allowed_node_tags.insert("aeroway");
  allowed_node_tags.insert("aerialway");
  allowed_node_tags.insert("power");
  allowed_node_tags.insert("man_made");
  allowed_node_tags.insert("leisure");
  allowed_node_tags.insert("amenity");
  allowed_node_tags.insert("shop");
  allowed_node_tags.insert("tourism");
  allowed_node_tags.insert("historic");
  allowed_node_tags.insert("landuse");
  allowed_node_tags.insert("military");
  allowed_node_tags.insert("natural");
  allowed_node_tags.insert("route");
  allowed_node_tags.insert("sport");
  allowed_node_tags.insert("bridge");
  allowed_node_tags.insert("crossing");
  allowed_node_tags.insert("mountain_pass");
  allowed_node_tags.insert("ele");
  allowed_node_tags.insert("operator");
  allowed_node_tags.insert("opening-hours");
  allowed_node_tags.insert("disused");
  allowed_node_tags.insert("wheelchair");
  allowed_node_tags.insert("noexit");
  allowed_node_tags.insert("traffic_sign");
  allowed_node_tags.insert("name");
  allowed_node_tags.insert("alt_name");
  allowed_node_tags.insert("int_name");
  allowed_node_tags.insert("nat_name");
  allowed_node_tags.insert("reg_name");
  allowed_node_tags.insert("loc_name");
  allowed_node_tags.insert("ref");
  allowed_node_tags.insert("int_ref");
  allowed_node_tags.insert("nat_ref");
  allowed_node_tags.insert("reg_ref");
  allowed_node_tags.insert("loc_ref");
  allowed_node_tags.insert("old_ref");
  allowed_node_tags.insert("source_ref");
  allowed_node_tags.insert("icao");
  allowed_node_tags.insert("iata");
  allowed_node_tags.insert("place");
  allowed_node_tags.insert("place_numbers");
  allowed_node_tags.insert("postal_code");
  allowed_node_tags.insert("is_in");
  allowed_node_tags.insert("population");
  
  cerr<<(uintmax_t)time(NULL)<<'\n';
  
  //TEMP
/*  max_node_id = 400*1000*1000;
  max_way_id = 50*1000*1000;
  postprocess_ways_2();
  postprocess_ways_3();
  postprocess_ways_4();
  cerr<<(uintmax_t)time(NULL)<<'\n';
  return 0;*/
  
  //prepare_nodes();
  
  state = NODES;
  
  //reading the main document
  parse(stdin, start, end);
  
  if (state == NODES)
  {
    //flush_nodes(nodes);
    nodes.clear();
    //postprocess_nodes();
  }
  else if (state == WAYS)
  {
/*    postprocess_ways(ways_fd, way_buf);
    postprocess_ways_2();
    postprocess_ways_3();
    postprocess_ways_4();*/
  }
  
  cerr<<'\n'<<(uintmax_t)time(NULL)<<'\n';
  return 0;
}