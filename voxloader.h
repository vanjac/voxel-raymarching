#ifndef VOXLOADER_H
#define VOXLOADER_H

#include <QFile>
#include <vector>
#include <string>
#include <unordered_map>
#include "util.h"

static const int PALETTE_ENTRIES = 256;
static const int PALETTE_SIZE = PALETTE_ENTRIES * 4;

struct VoxModel : noncopyable
{
    VoxModel(int xDim, int yDim, int zDim)
        : xDim(xDim), yDim(yDim), zDim(zDim)
        , data(xDim * yDim * zDim) { }

    int xDim, yDim, zDim;
    std::vector<char> data;
};

struct VoxPack : noncopyable
{
    std::vector<VoxModel> models;
    std::vector<VoxModel *> orderedModels;
    float palette[PALETTE_SIZE];
};

struct VoxTransform : noncopyable
{
    VoxTransform(std::string name, int id, int child)
        : name(name), id(id), child(child) { }
    std::string name;
    int id, child;
};

class VoxLoader : noncopyable
{
public:
    VoxLoader(QString filename);

    bool load();

    VoxPack pack;

private:
    bool readChunk();
    // chunk types
    bool readSIZE();
    bool readXYZI();
    bool readRGBA();
    bool readnTRN();
    bool readnSHP();
    // data types
    std::unordered_map<std::string, std::string> readDICT();
    std::string readSTRING();

    QFile file;

    std::vector<VoxTransform> transforms;
    // maps shape node ID to model ID
    std::unordered_map<int, int> shapeNodeModels;
};

#endif // VOXLOADER_H
