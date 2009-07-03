#include <stdexcept>
#include <vector>
#include <list>
#include "3ds.h"

#ifdef _MSC_VER
typedef unsigned short uint16;
typedef unsigned int uint32;
#else
#include <stdint.h>
typedef uint16_t uint16;
typedef uint32_t uint32;
#endif

using namespace std;


// Known 3DS chunks
#define CHUNK_MAIN          0x4d4d
#define CHUNK_M3D_VERSION   0x0002
#define CHUNK_3DEDIT        0x3d3d
#define CHUNK_MESH_VERSION  0x3d3e
#define CHUNK_OBJECT        0x4000
#define CHUNK_TRIMESH       0x4100
#define CHUNK_VERTEXLIST    0x4110
#define CHUNK_MAPPINGCOORDS 0x4140
#define CHUNK_FACES         0x4120
#define CHUNK_MSH_MAT_GROUP 0x4130
#define CHUNK_MAT_ENTRY     0xafff
#define CHUNK_MAT_NAME      0xa000
#define CHUNK_MAT_TEXMAP    0xa200
#define CHUNK_MAT_MAPNAME   0xa300

// 3DS object class
class Obj3DS {
  public:
    vector<uint16> mIndices;
    vector<Vector3> mVertices;
    vector<Vector2> mUVCoords;
};


/// Read a 16-bit integer, endian independent.
static uint16 ReadInt16(istream &aStream)
{
  unsigned char buf[2];
  aStream.read((char *) buf, 2);
  return ((uint16) buf[0]) | (((uint16) buf[1]) << 8);
}

/// Write a 16-bit integer, endian independent.
static void WriteInt16(ostream &aStream, uint16 aValue)
{
  unsigned char buf[2];
  buf[0] = aValue & 255;
  buf[1] = (aValue >> 8) & 255;
  aStream.write((char *) buf, 2);
}

/// Read a 32-bit integer, endian independent.
static uint32 ReadInt32(istream &aStream)
{
  unsigned char buf[4];
  aStream.read((char *) buf, 4);
  return ((uint32) buf[0]) | (((uint32) buf[1]) << 8) |
         (((uint32) buf[2]) << 16) | (((uint32) buf[3]) << 24);
}

/// Write a 32-bit integer, endian independent.
static void WriteInt32(ostream &aStream, uint32 aValue)
{
  unsigned char buf[4];
  buf[0] = aValue & 255;
  buf[1] = (aValue >> 8) & 255;
  buf[2] = (aValue >> 16) & 255;
  buf[3] = (aValue >> 24) & 255;
  aStream.write((char *) buf, 4);
}

/// Read a Vector2, endian independent.
static Vector2 ReadVector2(istream &aStream)
{
  union {
    uint32 i;
    float  f;
  } val;
  Vector2 result;
  val.i = ReadInt32(aStream);
  result.u = val.f;
  val.i = ReadInt32(aStream);
  result.v = val.f;
  return result;
}

/// Write a Vector2, endian independent.
static void WriteVector2(ostream &aStream, Vector2 aValue)
{
  union {
    uint32 i;
    float  f;
  } val;
  val.f = aValue.u;
  WriteInt32(aStream, val.i);
  val.f = aValue.v;
  WriteInt32(aStream, val.i);
}

/// Read a Vector3, endian independent.
static Vector3 ReadVector3(istream &aStream)
{
  union {
    uint32 i;
    float  f;
  } val;
  Vector3 result;
  val.i = ReadInt32(aStream);
  result.x = val.f;
  val.i = ReadInt32(aStream);
  result.y = val.f;
  val.i = ReadInt32(aStream);
  result.z = val.f;
  return result;
}

/// Write a Vector3, endian independent.
static void WriteVector3(ostream &aStream, Vector3 aValue)
{
  union {
    uint32 i;
    float  f;
  } val;
  val.f = aValue.x;
  WriteInt32(aStream, val.i);
  val.f = aValue.y;
  WriteInt32(aStream, val.i);
  val.f = aValue.z;
  WriteInt32(aStream, val.i);
}

/// Import a 3DS file from a stream.
void Import_3DS(istream &aStream, Mesh &aMesh)
{
  uint16 chunk, count;
  uint32 chunkLen;

  // Get file size
  aStream.seekg(0, ios_base::end);
  uint32 fileSize = aStream.tellg();
  aStream.seekg(0, ios_base::beg);

  // Check file size (rough initial check)
  if(fileSize < 6)
    throw runtime_error("Invalid 3DS file format.");

  // Read & check file header identifier
  chunk = ReadInt16(aStream);
  chunkLen = ReadInt32(aStream);
  if((chunk != CHUNK_MAIN) || (chunkLen != fileSize))
    throw runtime_error("Invalid 3DS file format.");

  // Parse chunks, and store the data in a temporary list, objList...
  Obj3DS * obj = 0;
  list<Obj3DS> objList;
  bool hasUVCoords = false;
  while(aStream.tellg() < fileSize)
  {
    // Read next chunk
    chunk = ReadInt16(aStream);
    chunkLen = ReadInt32(aStream);

    // What chunk did we get?
    switch(chunk)
    {
      // 3D Edit -> Step into
      case CHUNK_3DEDIT:
        break;

      // Object -> Step into
      case CHUNK_OBJECT:
        // Skip object name (null terminated string)
        while((aStream.tellg() < fileSize) && aStream.get());

        // Create a new object
        objList.push_back(Obj3DS());
        obj = &objList.back();
        break;

      // Triangle mesh -> Step into
      case CHUNK_TRIMESH:
        break;

      // Vertex list (point coordinates)
      case CHUNK_VERTEXLIST:
        count = ReadInt16(aStream);
        if((!obj) || ((obj->mVertices.size() > 0) && (obj->mVertices.size() != count)))
        {
          aStream.seekg(count * 12, ios_base::cur);
          break;
        }
        if(obj->mVertices.size() == 0)
          obj->mVertices.resize(count);
        for(uint16 i = 0; i < count; ++ i)
          obj->mVertices[i] = ReadVector3(aStream);
        break;

      // Texture map coordinates (UV coordinates)
      case CHUNK_MAPPINGCOORDS:
        count = ReadInt16(aStream);
        if((!obj) || ((obj->mUVCoords.size() > 0) && (obj->mUVCoords.size() != count)))
        {
          aStream.seekg(count * 8, ios_base::cur);
          break;
        }
        if(obj->mUVCoords.size() == 0)
          obj->mUVCoords.resize(count);
        for(uint16 i = 0; i < count; ++ i)
          obj->mUVCoords[i] = ReadVector2(aStream);
        if(count > 0)
          hasUVCoords = true;
        break;

      // Face description (triangle indices)
      case CHUNK_FACES:
        count = ReadInt16(aStream);
        if(!obj)
        {
          aStream.seekg(count * 8, ios_base::cur);
          break;
        }
        if(obj->mIndices.size() == 0)
          obj->mIndices.resize(3 * count);
        for(uint32 i = 0; i < count; ++ i)
        {
          obj->mIndices[i * 3] = ReadInt16(aStream);
          obj->mIndices[i * 3 + 1] = ReadInt16(aStream);
          obj->mIndices[i * 3 + 2] = ReadInt16(aStream);
          ReadInt16(aStream); // Skip face flag
        }
        break;
        
      default:      // Unknown/ignored - skip past this one
        aStream.seekg(chunkLen - 6, ios_base::cur);
    }
  }

  // Convert the loaded object list to the mesh structore (merge all geometries)
  aMesh.Clear();
  for(list<Obj3DS>::iterator o = objList.begin(); o != objList.end(); ++ o)
  {
    // Append...
    uint32 idxOffset = aMesh.mIndices.size();
    uint32 vertOffset = aMesh.mVertices.size();
    aMesh.mIndices.resize(idxOffset + (*o).mIndices.size());
    aMesh.mVertices.resize(vertOffset + (*o).mVertices.size());
    if(hasUVCoords)
      aMesh.mTexCoords.resize(vertOffset + (*o).mVertices.size());

    // Transcode the data
    for(uint32 i = 0; i < (*o).mIndices.size(); ++ i)
      aMesh.mIndices[idxOffset + i] = vertOffset + uint32((*o).mIndices[i]);
    for(uint32 i = 0; i < (*o).mVertices.size(); ++ i)
      aMesh.mVertices[vertOffset + i] = (*o).mVertices[i];
    if(hasUVCoords)
    {
      if((*o).mUVCoords.size() == (*o).mVertices.size())
        for(uint32 i = 0; i < (*o).mVertices.size(); ++ i)
          aMesh.mTexCoords[vertOffset + i] = (*o).mUVCoords[i];
      else
        for(uint32 i = 0; i < (*o).mVertices.size(); ++ i)
          aMesh.mTexCoords[vertOffset + i] = Vector2(0.0f, 0.0f);
    }
  }
}

/// Export a 3DS file to a stream.
void Export_3DS(ostream &aStream, Mesh &aMesh)
{
  // First, check that the mesh fits in a 3DS file (at most 65535 triangles
  // and 65535 vertices are supported).
  if((aMesh.mIndices.size() > (3*65535)) || (aMesh.mVertices.size() > 65535))
    throw runtime_error("The mesh is too large to fit in a 3DS file.");

  // Predefined names / strings
  string objName("Object1");
  string matName("Material0");

  // Get mesh properties
  uint32 triCount = aMesh.mIndices.size() / 3;
  uint32 vertCount = aMesh.mVertices.size();
  bool hasUVCoors = (aMesh.mTexCoords.size() == aMesh.mVertices.size());

  // Calculate the material chunk size
  uint32 materialSize = 0;
  uint32 matGroupSize = 0;
  if(hasUVCoors && aMesh.mTexFileName.size() > 0)
  {
    materialSize += 24 + matName.size() + 1 + aMesh.mTexFileName.size() + 1;
    matGroupSize += 8 + matName.size() + 1 + 2 * triCount;
  }

  // Calculate the mesh chunk size
  uint32 triMeshSize = 22 + 8 * triCount + 12 * vertCount + matGroupSize;
  if(hasUVCoors)
    triMeshSize += 8 + 8 * vertCount;

  // Calculate the total file size
  uint32 fileSize = 34 + objName.size() + 1 + materialSize + triMeshSize;

  // Write file header
  WriteInt16(aStream, CHUNK_MAIN);
  WriteInt32(aStream, fileSize);
  WriteInt16(aStream, CHUNK_M3D_VERSION);
  WriteInt32(aStream, 6 + 4);
  WriteInt32(aStream, 0x00000003);

  // 3D Edit chunk
  WriteInt16(aStream, CHUNK_3DEDIT);
  WriteInt32(aStream, 16 + materialSize + objName.size() + 1 + triMeshSize);
  WriteInt16(aStream, CHUNK_MESH_VERSION);
  WriteInt32(aStream, 6 + 4);
  WriteInt32(aStream, 0x00000003);

  // Material chunk
  if(materialSize > 0)
  {
    WriteInt16(aStream, CHUNK_MAT_ENTRY);
    WriteInt32(aStream, materialSize);
    WriteInt16(aStream, CHUNK_MAT_NAME);
    WriteInt32(aStream, 6 + matName.size() + 1);
    aStream.write(matName.c_str(), matName.size() + 1);
    WriteInt16(aStream, CHUNK_MAT_TEXMAP);
    WriteInt32(aStream, 12 + aMesh.mTexFileName.size() + 1);
    WriteInt16(aStream, CHUNK_MAT_MAPNAME);
    WriteInt32(aStream, 6 + aMesh.mTexFileName.size() + 1);
    aStream.write(aMesh.mTexFileName.c_str(), aMesh.mTexFileName.size() + 1);
  }

  // Object chunk
  WriteInt16(aStream, CHUNK_OBJECT);
  WriteInt32(aStream, 6 + objName.size() + 1 + triMeshSize);
  aStream.write(objName.c_str(), objName.size() + 1);

  // Triangle Mesh chunk
  WriteInt16(aStream, CHUNK_TRIMESH);
  WriteInt32(aStream, triMeshSize);

  // Vertex List chunk
  WriteInt16(aStream, CHUNK_VERTEXLIST);
  WriteInt32(aStream, 8 + 12 * vertCount);
  WriteInt16(aStream, vertCount);
  for(uint32 i = 0; i < vertCount; ++ i)
    WriteVector3(aStream, aMesh.mVertices[i]);

  // Mapping Coordinates chunk
  if(hasUVCoors)
  {
    WriteInt16(aStream, CHUNK_MAPPINGCOORDS);
    WriteInt32(aStream, 8 + 8 * vertCount);
    WriteInt16(aStream, vertCount);
    for(uint32 i = 0; i < vertCount; ++ i)
      WriteVector2(aStream, aMesh.mTexCoords[i]);
  }

  // Faces chunk
  WriteInt16(aStream, CHUNK_FACES);
  WriteInt32(aStream, 8 + 8 * triCount);
  WriteInt16(aStream, triCount);
  for(uint32 i = 0; i < triCount; ++ i)
  {
    WriteInt16(aStream, uint16(aMesh.mIndices[i * 3]));
    WriteInt16(aStream, uint16(aMesh.mIndices[i * 3 + 1]));
    WriteInt16(aStream, uint16(aMesh.mIndices[i * 3 + 2]));
    WriteInt16(aStream, 0);
  }

  // Material Group chunk
  if(matGroupSize > 0)
  {
    WriteInt16(aStream, CHUNK_MSH_MAT_GROUP);
    WriteInt32(aStream, matGroupSize);
    aStream.write(matName.c_str(), matName.size() + 1);
    WriteInt16(aStream, triCount);
    for(uint16 i = 0; i < triCount; ++ i)
      WriteInt16(aStream, i);
  }
}
