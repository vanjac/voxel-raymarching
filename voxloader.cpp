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
        qWarning() << "Bad magic!";
        return false;
    }

    int32_t version;
    file.read((char *)&version, 4);
    qDebug() << "Version:" << version;

    if (!readChunk())
        return false;

    for (auto &t : transforms) {
        if (!shapeNodeModels.count(t.child))
            continue;
        int modelID = shapeNodeModels[t.child];
        if (modelID < 0 || modelID >= pack.models.size()) {
            qWarning() << "Invalid model ID" << modelID;
            continue;
        }

        if (t.name.empty())
            continue;
        int order;
        try {
            order = std::stoi(t.name);
        }  catch (const std::logic_error &e) {
            qWarning() << "Invalid transform name" << QString::fromStdString(t.name);
            continue;
        }

        if (order + 1 > pack.orderedModels.size())
            pack.orderedModels.resize(order + 1);
        pack.orderedModels[order] = &pack.models[modelID];
        qDebug() << "Order" << order << "-> model" << modelID;
    }
    return true;
}


bool VoxLoader::readChunk()
{
    char id[5]{0,0,0,0,0};
    int32_t contentBytes, childBytes;
    file.read(id, 4);
    file.read((char *)&contentBytes, 4);
    file.read((char *)&childBytes, 4);
    int startData = file.pos();
    int endData = startData + contentBytes + childBytes;

    if (strncmp(id, "SIZE", 4) == 0 && !readSIZE())
        return false;
    if (strncmp(id, "XYZI", 4) == 0 && !readXYZI())
        return false;
    if (strncmp(id, "RGBA", 4) == 0 && !readRGBA())
        return false;
    if (strncmp(id, "nTRN", 4) == 0 && !readnTRN())
        return false;
    if (strncmp(id, "nSHP", 4) == 0 && !readnSHP())
        return false;

    file.seek(startData + contentBytes);
    while (file.pos() < endData) {
        if (!readChunk())
            return false;
    }
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
            qWarning() << "Bad voxel position!" << x << y << z;
            return false;
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

bool VoxLoader::readnTRN() {
    int32_t nodeID, childID;
    std::string name;
    file.read((char *)&nodeID, 4);
    auto nodeAttr = readDICT();
    if (nodeAttr.count("_name")) {
        name = nodeAttr["_name"];
    }
    file.read((char *)&childID, 4);
    transforms.emplace_back(name, nodeID, childID);
    // ignore the rest
    return true;
}

bool VoxLoader::readnSHP() {
    int32_t nodeID, numModels, modelID;
    file.read((char *)&nodeID, 4);
    readDICT();
    file.read((char *)&numModels, 4);
    if (numModels != 1) {
        qWarning() << "numModels isn't 1!";
        return false;
    }
    file.read((char *)&modelID, 4);
    shapeNodeModels[nodeID] = modelID;
    // ignore the rest
    return true;
}

std::unordered_map<std::string, std::string> VoxLoader::readDICT() {
    std::unordered_map<std::string, std::string> map;
    int32_t numKeys;
    file.read((char *)&numKeys, 4);
    for (int i = 0; i < numKeys; i++) {
        auto key = readSTRING();
        auto value = readSTRING();
        map[key] = value;
    }
    return map;
}

std::string VoxLoader::readSTRING() {
    int32_t stringSize;
    file.read((char *)&stringSize, 4);
    char *buffer = new char[stringSize];
    file.read(buffer, stringSize);
    auto s = std::string(buffer, stringSize);
    delete[] buffer;
    return s;
}
