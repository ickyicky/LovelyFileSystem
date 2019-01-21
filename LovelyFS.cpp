#include <string>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace std;

#define BLOCK_SIZE 4096 //4kb

class LoveLyFS{
private:
  using str = char[64];              //each str we consider as 64 chars
  struct tag{
    str name;
  };                                //tags take 64 byte
  struct inode{
    str filename;                   //name of file
    unsigned int number;             //number in table
    unsigned int size;              //filesize
    int tags[11];           //tags of file, index of them
  };                                //inode takes 128 bytes
  using buffer = char[BLOCK_SIZE];  //we want buffer to accomulate exacly one block
  str filename;                  //filename of virtual fs storage
  unsigned int size;                //basically number of indoes in current fs (for user)
  unsigned int SYSTEM_BLOCKS;       //number of system blocks
  unsigned int size_in_use;         //number of inodes in use
  unsigned int inodes_blocks;
  unsigned int blocks_for_u;
  unsigned int next_blocks;
  vector<bool> u;                   //used table here
  vector<inode> inodes;             //table of inodes
  vector<tag> tags;                 //tags table
  vector<unsigned int> next;         //next table


  std::ifstream::pos_type file_size(const char* filename)
  {
      std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
      return in.tellg();
  }


  int find_tag_index(string name)
  {
    for(int i = 0; i < 64; ++i)
      if(strcmp(tags[i].name, name.c_str()) == 0)
        return i;

    return -1;
  }

  inode &find_inode(const str name, int tags[11]) //TODO find by tags
  {
    for(inode& node: inodes)
    {
      bool is = (strcmp(node.filename,name) == 0);
      for(int i = 0; i < 11; ++i)
        if(node.tags[i] != tags[i])
          is = false;

      if(is)
        return node;
    }

    return inodes[0];
  }

  int alloc(int fsize)
  {
    //ifirst, check if we have at least size left space
    if(size - size_in_use < fsize)
      throw "not enough space";

    unsigned int current,first;

    //searching for ifirst avalibe free block
    for(int i = 0; i < size; ++i)
      if(!u[i])
        {
          current = first = i;
          break;
        }

    while(fsize-- > 0)
    {
      for(int i = current; i < size; ++i)
        if(!u[i])
          {
            next[current] = i;
            u[i] = true;
            current = i;
            break;
          }
    }
    //if the inode doesnt have next, we basically set its next to itself
    next[current] = current;

    return first;
  }
public:
  LoveLyFS()
  {
  }//initializer

  ~LoveLyFS()
  {
    close();
  }//destructor

  void create_fs(const str name, const unsigned int fsize)
  {
    /* FILESYSTEM ORDER
    Fixed blocks:
      1. super block, takes exacly one block
      2. tags block, takes exacly one block
    For each block (4096 bytes) we need:
      1. inode(128b), 4kb (1 block) can store 32 indoes
      2. 1 position in u table, takes 1b per position, 4096 positions per block
      3. 1 position in next table, each entry requires 4 bytes (unsigned int), 1024 pos. per block
    */

    strcpy(filename, name);
    size = fsize - 1;
    SYSTEM_BLOCKS;
    do
    {
      --size;
      inodes_blocks = size * sizeof(inode) / BLOCK_SIZE + 1;
      blocks_for_u = size / BLOCK_SIZE + 1;
      next_blocks = size * sizeof(unsigned int) / BLOCK_SIZE + 1;
      SYSTEM_BLOCKS = 2 + inodes_blocks + blocks_for_u + next_blocks;
    } while(SYSTEM_BLOCKS + size > fsize);

    //time to write empty system block and non empty super block
    buffer buf;
    for(unsigned int i=0; i<BLOCK_SIZE; ++i)
    {
      buf[i] = '\0';
    }

    fstream fs;
    fs.open(filename, ios::out|ios::binary);

    //at the beginnig we have filename
    fs.write(filename, sizeof(str));
    //then, number of indoes
    fs.write((char*)&size,sizeof(size));
    //then SYSTEM_BLOCKS number
    fs.write((char*)&SYSTEM_BLOCKS,sizeof(SYSTEM_BLOCKS));
    //then, number of used inodes (used space)
    fs.write((char*)&size_in_use,sizeof(size_in_use));
    fs.write((char*)&inodes_blocks,sizeof(inodes_blocks));
    fs.write((char*)&blocks_for_u,sizeof(blocks_for_u));
    fs.write((char*)&next_blocks,sizeof(next_blocks));
    //the rest is none
    fs.write(buf, BLOCK_SIZE - sizeof(str) - 6 * sizeof(unsigned int));

    for(unsigned int i=1; i<fsize; ++i)
    {
      fs.write(buf, BLOCK_SIZE);
    }

    inodes.resize(size);
    for(int i = 0; i < size; ++i)
    {
      strcpy(inodes[i].filename, "");
      inodes[i].number = i;
      for(auto &j: inodes[i].tags)
        j = -1;
    }

    u.resize(size);
    for(auto x: u)
      x = false;

    next.resize(size);
    for(int i = 0; i < size; ++i)
      next[i] = i;

    tags.resize(64);
    for(auto &t: tags)
      strcpy(t.name, "");
  }//create_fs

  void open(const str name)
  {
    unsigned int total_size = file_size(filename) / BLOCK_SIZE;
    fstream strm;
    strm.open(name, ios::in|ios::binary);
    buffer buf;
    size_in_use = 0;

    //reading super block
    strm.read(buf, BLOCK_SIZE);
    strcpy(filename, *reinterpret_cast<str *>(buf));
    size = *reinterpret_cast<unsigned int *>(buf + sizeof(str));
    SYSTEM_BLOCKS = *reinterpret_cast<unsigned int *>(buf + sizeof(str) + sizeof(unsigned int));
    size_in_use = *reinterpret_cast<unsigned int *>(buf + sizeof(str) + 2 * sizeof(unsigned int));
    inodes_blocks = *reinterpret_cast<unsigned int *>(buf + sizeof(str) + 3 * sizeof(unsigned int));
    blocks_for_u = *reinterpret_cast<unsigned int *>(buf + sizeof(str) + 4 * sizeof(unsigned int));
    next_blocks = *reinterpret_cast<unsigned int *>(buf + sizeof(str) + 5 * sizeof(unsigned int));

    strm.seekg(BLOCK_SIZE);
    //reading u
    u.resize(size);
    unsigned int u_per_block = BLOCK_SIZE / sizeof(bool);
    for(int i = 0; i < blocks_for_u; i++)
    {
      strm.read(buf, BLOCK_SIZE);
      for(int j = 0; j < u_per_block; j++)
      {
        if(i * u_per_block + j >= size) break;
        u[i * u_per_block + j] = *reinterpret_cast<bool *>(buf + j * (sizeof(bool)));
        if(u[i * u_per_block + j]) size_in_use++;
      }
    }

    //reading tags
    strm.seekg(BLOCK_SIZE*(1+blocks_for_u));
    tags.resize(64);
    strm.read(buf, BLOCK_SIZE);
    for(int i = 0; i < 64; ++i)
      tags[i] = *reinterpret_cast<tag *>(buf + i * sizeof(tag));



    //reding next
    strm.seekg(BLOCK_SIZE*(2+blocks_for_u));
    next.resize(size);
    unsigned int next_per_block = BLOCK_SIZE / sizeof(unsigned int);
    for(int i = 0; i < next_blocks; i++)
    {
      strm.read(buf, BLOCK_SIZE);
      for(int j = 0; j < next_per_block; ++j)
      {
        if(i * next_per_block + j >= size) break;
        next[i * next_per_block + j] = *reinterpret_cast<unsigned int *>(buf + j * (sizeof(unsigned int)));
      }
    }

    //reading inodes
    inodes.resize(size);
    unsigned int inodes_per_block = BLOCK_SIZE / sizeof(inode);
    for(int i = 0; i < inodes_blocks; ++i)
    {
      strm.seekg(BLOCK_SIZE*(2+blocks_for_u+next_blocks+i));
      strm.read(buf, BLOCK_SIZE);
      for(int j = 0; j < inodes_per_block; ++j)
      {
        if(i * inodes_per_block + j >= size) break;
        inodes[i * inodes_per_block + j] = *reinterpret_cast<inode *>(buf + j * sizeof(inode));
      }
    }

  }//open fs

  void close() //TODO
  {
    //we only have to write system blocks
    buffer buf;
    for(unsigned int i=0; i<BLOCK_SIZE; ++i)
    {
      buf[i] = '\0';
    }

    fstream fs;
    fs.open(filename, ios::in|ios::out|ios::binary);

    //at the beginnig we have filename
    fs.write(filename, sizeof(str));
    //then, number of indoes
    fs.write((char*)&size,sizeof(size));
    //then SYSTEM_BLOCKS number
    fs.write((char*)&SYSTEM_BLOCKS,sizeof(SYSTEM_BLOCKS));
    //then, number of used inodes (used space)
    fs.write((char*)&size_in_use,sizeof(size_in_use));
    fs.write((char*)&inodes_blocks,sizeof(inodes_blocks));
    fs.write((char*)&blocks_for_u,sizeof(blocks_for_u));
    fs.write((char*)&next_blocks,sizeof(next_blocks));
    //the rest is none
    fs.write(buf, BLOCK_SIZE - sizeof(str) - 6 * sizeof(unsigned int));

    //writing used
    fs.seekg(BLOCK_SIZE);
    unsigned int empty_blocks;
    for(unsigned int i = 0; i < blocks_for_u; ++i)
    {
      empty_blocks = BLOCK_SIZE;
      for(unsigned int j = 0; j < BLOCK_SIZE; ++j)
      {
        bool temp = u[BLOCK_SIZE * i + j];
        fs.write((char*)&temp,sizeof(temp));
        --empty_blocks;
      }
      fs.write(buf, empty_blocks);
    }

    //writing tags
    fs.seekg(BLOCK_SIZE*(1+blocks_for_u));
    empty_blocks = BLOCK_SIZE;
    for(unsigned int j = 0; j < 64; ++j)
    {
      fs.write((char*)&tags[j],sizeof(tags[j]));
      empty_blocks -= sizeof(tag);
    }
    fs.write(buf, empty_blocks);

    //writing next
    fs.seekg(BLOCK_SIZE*(2+blocks_for_u));
    unsigned int next_per_block = BLOCK_SIZE / sizeof(unsigned int);
    for(unsigned int i = 0; i < next_blocks; ++i)
    {
      empty_blocks = BLOCK_SIZE;
      for(unsigned int j = 0; j < next_per_block; ++j)
      {
        fs.write((char*)&next[next_per_block * i + j],sizeof(next[next_per_block * i + j]));
        empty_blocks -= sizeof(unsigned int);
      }
      fs.write(buf, empty_blocks);
    }

    //writing inodes
    unsigned int i_per_block = BLOCK_SIZE / sizeof(inode);
    for(unsigned int i = 0; i < inodes_blocks; ++i)
    {
      fs.seekg(BLOCK_SIZE*(2+blocks_for_u+next_blocks+i));
      empty_blocks = BLOCK_SIZE;
      for(unsigned int j = 0; j < i_per_block; ++j)
      {
        fs.write((char*)&inodes[i_per_block * i + j],sizeof(inodes[i_per_block * i + j]));
        empty_blocks -= sizeof(inode);
      }
      fs.write(buf, empty_blocks);
    }

  }//close fs

  void add_tag(str name)
  {
    if(tags.size() == 16)
      throw "tags full";

    tag ntag;
    strcpy(ntag.name, name);
    tags.push_back(ntag);
  }

  void delete_tag(str name)
  {
    //TODO
  }

  void upload_file(const str name, string tags[11])
  {
    int int_tags[11];
    for(int i=0; i < 11; ++i)
      int_tags[i] = find_tag_index(tags[i]);

    inode& found = find_inode(name, int_tags);
    if(strcmp(found.filename,name)==0 && u[found.number])
      throw "Not unique name and tags!";

    unsigned int ffsize = file_size(name);
    unsigned int fsize = ffsize / BLOCK_SIZE + 1;
    unsigned int current_i = alloc(fsize);
    size_in_use += fsize;

    //reading file to vs
    fstream file;
    file.open(name, ios::in|ios::binary);
    //TODO add tags
    fstream vs;
    vs.open(filename, ios::in|ios::out|ios::binary);
    for(unsigned int i = 0; i < fsize; ++i)
    {
      inodes[current_i].size = ffsize > BLOCK_SIZE? BLOCK_SIZE: ffsize;

      strcpy(inodes[current_i].filename, name);
      for(int i=0; i < 11; ++i)
        inodes[current_i].tags[i] = int_tags[i];

      ffsize -= BLOCK_SIZE;
      buffer temp;
      file.read(temp, inodes[current_i].size);
      vs.seekg((SYSTEM_BLOCKS + inodes[current_i].number) * BLOCK_SIZE);
      vs.write(temp, inodes[current_i].size);
      current_i = next[current_i];
    }
  }

  void download_file(const str name, const str outputname, string tags[11])
  {
    int int_tags[11];
    for(int i=0; i < 11; ++i)
      int_tags[i] = find_tag_index(tags[i]);

    auto &current = find_inode(name, int_tags);
    if(strcmp(current.filename,name)!=0)
      throw "Invalid name or tags";

    //read file to buffer
    fstream vs;
    fstream output;
    vs.open(filename, ios::in|ios::binary);
    output.open(outputname, ios::out|ios::binary);
    while(true)
    {
      buffer temp;
      vs.seekg((SYSTEM_BLOCKS + current.number) * BLOCK_SIZE);
      vs.read(temp, current.size);
      output.write(temp, current.size);
      auto previous = current.number;
      current = inodes[next[current.number]];
      if(previous == current.number)
        break;
    }
  }

  void delete_file(const str name, string tags[11])
  {
    int int_tags[11];
    for(int i=0; i < 11; ++i)
      int_tags[i] = find_tag_index(tags[i]);

    auto &current = find_inode(name, int_tags);
    if(strcmp(current.filename,name)!=0)
      throw "Invalid name or tags";
    unsigned int previous;

    while(true)
    {
      u[current.number] = false;
      previous = current.number;
      current = inodes[next[current.number]];
      if(previous == current.number)
        break;
    }
  }

  void map_fs()
  {
    cout<<"LEGEND:"<<endl<<"[s] - system block"<<endl<<"[u :name] - used block"<<endl<<"[n] - not used block"<<endl;
    for(int i = 0; i < SYSTEM_BLOCKS; i++)
      cout<<"[s]";

    for(int i = 0; i<size; ++i)
      {
        if(u[i]) cout<<"[u :"<<inodes[i].filename<<"]";
        else cout<<"[n]";
      }
  }
};


#include <algorithm>

char* getCmdOption(char ** begin, char ** end, const string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const string& option)
{
    return find(begin, end, option) != end;
}

int main(int argc, char * argv[])
{
    if(cmdOptionExists(argv, argv+argc, "-h"))
    {
        cout<<"opcje programu:"<<endl;
        cout<<"-f wymagane, plik z virtualnym systemem plikow"<<endl;
        cout<<"-c -fs tworzy wirtualny system o  fs blokach, w innym przypadku nastapi proba wczytania"<<endl;
        cout<<"-u <filename> upload pliku o podanej nazwie na dysk"<<endl;
        cout<<"-d <filename> -dd <destination_filename> pobiera plik"<<endl;
        cout<<"-r <filename> usuwa plik"<<endl;
        cout<<"-m wyswietla zawartosc syku"<<endl;
        cout<<"-t uzyj tagu (domyslnie, 6 znakow)"<<endl;
    }

    LoveLyFS fs = LoveLyFS();
    string tags[11];

    char * filename = getCmdOption(argv, argv + argc, "-f");
    char * ffs = getCmdOption(argv, argv + argc, "-fs");
    if(!filename)
    {
      cout<<"Wpisz -h aby uzyskac pomoc"<<endl;
      return 1;
    }

    if(cmdOptionExists(argv, argv + argc, "-c"))
    {
      if(ffs)
        fs.create_fs(filename, atoi(ffs));
    }
    else
    {
      fs.open(filename);
    }

    char * t = getCmdOption(argv, argv + argc, "-t");
    char * u = getCmdOption(argv, argv + argc, "-u");
    char * d = getCmdOption(argv, argv + argc, "-d");
    char * dd = getCmdOption(argv, argv + argc, "-dd");
    char * r = getCmdOption(argv, argv + argc, "-r");
    bool m = cmdOptionExists(argv, argv + argc, "-m");
    if(t)
    {
      for(int i = 0; i < 6; i++)
        tags[0].push_back(t[i]);
      fs.add_tag(t);
    }
    if(u)
    {
      fs.upload_file(u, tags);
    }
    if(d)
    {
      if(dd)
      {
        fs.download_file(d, dd, tags);
      }
    }
    if(r)
    {
      fs.delete_file(r, tags);
    }
    if(m)
    {
      fs.map_fs();
    }

    return 0;
}
