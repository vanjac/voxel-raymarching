#include "voxloader.h"

#include <QDebug>
#include <glm/glm.hpp>

// https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt
// https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox-extension.txt

VoxLoader::VoxLoader(QString filename)
    : file(filename)
{
    file.open(QIODevice::ReadOnly);
}

bool VoxLoader::load()
{
    char fourcc[5]{0,0,0,0,0};
    file.read(fourcc, 4);

    if (strncmp(fourcc, "VOX ", 4) != 0) {
        qDebug() << "Bad magic!";
        return false;
    }

    int32_t version;
    file.read((char *)&version, 4);
    qDebug() << "Version:" << version;

    return readChunk();
}


bool VoxLoader::readChunk()
{
    char id[5]{0,0,0,0,0};
    int32_t contentBytes, childBytes;
    file.read(id, 4);
    file.read((char *)&contentBytes, 4);
    file.read((char *)&childBytes, 4);
    //qDebug() << "Chunk" << id;
    int startData = file.pos();
    int endData = startData + contentBytes + childBytes;

    if (strncmp(id, "SIZE", 4) == 0 && !readSIZE())
        return false;
    if (strncmp(id, "XYZI", 4) == 0 && !readXYZI())
        return false;
    if (strncmp(id, "RGBA", 4) == 0 && !readRGBA())
        return false;

    file.seek(startData + contentBytes);
    while (file.pos() < endData) {
        if (!readChunk())
            return false;
    }
    //qDebug() << "End" << id;
    file.seek(endData);
    return true;
}


bool VoxLoader::readSIZE() {
    int32_t xDim, yDim, zDim;
    file.read((char *)&xDim, 4);
    file.read((char *)&yDim, 4);
    file.read((char *)&zDim, 4);
    qDebug() << "Size:" << xDim << yDim << zDim;
    pack.models.emplace_back(xDim, yDim, zDim);
    return true;
}

bool VoxLoader::readXYZI() {
    int32_t numVoxels;
    file.read((char *)&numVoxels, 4);
    qDebug() << "Num voxels:" << numVoxels;
    VoxModel &model = pack.models.back();
    for (int i = 0; i < numVoxels; i++) {
        uint8_t x, y, z, index;
        file.read((char *)&x, 1);
        file.read((char *)&y, 1);
        file.read((char *)&z, 1);
        file.read((char *)&index, 1);
        if (x < 0 || x >= model.xDim
                || y < 0 || y >= model.yDim
                || z < 0 || z >= model.zDim) {
            qDebug() << "Bad voxel position!" << x << y << z;
            continue;
        }
        model.data[x + y * model.xDim + z * model.xDim * model.yDim] = index;
    }
    return true;
}

bool VoxLoader::readRGBA() {
    qDebug() << "Read palette";
    uint8_t pal[PALETTE_SIZE];
    file.read((char *)pal, PALETTE_SIZE);
    for (int i = 0; i < 4; i++)
        pack.palette[i] = 0;  // first color is air
    // convert to linear color space
    for (int i = 0; i < PALETTE_SIZE - 4; i++) {
        pack.palette[i + 4] = glm::pow(pal[i] / 256.0, 2.2);
    }
    return true;
}
