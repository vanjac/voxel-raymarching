#ifndef VOXLOADER_H
#define VOXLOADER_H

#include <QFile>
#include <vector>

static const int PALETTE_SIZE = 256 * 4;

struct VoxModel
{
    VoxModel(int xDim, int yDim, int zDim)
        : xDim(xDim), yDim(yDim), zDim(zDim)
        , data(new unsigned char [xDim * yDim * zDim]{ 0 }) { }
    ~VoxModel() { delete[] data; }

    int numVoxels() const { return xDim * yDim * zDim; }

    int xDim, yDim, zDim;
    unsigned char *data;
};

struct VoxPack
{
    VoxPack()
        : palette(new float[PALETTE_SIZE]) { }
    ~VoxPack() { delete[] palette; }
    std::vector<VoxModel> models;
    float *palette;
};

class VoxLoader
{
public:
    VoxLoader(QString filename);

    bool load();

    VoxPack pack;

private:
    bool readChunk();
    bool readSIZE();
    bool readXYZI();
    bool readRGBA();

    QFile file;
};

#endif // VOXLOADER_H
