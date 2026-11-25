#include "game/world/generation/WallGenerator.hpp"

#include <cmath>
#include <algorithm>
#include <vector>

#include "game/util/AABB.hpp"
#include "game/world/Level.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/Blocks.hpp"
#include "game/world/block/EnumProperty.hpp"
#include "game/world/block/PlanksBlock.hpp"
#include "game/world/block/TypedBoxed.hpp"
#include "game/world/block/BlockTransaction.hpp"
#include "game/core/GenerationWriteTask.hpp"

WallGenerator::Wall::Wall(AABB* area, bool internal, bool boundary)
    : mArea(new AABB(*area)),
      mInternal(internal),
      mBoundary(boundary)
{
}

WallGenerator::Wall::~Wall()
{
    delete mArea;
}

WallGenerator::WallGenerator(Random random, std::vector<AABB*>* areas, std::vector<AABB*>* walls) : MapGenerator(random)
{
    mBounds = NULL;
    if (areas != NULL)
    {
        for (std::vector<AABB*>::iterator area = areas->begin(); area != areas->end(); ++area)
        {
            if (*area == NULL)
                continue;
            mAreas.push_back(*area);
            if (mBounds == NULL)
                mBounds = new AABB(**area);
            else
                mBounds->set(mBounds->minmax(*area));
        }
    }
    if (walls != NULL)
    {
        for (std::vector<AABB*>::iterator wall = walls->begin(); wall != walls->end(); ++wall)
        {
            if (*wall == NULL)
                continue;
            if (mBounds == NULL)
                mBounds = new AABB(**wall);
            else
                mBounds->set(mBounds->minmax(*wall));
            mWalls.push_back(new Wall(*wall, true, false));
        }
    }
    if (mBounds != NULL)
    {
        mBounds->set(mBounds->grow(2.0, 0.0, 2.0));
        if (mWalls.empty())
            mBounds->mMaxY = std::max(mBounds->mMaxY, mBounds->mMinY + 16.0);
    }
}

WallGenerator::~WallGenerator()
{
    delete mBounds;
    std::vector<Wall*>::iterator wall = mWalls.begin();
    while (wall != mWalls.end())
    {
        delete *wall;
        ++wall;
    }
}

void WallGenerator::GetInternalWalls(std::vector<AABB*>* walls)
{
    std::vector<Wall*>::iterator wall = mWalls.begin();
    while (wall != mWalls.end())
    {
        if ((*wall)->mInternal)
        {
            walls->push_back((*wall)->mArea);
        }
        ++wall;
    }
}

bool WallGenerator::IsPointInternal(int x, int z, int size)
{
    return x > 0 && x < size && z > 0 && z < size;
}

void WallGenerator::StartGeneration(Level* level)
{
    if (level == NULL || mBounds == NULL || mAreas.empty())
        return;
    const AABB grownBounds(*mBounds->grow(2.0, 1.0, 2.0));
    const AABB* grown = &grownBounds;
    BlockTransaction* transaction = BlockTransaction::CreateTransaction(
        BlockPos(grown->mMinX, grown->mMinY, grown->mMinZ), BlockPos(grown->mMaxX, grown->mMaxY, grown->mMaxZ));

    if (mWalls.empty())
    {
        // Build walls from plot boundaries instead of assuming a square 2x2 arena.
        // Shared walls are marked internal so judging can remove them.
        for (std::vector<AABB*>::const_iterator it = mAreas.begin(); it != mAreas.end(); ++it)
        {
            const AABB& plot = **it;
            AABB sides[] = {
                AABB(plot.mMinX - 1, plot.mMinY, plot.mMinZ - 1,
                     plot.mMinX, plot.mMaxY, plot.mMaxZ + 1),
                AABB(plot.mMaxX, plot.mMinY, plot.mMinZ - 1,
                     plot.mMaxX + 1, plot.mMaxY, plot.mMaxZ + 1),
                AABB(plot.mMinX - 1, plot.mMinY, plot.mMinZ - 1,
                     plot.mMaxX + 1, plot.mMaxY, plot.mMinZ),
                AABB(plot.mMinX - 1, plot.mMinY, plot.mMaxZ,
                     plot.mMaxX + 1, plot.mMaxY, plot.mMaxZ + 1)
            };
            for (unsigned int side = 0; side < 4; ++side)
            {
                bool internal = false;
                for (std::vector<AABB*>::const_iterator other = mAreas.begin(); other != mAreas.end(); ++other)
                {
                    if (*other == *it)
                        continue;
                    const AABB& next = **other;
                    const bool overlapX = plot.mMinX < next.mMaxX && plot.mMaxX > next.mMinX;
                    const bool overlapZ = plot.mMinZ < next.mMaxZ && plot.mMaxZ > next.mMinZ;
                    if ((side == 0 && overlapZ && next.mMaxX == plot.mMinX - 1) ||
                        (side == 1 && overlapZ && next.mMinX == plot.mMaxX + 1) ||
                        (side == 2 && overlapX && next.mMaxZ == plot.mMinZ - 1) ||
                        (side == 3 && overlapX && next.mMinZ == plot.mMaxZ + 1))
                    {
                        internal = true;
                        break;
                    }
                }
                bool duplicate = false;
                for (std::vector<Wall*>::const_iterator wall = mWalls.begin(); wall != mWalls.end(); ++wall)
                {
                    if ((*wall)->mArea->equals(&sides[side]))
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    mWalls.push_back(new Wall(&sides[side], internal, false));
            }
        }
    }

    // Clear builds on every round, even when explicit walls were supplied.
    // structure_void is the transaction's explicit air marker.
    const BlockState* voidState = Blocks::structure_void->defaultBlockState();
    const BlockState* floor = Blocks::planks->defaultBlockState()->setValue(
        PlanksBlock::VARIANT, PlanksBlock::Variant::SPRUCE);
    for (std::vector<AABB*>::iterator area = mAreas.begin(); area != mAreas.end(); ++area)
    {
        const AABB* bounds = *area;
        SetArea(level, transaction, *area, voidState);
        AABB floorArea(bounds->mMinX, bounds->mMinY - 1.0, bounds->mMinZ,
                       bounds->mMaxX, bounds->mMinY, bounds->mMaxZ);
        SetArea(level, transaction, &floorArea, floor);
    }

    for (std::vector<Wall*>::iterator wall = mWalls.begin(); wall != mWalls.end(); ++wall)
    {
        const BlockState* state = ((*wall)->mBoundary || (*wall)->mInternal) ? Blocks::planks->defaultBlockState()
                                                                             : Blocks::stonebrick->defaultBlockState();
        SetArea(level, transaction, (*wall)->mArea, state);
    }

    BlockPos origin(static_cast<int>(grown->mMinX), static_cast<int>(grown->mMinY), static_cast<int>(grown->mMinZ));
    mGenerationWriteTasks.push_back(new GenerationWriteTask(level, transaction, origin, NULL));
}

AABB* WallGenerator::GetSpawnArea() const
{
    return mAreas.empty() ? NULL : mAreas.front();
}

void WallGenerator::CleanUp(Level*)
{
}

AABB* WallGenerator::GetLayerBB(unsigned int)
{
    return 0;
}

int WallGenerator::GetNumberOfLayers()
{
    return 0;
}

void WallGenerator::GetLayerExtents(unsigned int, float*, float*)
{
}

void WallGenerator::SetArea(Level*, BlockTransaction* transaction, AABB* area, const BlockState* state)
{
    if (transaction == NULL || area == NULL || state == NULL || mBounds == NULL)
        return;

    const AABB* grown = mBounds->grow(2.0, 1.0, 2.0);
    const Vec3i origin(static_cast<int>(grown->mMinX), static_cast<int>(grown->mMinY),
                       static_cast<int>(grown->mMinZ));
    for (int x = static_cast<int>(area->mMinX); x < area->mMaxX; ++x)
        for (int y = static_cast<int>(area->mMinY); y < area->mMaxY; ++y)
            for (int z = static_cast<int>(area->mMinZ); z < area->mMaxZ; ++z)
            {
                const Vec3i position(x - origin.x, y - origin.y, z - origin.z);
                if (transaction->inBounds(position))
                    transaction->setBlock(position, state);
            }
}
