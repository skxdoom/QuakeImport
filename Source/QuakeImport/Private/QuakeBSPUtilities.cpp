// Fill out your copyright notice in the Description page of Project Settings.

// QuakeImport
#include "QuakeBSPUtilities.h"
#include "QuakeImportCommon.h"

// EPIC
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "Editor/EditorEngine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/CollisionProfile.h"
#include "RawMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace bsputils
{
    BspLoader::BspLoader() :
        m_bsp29(nullptr)
    {
        /* do nothing */
    }

    BspLoader::~BspLoader()
    {
        delete m_bsp29;
    }

    void BspLoader::Load(const uint8* data, int64 dataSize)
    {
        m_dataStart = data;
        m_dataSize = dataSize;

        if (!m_dataStart || m_dataSize < 4)
        {
            return;
        }

        const char* Magic = reinterpret_cast<const char*>(m_dataStart);
        const bool bIsBsp2 =
            (m_dataSize >= int64(sizeof(bspformat2::HeaderNoVersion))) &&
            (FMemory::Memcmp(Magic, bspformat2::HEADER_IDENT_BSP2, 4) == 0 || FMemory::Memcmp(Magic, bspformat2::HEADER_IDENT_2PSB, 4) == 0);

        m_bsp29 = new bspformat29::Bsp_29();

        auto ValidateLumps = [this](const bspformat29::Lump* Lumps, int32 Count)
        {
            for (int32 LumpIndex = 0; LumpIndex < Count; LumpIndex++)
            {
                const bspformat29::Lump& L = Lumps[LumpIndex];
                const int64 Pos = int64(L.position);
                const int64 Len = int64(L.length);
                if (Pos < 0 || Len < 0 || Pos + Len > m_dataSize)
                {
                    UE_LOG(LogTemp, Warning, TEXT("BSP Import: Lump %d out of bounds (pos=%d len=%d size=%lld)"), LumpIndex, L.position, L.length, m_dataSize);
                    return false;
                }
            }
            return true;
        };

        auto DeserializeEdges29 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat29::FileEdge> Tmp;
            if (!DeserializeLump<bspformat29::FileEdge>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->edges.Reset(Tmp.Num());
            m_bsp29->edges.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                m_bsp29->edges[i].first = int32(Tmp[i].first);
                m_bsp29->edges[i].second = int32(Tmp[i].second);
            }
            return true;
        };

        auto DeserializeMarks29 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat29::FileMarksurface> Tmp;
            if (!DeserializeLump<bspformat29::FileMarksurface>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->marksurfaces.Reset(Tmp.Num());
            m_bsp29->marksurfaces.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                m_bsp29->marksurfaces[i].index = int32(Tmp[i].index);
            }
            return true;
        };

        auto DeserializeFaces29 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat29::FileFace> Tmp;
            if (!DeserializeLump<bspformat29::FileFace>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->faces.Reset(Tmp.Num());
            m_bsp29->faces.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::Face& Dst = m_bsp29->faces[i];
                const bspformat29::FileFace& Src = Tmp[i];

                Dst.planenum = int32(Src.planenum);
                Dst.side = int32(Src.side);
                Dst.firstedge = Src.firstedge;
                Dst.numedges = int32(Src.numedges);
                Dst.texinfo = int32(Src.texinfo);
                FMemory::Memcpy(Dst.styles, Src.styles, bspformat29::MAXLIGHTMAPS);
                Dst.lightofs = Src.lightofs;
            }
            return true;
        };

        auto DeserializeLeaves29 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat29::FileLeaf> Tmp;
            if (!DeserializeLump<bspformat29::FileLeaf>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->leaves.Reset(Tmp.Num());
            m_bsp29->leaves.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::Leaf& Dst = m_bsp29->leaves[i];
                const bspformat29::FileLeaf& Src = Tmp[i];

                Dst.contents = Src.contents;
                Dst.visofs = Src.visofs;
                for (int32 a = 0; a < 3; a++)
                {
                    Dst.mins[a] = int32(Src.mins[a]);
                    Dst.maxs[a] = int32(Src.maxs[a]);
                }
                Dst.firstmarksurface = int32(Src.firstmarksurface);
                Dst.nummarksurfaces = int32(Src.nummarksurfaces);
                FMemory::Memcpy(Dst.ambient_level, Src.ambient_level, 4);
            }
            return true;
        };

        auto DeserializeNodes29 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat29::FileNode> Tmp;
            if (!DeserializeLump<bspformat29::FileNode>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->nodes.Reset(Tmp.Num());
            m_bsp29->nodes.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::Node& Dst = m_bsp29->nodes[i];
                const bspformat29::FileNode& Src = Tmp[i];

                Dst.planenum = Src.planenum;
                Dst.children[0] = int32(Src.children[0]);
                Dst.children[1] = int32(Src.children[1]);
                for (int32 a = 0; a < 3; a++)
                {
                    Dst.mins[a] = int32(Src.mins[a]);
                    Dst.maxs[a] = int32(Src.maxs[a]);
                }
                Dst.firstface = int32(Src.firstface);
                Dst.numfaces = int32(Src.numfaces);
            }
            return true;
        };

        auto DeserializeEdges2 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat2::FileEdge> Tmp;
            if (!DeserializeLump<bspformat2::FileEdge>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->edges.Reset(Tmp.Num());
            m_bsp29->edges.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                m_bsp29->edges[i].first = Tmp[i].first;
                m_bsp29->edges[i].second = Tmp[i].second;
            }
            return true;
        };

        auto DeserializeMarks2 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat2::FileMarksurface> Tmp;
            if (!DeserializeLump<bspformat2::FileMarksurface>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->marksurfaces.Reset(Tmp.Num());
            m_bsp29->marksurfaces.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                m_bsp29->marksurfaces[i].index = Tmp[i].index;
            }
            return true;
        };

        auto DeserializeFaces2 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat2::FileFace> Tmp;
            if (!DeserializeLump<bspformat2::FileFace>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->faces.Reset(Tmp.Num());
            m_bsp29->faces.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::Face& Dst = m_bsp29->faces[i];
                const bspformat2::FileFace& Src = Tmp[i];

                Dst.planenum = Src.planenum;
                Dst.side = Src.side;
                Dst.firstedge = Src.firstedge;
                Dst.numedges = Src.numedges;
                Dst.texinfo = Src.texinfo;
                FMemory::Memcpy(Dst.styles, Src.styles, bspformat29::MAXLIGHTMAPS);
                Dst.lightofs = Src.lightofs;
            }
            return true;
        };

        auto DeserializeLeaves2 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat2::FileLeaf> Tmp;
            if (!DeserializeLump<bspformat2::FileLeaf>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->leaves.Reset(Tmp.Num());
            m_bsp29->leaves.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::Leaf& Dst = m_bsp29->leaves[i];
                const bspformat2::FileLeaf& Src = Tmp[i];

                Dst.contents = Src.contents;
                Dst.visofs = Src.visofs;
                for (int32 a = 0; a < 3; a++)
                {
                    Dst.mins[a] = int32(Src.mins[a]);
                    Dst.maxs[a] = int32(Src.maxs[a]);
                }
                Dst.firstmarksurface = Src.firstmarksurface;
                Dst.nummarksurfaces = Src.nummarksurfaces;
                FMemory::Memcpy(Dst.ambient_level, Src.ambient_level, 4);
            }
            return true;
        };

        auto DeserializeNodes2 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat2::FileNode> Tmp;
            if (!DeserializeLump<bspformat2::FileNode>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->nodes.Reset(Tmp.Num());
            m_bsp29->nodes.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::Node& Dst = m_bsp29->nodes[i];
                const bspformat2::FileNode& Src = Tmp[i];

                Dst.planenum = Src.planenum;
                Dst.children[0] = Src.children[0];
                Dst.children[1] = Src.children[1];
                for (int32 a = 0; a < 3; a++)
                {
                    Dst.mins[a] = int32(Src.mins[a]);
                    Dst.maxs[a] = int32(Src.maxs[a]);
                }
                Dst.firstface = Src.firstface;
                Dst.numfaces = Src.numfaces;
            }
            return true;
        };

        auto DeserializeModels2 = [this](const bspformat29::Lump& Lump)
        {
            TArray<bspformat2::FileModel> Tmp;
            if (!DeserializeLump<bspformat2::FileModel>(m_dataStart, Lump, Tmp))
            {
                return false;
            }

            m_bsp29->submodels.Reset(Tmp.Num());
            m_bsp29->submodels.SetNumUninitialized(Tmp.Num());
            for (int32 i = 0; i < Tmp.Num(); i++)
            {
                bspformat29::SubModel& Dst = m_bsp29->submodels[i];
                const bspformat2::FileModel& Src = Tmp[i];

                FMemory::Memcpy(Dst.mins, Src.mins, sizeof(Dst.mins));
                FMemory::Memcpy(Dst.maxs, Src.maxs, sizeof(Dst.maxs));
                FMemory::Memcpy(Dst.origin, Src.origin, sizeof(Dst.origin));
                for (int32 h = 0; h < 4; h++)
                {
                    Dst.headnode[h] = int32(Src.headnode[h]);
                }
                Dst.visleafs = int32(Src.visleafs);
                Dst.firstface = int32(Src.firstface);
                Dst.numfaces = int32(Src.numfaces);
            }
            return true;
        };

        bspformat29::Lump Lumps[bspformat29::HEADER_LUMP_SIZE];
        if (bIsBsp2)
        {
            auto ReadI32 = [](const uint8* Ptr, bool bSwap)
            {
                int32 V = 0;
                FMemory::Memcpy(&V, Ptr, sizeof(int32));
                if (bSwap)
                {
                    V = int32(BYTESWAP_ORDER32(uint32(V)));
                }
                return V;
            };

            auto TryHeader = [this, &ReadI32](const bspformat29::Lump* InLumps, bool bSwap, bspformat29::Lump* OutLumps)
            {
                if (!InLumps || !OutLumps)
                {
                    return false;
                }

                bspformat29::Lump Tmp[bspformat29::HEADER_LUMP_SIZE];
                for (int32 i = 0; i < bspformat29::HEADER_LUMP_SIZE; i++)
                {
                    Tmp[i].position = bSwap ? int32(BYTESWAP_ORDER32(uint32(InLumps[i].position))) : InLumps[i].position;
                    Tmp[i].length = bSwap ? int32(BYTESWAP_ORDER32(uint32(InLumps[i].length))) : InLumps[i].length;
                }

                /*if (!ValidateLumps(Tmp, bspformat29::HEADER_LUMP_SIZE))
                {
                    return false;
                }*/

                // Extra sanity: texture lump should contain a reasonable count.
                const bspformat29::Lump& TexLump = Tmp[bspformat29::LUMP_TEXTURES];
                const int64 Pos = int64(TexLump.position);
                const int64 Len = int64(TexLump.length);
                if (Pos < 0 || Len < int64(sizeof(int32)) || Pos + int64(sizeof(int32)) > m_dataSize)
                {
                    return false;
                }

                const int32 NumTex = ReadI32(m_dataStart + Pos, bSwap);
                if (NumTex < 0 || NumTex > 131072)
                {
                    return false;
                }

                // Make sure the offsets table fits.
                const int64 OffsTableEnd = Pos + int64(sizeof(int32)) + int64(NumTex) * int64(sizeof(int32));
                if (NumTex > 0 && OffsTableEnd > m_dataSize)
                {
                    return false;
                }

                // Require some essential lumps to be non-empty.
                const auto& Verts = Tmp[bspformat29::LUMP_VERTEXES];
                const auto& Edges = Tmp[bspformat29::LUMP_EDGES];
                const auto& Faces = Tmp[bspformat29::LUMP_FACES];
                const auto& Texinfo = Tmp[bspformat29::LUMP_TEXINFO];
                const auto& Models = Tmp[bspformat29::LUMP_MODELS];
                if (Verts.length <= 0 || Edges.length <= 0 || Faces.length <= 0 || Texinfo.length <= 0 || Models.length <= 0)
                {
                    return false;
                }

                FMemory::Memcpy(OutLumps, Tmp, sizeof(Lumps));
                return true;
            };

            // First try the common (ident + version + lumps) layout.
            bool bGotValid = false;
            const bool bSwap = false;// (BspMagic[0] == '2' && BspMagic[1] == 'P' && BspMagic[2] == 'S' && BspMagic[3] == 'B');
            if (m_dataSize >= int64(sizeof(bspformat2::Header)))
            {
                bspformat2::Header H;
                FMemory::Memcpy(&H, m_dataStart, sizeof(H));
                bGotValid = TryHeader(H.lumps, bSwap, Lumps);
            }

            // Fallback: ident + lumps (no version).
            if (!bGotValid)
            {
                bspformat2::HeaderNoVersion Hnv;
                FMemory::Memcpy(&Hnv, m_dataStart, sizeof(Hnv));
                bGotValid = TryHeader(Hnv.lumps, bSwap, Lumps);
            }

            if (!bGotValid)
            {
                UE_LOG(LogTemp, Warning, TEXT("BSP Import: Failed to parse BSP2 header (no valid lump directory found)"));
                return;
            }
        }
        else
        {
            if (m_dataSize < int64(sizeof(bspformat29::Header)))
            {
                return;
            }

            bspformat29::Header H;
            QuakeCommon::ReadData<bspformat29::Header>(m_dataStart, 0, H);
            if (H.version != bspformat29::HEADER_VERSION_29)
            {
                delete m_bsp29;
                m_bsp29 = nullptr;
                return;
            }

            FMemory::Memcpy(Lumps, H.lumps, sizeof(Lumps));
        }

        if (!bIsBsp2)
        {
            if (!ValidateLumps(Lumps, bspformat29::HEADER_LUMP_SIZE))
            {
                return;
            }
        }

        DeserializeLump<bspformat29::Point3f>(m_dataStart, Lumps[bspformat29::LUMP_VERTEXES], m_bsp29->vertices);
        DeserializeLump<bspformat29::Surfedge>(data, Lumps[bspformat29::LUMP_SURFEDGES], m_bsp29->surfedges);
        DeserializeLump<uint8>(data, Lumps[bspformat29::LUMP_LIGHTING], m_bsp29->lightdata);
        DeserializeLump<bspformat29::Plane>(data, Lumps[bspformat29::LUMP_PLANES], m_bsp29->planes);
        if (bIsBsp2)
        {
            DeserializeModels2(Lumps[bspformat29::LUMP_MODELS]);
        }
        else
        {
            DeserializeLump<bspformat29::SubModel>(data, Lumps[bspformat29::LUMP_MODELS], m_bsp29->submodels);
        }
        DeserializeLump<bspformat29::TexInfo>(data, Lumps[bspformat29::LUMP_TEXINFO], m_bsp29->texinfos);
        DeserializeLump<uint8>(data, Lumps[bspformat29::LUMP_VISIBILITY], m_bsp29->visdata);

        if (bIsBsp2)
        {
            DeserializeEdges2(Lumps[bspformat29::LUMP_EDGES]);
            DeserializeFaces2(Lumps[bspformat29::LUMP_FACES]);
            DeserializeMarks2(Lumps[bspformat29::LUMP_MARKSURFACES]);
            DeserializeLeaves2(Lumps[bspformat29::LUMP_LEAFS]);
            DeserializeNodes2(Lumps[bspformat29::LUMP_NODES]);
        }
        else
        {
            DeserializeEdges29(Lumps[bspformat29::LUMP_EDGES]);
            DeserializeFaces29(Lumps[bspformat29::LUMP_FACES]);
            DeserializeMarks29(Lumps[bspformat29::LUMP_MARKSURFACES]);
            DeserializeLeaves29(Lumps[bspformat29::LUMP_LEAFS]);
            DeserializeNodes29(Lumps[bspformat29::LUMP_NODES]);
        }

        LoadTextures(data, Lumps[bspformat29::LUMP_TEXTURES]);
        LoadEntities(data, Lumps[bspformat29::LUMP_ENTITIES]);
    }

    void BspLoader::LoadTextures(const uint8*& data, const bspformat29::Lump& lump)
    {
        const int64 LumpPos = int64(lump.position);
        const int64 LumpLen = int64(lump.length);

        if (LumpPos < 0 || LumpLen < int64(sizeof(int32)) || LumpPos + LumpLen > m_dataSize)
        {
            UE_LOG(LogTemp, Warning, TEXT("BSP Import: Texture lump out of bounds (pos=%d len=%d size=%lld)"), lump.position, lump.length, m_dataSize);
            return;
        }

        int32 NumTex = 0;
        int64 Cursor = LumpPos;
        Cursor += QuakeCommon::ReadData(data, int32(Cursor), NumTex);

        if (NumTex < 0 || NumTex > 16384)
        {
            UE_LOG(LogTemp, Warning, TEXT("BSP Import: Texture count invalid (%d)"), NumTex);
            return;
        }

        const int64 TableBytes = int64(NumTex) * int64(sizeof(int32));
        if (Cursor + TableBytes > LumpPos + LumpLen)
        {
            UE_LOG(LogTemp, Warning, TEXT("BSP Import: Texture offset table out of bounds"));
            return;
        }

        m_bsp29->textures.Reset(NumTex);

        for (int32 i = 0; i < NumTex; i++)
        {
            int32 Offset = 0;
            Cursor += QuakeCommon::ReadData(data, int32(Cursor), Offset);

            bspformat29::Texture Tex;
            Tex.width = 0;
            Tex.height = 0;

            // Quake BSP allows external textures. Offset -1 means the texture isn't embedded.
            if (Offset <= 0)
            {
                Tex.name = FString::Printf(TEXT("missing_%d"), i);
                m_bsp29->textures.Add(MoveTemp(Tex));
                continue;
            }

            const int64 MiptexStart = LumpPos + int64(Offset);
            const int64 MinMiptexSize = int64(sizeof(bspformat29::Miptex));
            if (MiptexStart < LumpPos || MiptexStart + MinMiptexSize > LumpPos + LumpLen)
            {
                Tex.name = FString::Printf(TEXT("missing_%d"), i);
                m_bsp29->textures.Add(MoveTemp(Tex));
                continue;
            }

            const bspformat29::Miptex* Mt = reinterpret_cast<const bspformat29::Miptex*>(data + MiptexStart);

            char NameBuf[17];
            FMemory::Memcpy(NameBuf, Mt->name, 16);
            NameBuf[16] = 0;
            Tex.name = ANSI_TO_TCHAR(NameBuf);

            const uint32 W = Mt->width;
            const uint32 H = Mt->height;
            if (W == 0 || H == 0 || W > 8192 || H > 8192)
            {
                UE_LOG(LogTemp, Warning, TEXT("BSP Import: Invalid texture size %s (%u x %u)"), *Tex.name, W, H);
                Tex.name = FString::Printf(TEXT("missing_%d"), i);
                m_bsp29->textures.Add(MoveTemp(Tex));
                continue;
            }

            const int64 Bytes64 = int64(W) * int64(H);
            if (Bytes64 <= 0 || Bytes64 > int64(512) * 1024 * 1024)
            {
                UE_LOG(LogTemp, Warning, TEXT("BSP Import: Texture byte size invalid %s (%lld)"), *Tex.name, Bytes64);
                Tex.name = FString::Printf(TEXT("missing_%d"), i);
                m_bsp29->textures.Add(MoveTemp(Tex));
                continue;
            }

            const int64 Mip0Rel = int64(Mt->offsets[0]);
            const int64 Mip0Abs = MiptexStart + Mip0Rel;
            if (Mip0Rel <= 0 || Mip0Abs < LumpPos || Mip0Abs + Bytes64 > LumpPos + LumpLen)
            {
                UE_LOG(LogTemp, Warning, TEXT("BSP Import: Mip0 out of bounds for %s"), *Tex.name);
                Tex.name = FString::Printf(TEXT("missing_%d"), i);
                m_bsp29->textures.Add(MoveTemp(Tex));
                continue;
            }

            Tex.width = W;
            Tex.height = H;
            Tex.mip0.SetNumUninitialized(int32(Bytes64));
            FMemory::Memcpy(Tex.mip0.GetData(), data + Mip0Abs, size_t(Bytes64));
            m_bsp29->textures.Add(MoveTemp(Tex));
        }
    }

    void BspLoader::LoadEntities(const uint8*& data, const bspformat29::Lump& lump)
    {
        const int64 Pos = int64(lump.position);
        const int64 Len = int64(lump.length);
        if (Pos < 0 || Len < 0 || Pos + Len > m_dataSize)
        {
            return;
        }

        TArray<char> Temp;
        Temp.SetNumUninitialized(int32(Len) + 1);
        FMemory::Memcpy(Temp.GetData(), data + Pos, size_t(Len));
        Temp[int32(Len)] = 0;
        m_bsp29->entities = ANSI_TO_TCHAR(Temp.GetData());
    }

    void AddWedgeEntry(FRawMesh& mesh, const uint32 index, const FVector3f normal, const FVector2f texcoord0, const FVector2f texcoord1)
    {
        mesh.WedgeIndices.Add(index);
        mesh.WedgeColors.Add(FColor(0));
        mesh.WedgeTangentZ.Add(normal);
        mesh.WedgeTexCoords[0].Add(texcoord0);
        mesh.WedgeTexCoords[1].Add(texcoord1);
    }

    struct FWorldChunkBuild
    {
        FRawMesh RawMesh;
        TMap<int32, int32> BspVertexToLocal;
        TArray<int32> SlotToTextureId;
        TMap<int32, int32> TextureIdToSlot;
    };

    static FIntVector GetChunkKey3D(const FVector3f& Center, int32 ChunkSize)
    {
        if (ChunkSize <= 0)
        {
            return FIntVector(0, 0, 0);
        }

        const int32 X = FMath::FloorToInt(float(Center.X) / float(ChunkSize));
        const int32 Y = FMath::FloorToInt(float(Center.Y) / float(ChunkSize));
        const int32 Z = FMath::FloorToInt(float(Center.Z) / float(ChunkSize));
        return FIntVector(X, Y, Z);
    }

    static int32 GetOrAddMaterialSlot(FWorldChunkBuild& Chunk, int32 TextureId)
    {
        if (const int32* Found = Chunk.TextureIdToSlot.Find(TextureId))
        {
            return *Found;
        }

        const int32 NewSlot = Chunk.SlotToTextureId.Num();
        Chunk.SlotToTextureId.Add(TextureId);
        Chunk.TextureIdToSlot.Add(TextureId, NewSlot);
        return NewSlot;
    }

    static uint32 GetOrAddLocalVertex(FWorldChunkBuild& Chunk, const bspformat29::Bsp_29& Model, int32 BspVertexIndex, float ImportScale)
    {
        if (const int32* Found = Chunk.BspVertexToLocal.Find(BspVertexIndex))
        {
            return uint32(*Found);
        }

        const int32 NewIndex = Chunk.RawMesh.VertexPositions.Num();
        Chunk.BspVertexToLocal.Add(BspVertexIndex, NewIndex);

        const bspformat29::Point3f& V = Model.vertices[BspVertexIndex];
        const FVector3f P(-V.x, V.y, V.z);
        Chunk.RawMesh.VertexPositions.Add(P * ImportScale);

        return uint32(NewIndex);
    }

    
    static UPackage* CreateAssetPackage(const FString& LongPackageName)
    {
        UPackage* Pkg = CreatePackage(*LongPackageName);
        Pkg->MarkPackageDirty();
        return Pkg;
    }
static void ResetStaticMeshForBuild(UStaticMesh* StaticMesh)
    {
        if (!StaticMesh)
        {
            return;
        }

        StaticMesh->Modify();
        StaticMesh->GetStaticMaterials().Reset();
        StaticMesh->SetNumSourceModels(0);
    }

    static UStaticMesh* GetOrCreateStaticMesh(UPackage& Package, const FString& Name)
    {
        UStaticMesh* Existing = FindObject<UStaticMesh>(&Package, *Name);
        if (Existing)
        {
            ResetStaticMeshForBuild(Existing);
            return Existing;
        }

        UStaticMesh* StaticMesh = NewObject<UStaticMesh>(&Package, FName(*Name), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(StaticMesh);
        return StaticMesh;
    }

    static FString SanitizeSurfaceNameForAsset(const FString& InName)
    {
        FString Out = InName;

        if (Out.StartsWith(TEXT("*")))
        {
            Out[0] = TCHAR('-');
        }

        for (int32 I = 0; I < Out.Len(); I++)
        {
            const TCHAR C = Out[I];
            if (!(FChar::IsAlnum(C) || C == TCHAR('_') || C == TCHAR('-')))
            {
                Out[I] = TCHAR('_');
            }
        }

        return Out;
    }

    static bool IsTransparentSurfaceName(const FString& TexName)
    {
        if (TexName.StartsWith(TEXT("*")))
        {
            return true;
        }

        return TexName.Equals(TEXT("trigger"), ESearchCase::IgnoreCase);
    }


struct FFaceLightmapCalc
{
    int32 FaceIndex = -1;
    int32 TexMinS = 0;
    int32 TexMinT = 0;
    int32 W = 0;
    int32 H = 0;
    int32 LightOfs = -1;
};

static void ComputeFaceLightmapDimensions(const bspformat29::Bsp_29& Model, int32 FaceIndex, int32& OutTexMinS, int32& OutTexMinT, int32& OutW, int32& OutH)
{
    OutTexMinS = 0;
    OutTexMinT = 0;
    OutW = 0;
    OutH = 0;

    if (!Model.faces.IsValidIndex(FaceIndex))
    {
        return;
    }

    const bspformat29::Face& Face = Model.faces[FaceIndex];
    if (!Model.texinfos.IsValidIndex(Face.texinfo))
    {
        return;
    }

    const bspformat29::TexInfo& Ti = Model.texinfos[Face.texinfo];

    float MinS = 0.0f;
    float MaxS = 0.0f;
    float MinT = 0.0f;
    float MaxT = 0.0f;
    bool bInit = false;

    for (int32 E = Face.numedges; E-- > 0;)
    {
        const bspformat29::Surfedge& Surfedge = Model.surfedges[Face.firstedge + E];
        const bspformat29::Edge& Edge = Model.edges[abs(Surfedge.index)];

        int32 VertexId = Edge.first;
        if (Surfedge.index < 0)
        {
            VertexId = Edge.second;
        }

        if (!Model.vertices.IsValidIndex(VertexId))
        {
            continue;
        }

        const bspformat29::Point3f& P = Model.vertices[VertexId];
        const FVector3f Unflipped(P.x, P.y, P.z);

        const float S = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[0][0], Ti.vecs[0][1], Ti.vecs[0][2])) + Ti.vecs[0][3];
        const float T = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[1][0], Ti.vecs[1][1], Ti.vecs[1][2])) + Ti.vecs[1][3];

        if (!bInit)
        {
            MinS = MaxS = S;
            MinT = MaxT = T;
            bInit = true;
        }
        else
        {
            MinS = FMath::Min(MinS, S);
            MaxS = FMath::Max(MaxS, S);
            MinT = FMath::Min(MinT, T);
            MaxT = FMath::Max(MaxT, T);
        }
    }

    if (!bInit)
    {
        return;
    }

    const int32 TexMinS = FMath::FloorToInt(MinS / 16.0f) * 16;
    const int32 TexMinT = FMath::FloorToInt(MinT / 16.0f) * 16;
    const int32 TexMaxS = FMath::CeilToInt(MaxS / 16.0f) * 16;
    const int32 TexMaxT = FMath::CeilToInt(MaxT / 16.0f) * 16;

    const int32 ExtS = FMath::Max(0, TexMaxS - TexMinS);
    const int32 ExtT = FMath::Max(0, TexMaxT - TexMinT);

    OutTexMinS = TexMinS;
    OutTexMinT = TexMinT;
    OutW = (ExtS / 16) + 1;
    OutH = (ExtT / 16) + 1;
}

bool BuildLightmapAtlas(const bspformat29::Bsp_29& Model, const FString& LightmapsPath, const FString& MapName, const FString& LitFilePath, bool bOverwrite, FLightmapAtlas& OutAtlas)
{
    OutAtlas = FLightmapAtlas();

    if (Model.lightdata.Num() == 0)
    {
        return false;
    }

    TArray<uint8> LitRgbData;
    bool bUseLit = false;
    if (!LitFilePath.IsEmpty())
    {
        FString LitAbs = LitFilePath;
        if (FPaths::IsRelative(LitAbs))
        {
            LitAbs = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), LitAbs);
        }

        TArray<uint8> LitFile;
        if (FFileHelper::LoadFileToArray(LitFile, *LitAbs))
        {
            const int32 HeaderSize = 8;
            if (LitFile.Num() >= HeaderSize)
            {
                const char* Magic = reinterpret_cast<const char*>(LitFile.GetData());
                int32 Version = 0;
                FMemory::Memcpy(&Version, LitFile.GetData() + 4, sizeof(int32));
                const int64 Expected = int64(Model.lightdata.Num()) * 3;
                const int64 Payload = int64(LitFile.Num()) - HeaderSize;

                if (FMemory::Memcmp(Magic, "QLIT", 4) == 0 && Version == 1 && Payload == Expected)
                {
                    LitRgbData.Reset(int32(Expected));
                    LitRgbData.Append(LitFile.GetData() + HeaderSize, int32(Expected));
                    bUseLit = true;
                }
            }
        }
    }

    TArray<FFaceLightmapCalc> Faces;
    Faces.Reserve(Model.faces.Num());

    for (int32 FaceIndex = 0; FaceIndex < Model.faces.Num(); FaceIndex++)
    {
        const bspformat29::Face& Face = Model.faces[FaceIndex];
        if (Face.lightofs < 0)
        {
            continue;
        }

        if (uint8(Face.styles[0]) == 255)
        {
            continue;
        }

        int32 TexMinS = 0;
        int32 TexMinT = 0;
        int32 W = 0;
        int32 H = 0;
        ComputeFaceLightmapDimensions(Model, FaceIndex, TexMinS, TexMinT, W, H);
        if (W <= 0 || H <= 0)
        {
            continue;
        }

        const int64 BytesNeeded = int64(Face.lightofs) + int64(W) * int64(H);
        if (BytesNeeded > int64(Model.lightdata.Num()))
        {
            continue;
        }

        FFaceLightmapCalc Info;
        Info.FaceIndex = FaceIndex;
        Info.TexMinS = TexMinS;
        Info.TexMinT = TexMinT;
        Info.W = W;
        Info.H = H;
        Info.LightOfs = Face.lightofs;
        Faces.Add(Info);
    }

    if (Faces.Num() == 0)
    {
        return false;
    }

    Faces.Sort([](const FFaceLightmapCalc& A, const FFaceLightmapCalc& B)
    {
        if (A.H != B.H)
        {
            return A.H > B.H;
        }
        return A.W > B.W;
    });

    const int32 MaxAtlasSize = 4096;
    int32 AtlasSize = 1024;
    bool bPacked = false;

    struct FPlaced
    {
        int32 FaceIndex = -1;
        int32 X = 0;
        int32 Y = 0;
        int32 W = 0;
        int32 H = 0;
        int32 TexMinS = 0;
        int32 TexMinT = 0;
        int32 LightOfs = -1;
    };

    TArray<FPlaced> Placed;

    while (AtlasSize <= MaxAtlasSize && !bPacked)
    {
        Placed.Reset();

        int32 CursorX = 0;
        int32 CursorY = 0;
        int32 RowH = 0;

        bool bFail = false;

        for (const FFaceLightmapCalc& F : Faces)
        {
            const int32 Pad = 2;
            const int32 RW = F.W + Pad * 2;
            const int32 RH = F.H + Pad * 2;

            if (RW > AtlasSize || RH > AtlasSize)
            {
                bFail = true;
                break;
            }

            if (CursorX + RW > AtlasSize)
            {
                CursorX = 0;
                CursorY += RowH;
                RowH = 0;
            }

            if (CursorY + RH > AtlasSize)
            {
                bFail = true;
                break;
            }

            FPlaced P;
            P.FaceIndex = F.FaceIndex;
            P.X = CursorX + Pad;
            P.Y = CursorY + Pad;
            P.W = F.W;
            P.H = F.H;
            P.TexMinS = F.TexMinS;
            P.TexMinT = F.TexMinT;
            P.LightOfs = F.LightOfs;
            Placed.Add(P);

            CursorX += RW;
            RowH = FMath::Max(RowH, RH);
        }

        if (!bFail)
        {
            bPacked = true;
            break;
        }

        AtlasSize *= 2;
    }

    if (!bPacked)
    {
        UE_LOG(LogTemp, Warning, TEXT("BSP Import: Could not pack lightmaps into an atlas (too large)"));
        return false;
    }

    OutAtlas.AtlasW = AtlasSize;
    OutAtlas.AtlasH = AtlasSize;

    TArray<uint8> AtlasData;
    TArray<uint8> AtlasDataBGRA;
    if (bUseLit)
    {
        AtlasDataBGRA.SetNumZeroed(AtlasSize * AtlasSize * 4);
    }
    else
    {
        AtlasData.SetNumZeroed(AtlasSize * AtlasSize);
    }

    for (const FPlaced& P : Placed)
    {
        const int32 FaceIndex = P.FaceIndex;
        FLightmapAtlasFace FaceInfo;
        FaceInfo.X = P.X;
        FaceInfo.Y = P.Y;
        FaceInfo.W = P.W;
        FaceInfo.H = P.H;
        FaceInfo.TexMinS = P.TexMinS;
        FaceInfo.TexMinT = P.TexMinT;
        OutAtlas.FaceToAtlas.Add(FaceIndex, FaceInfo);

        const int32 SrcOfs = P.LightOfs;
        for (int32 Y = 0; Y < P.H; Y++)
        {
            const int32 SrcRow = SrcOfs + Y * P.W;
            const int32 DstRow = (P.Y + Y) * AtlasSize + P.X;
            for (int32 X = 0; X < P.W; X++)
            {
                if (bUseLit)
                {
                    const int32 SrcIdx = (SrcRow + X) * 3;
                    const int32 DstIdx = (DstRow + X) * 4;
                    AtlasDataBGRA[DstIdx + 0] = LitRgbData[SrcIdx + 2];
                    AtlasDataBGRA[DstIdx + 1] = LitRgbData[SrcIdx + 1];
                    AtlasDataBGRA[DstIdx + 2] = LitRgbData[SrcIdx + 0];
                    AtlasDataBGRA[DstIdx + 3] = 255;
                }
                else
                {
                    AtlasData[DstRow + X] = Model.lightdata[SrcRow + X];
                }
            }
        }

        // simple padding: duplicate edge luxels into the padding area
        const int32 Pad = 2;
        for (int32 Y = -Pad; Y < P.H + Pad; Y++)
        {
            const int32 SrcY = FMath::Clamp(Y, 0, P.H - 1);
            for (int32 X = -Pad; X < P.W + Pad; X++)
            {
                const int32 SrcX = FMath::Clamp(X, 0, P.W - 1);
                const int32 DstX = P.X + X;
                const int32 DstY = P.Y + Y;
                if (DstX < 0 || DstY < 0 || DstX >= AtlasSize || DstY >= AtlasSize)
                {
                    continue;
                }
                if (bUseLit)
                {
                    const int32 SrcIdx = (SrcOfs + SrcY * P.W + SrcX) * 3;
                    const int32 DstIdx = (DstY * AtlasSize + DstX) * 4;
                    AtlasDataBGRA[DstIdx + 0] = LitRgbData[SrcIdx + 2];
                    AtlasDataBGRA[DstIdx + 1] = LitRgbData[SrcIdx + 1];
                    AtlasDataBGRA[DstIdx + 2] = LitRgbData[SrcIdx + 0];
                    AtlasDataBGRA[DstIdx + 3] = 255;
                }
                else
                {
                    const uint8 V = Model.lightdata[SrcOfs + SrcY * P.W + SrcX];
                    AtlasData[DstY * AtlasSize + DstX] = V;
                }
            }
        }
    }

    const FString TexName = FString::Printf(TEXT("LM_%s"), *MapName);
    const FString TexAssetName = TEXT("T_") + TexName;
    UPackage* TexPkg = CreateAssetPackage(LightmapsPath / TexAssetName);
    if (!TexPkg)
    {
        return false;
    }

    UTexture2D* Tex = nullptr;
    if (bUseLit)
    {
        Tex = QuakeCommon::CreateOrUpdateUTexture2DFromBGRA(TexName, AtlasSize, AtlasSize, AtlasDataBGRA, *TexPkg, bOverwrite);
    }
    else
    {
        TArray<QuakeCommon::QColor> GrayPalette;
        GrayPalette.SetNumUninitialized(256);
        for (int32 I = 0; I < 256; I++)
        {
            GrayPalette[I].r = uint8(I);
            GrayPalette[I].g = uint8(I);
            GrayPalette[I].b = uint8(I);
        }
        Tex = QuakeCommon::CreateOrUpdateUTexture2D(TexName, AtlasSize, AtlasSize, AtlasData, *TexPkg, GrayPalette, bOverwrite, false);
    }
    if (!Tex)
    {
        return false;
    }

	// Lightmaps should be filterable (unlike most Quake palette textures).
	Tex->PreEditChange(nullptr);
	Tex->SRGB = false;
	Tex->Filter = TF_Default;
	Tex->LODGroup = TEXTUREGROUP_World;
	Tex->MipGenSettings = TMGS_NoMipmaps;
	Tex->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	Tex->NeverStream = true;
	Tex->UpdateResource();
	Tex->PostEditChange();

    OutAtlas.LightmapTextureObjectPath = Tex->GetPathName();
    return true;
}

static FVector2f ComputeLightmapUVForFace(const bspformat29::Bsp_29& Model, int32 FaceIndex, float S, float T, const FLightmapAtlas* Atlas)
{
    if (!Atlas)
    {
        return FVector2f(0.0f, 0.0f);
    }

    const FLightmapAtlasFace* Info = Atlas->FaceToAtlas.Find(FaceIndex);
    if (!Info || Atlas->AtlasW <= 0 || Atlas->AtlasH <= 0)
    {
        return FVector2f(0.0f, 0.0f);
    }

    const float LMs = (S - float(Info->TexMinS)) / 16.0f;
    const float LMt = (T - float(Info->TexMinT)) / 16.0f;

    const float U = (float(Info->X) + LMs + 0.5f) / float(Atlas->AtlasW);
    const float V = (float(Info->Y) + LMt + 0.5f) / float(Atlas->AtlasH);
    return FVector2f(U, V);
}

static UMaterialInterface* GetWorldGridMaterial()
{
    static UMaterialInterface* Cached = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
    return Cached ? Cached : UMaterial::GetDefaultMaterial(MD_Surface);
}

    static void BuildStaticMesh(UStaticMesh* StaticMesh, const bspformat29::Bsp_29& Model, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>* MaskedTextureNames, const FWorldChunkBuild& Chunk, int32 LightmapSize, const FName& CollisionProfileName, const FName& MaskedCollisionProfileName, bool bGenerateLightmapUVs)
    {
        if (!StaticMesh)
        {
            return;
        }

        bool bHasMaskedTexture = false;
        for (int32 Slot = 0; Slot < Chunk.SlotToTextureId.Num(); Slot++)
        {
            const int32 TextureId = Chunk.SlotToTextureId[Slot];
            const FString& MatName = Model.textures[TextureId].name;
            const FString SafeSlotName = SanitizeSurfaceNameForAsset(MatName);
            if (!bHasMaskedTexture && MaskedTextureNames && MaskedTextureNames->Contains(MatName))
            {
                bHasMaskedTexture = true;
            }

            UMaterialInterface* Material = nullptr;

            if (const UMaterialInterface* const* Found = MaterialsByName.Find(MatName))
            {
                Material = const_cast<UMaterialInterface*>(*Found);
            }

            if (!Material)
            {
                Material = GetWorldGridMaterial();
            }

            StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, FName(*SafeSlotName), FName(*SafeSlotName)));
        }

        FStaticMeshSourceModel* SrcModel = &StaticMesh->AddSourceModel();
        SrcModel->BuildSettings.MinLightmapResolution = LightmapSize;
        SrcModel->BuildSettings.SrcLightmapIndex = 0;
        SrcModel->BuildSettings.DstLightmapIndex = 1;
        SrcModel->BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
        SrcModel->BuildSettings.bUseFullPrecisionUVs = true;

        FRawMesh LocalCopy = Chunk.RawMesh;
        SrcModel->RawMeshBulkData->SaveRawMesh(LocalCopy);

        StaticMesh->SetLightingGuid();
        StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
        StaticMesh->EnforceLightmapRestrictions();
        StaticMesh->Build();
        StaticMesh->SetLightingGuid();
        StaticMesh->SetLightMapResolution(LightmapSize);
        StaticMesh->SetLightMapCoordinateIndex(1);

        UBodySetup* BodySetup = StaticMesh->GetBodySetup();
        if (!BodySetup)
        {
            StaticMesh->CreateBodySetup();
            BodySetup = StaticMesh->GetBodySetup();
        }

        if (BodySetup)
        {
            FName EffectiveCollisionProfile = CollisionProfileName;
            if (bHasMaskedTexture && MaskedCollisionProfileName != NAME_None && CollisionProfileName != UCollisionProfile::NoCollision_ProfileName)
            {
                EffectiveCollisionProfile = MaskedCollisionProfileName;
            }
            const bool bEnableCollision = (EffectiveCollisionProfile != NAME_None) && (EffectiveCollisionProfile != UCollisionProfile::NoCollision_ProfileName);

            if (bEnableCollision)
            {
                BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
                BodySetup->DefaultInstance.SetCollisionProfileName(EffectiveCollisionProfile);
                BodySetup->InvalidatePhysicsData();
                BodySetup->CreatePhysicsMeshes();
            }
            else
            {
                BodySetup->CollisionTraceFlag = CTF_UseDefault;
                BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
                BodySetup->InvalidatePhysicsData();
            }
        }

        StaticMesh->PostEditChange();
    }

    static void CreateWorldChunks(const FString& MeshesPath, const FString& MapName, const bspformat29::Bsp_29& Model, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>& MaskedTextureNames, int32 ChunkSize, float ImportScale, bool bIncludeSky, bool bIncludeWater, const FName& BspCollisionProfile, const FName& MaskedCollisionProfile, const FName& WaterCollisionProfile, const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths, TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths , const bsputils::FLightmapAtlas* LightmapAtlas)
    {
        using namespace bsputils;

        struct FFaceTemp
        {
            int32 NumTris = 0;
            FVector Normal;
            TArray<int32> BspVertexIds;
            int32 TexInfo = 0;
            TArray<FVector2f> TexCoords;
            TArray<FVector2f> LightmapST;
            FVector3f Center;
        };

        struct FChunkPair
        {
            FWorldChunkBuild Opaque;
            FWorldChunkBuild Transparent;
        };

        TMap<FIntVector, FChunkPair> BspChunkMap;
        TMap<FIntVector, FWorldChunkBuild> WaterChunkMap;
        TMap<FIntVector, FWorldChunkBuild> SkyChunkMap;

        int32 FirstFace = 0;
        int32 FaceCount = 0;

        if (Model.submodels.Num() > 0)
        {
            FirstFace = Model.submodels[0].firstface;
            FaceCount = Model.submodels[0].numfaces;
        }
        else
        {
            // Some malformed files (or failed model lump parse) may yield an empty submodel list.
            // Fall back to treating the entire faces lump as the world model to avoid a hard crash.
            FirstFace = 0;
            FaceCount = Model.faces.Num();
        }

        for (int32 F = FirstFace; F < FirstFace + FaceCount; F++)
        {
            const bspformat29::Face& Face = Model.faces[F];
            const bspformat29::TexInfo& Ti = Model.texinfos[Face.texinfo];
            const bspformat29::Texture& Tex = Model.textures[Ti.miptex];

            const bool bIsSky = Tex.name.StartsWith(TEXT("sky"));
            const bool bIsWater = Tex.name.StartsWith(TEXT("*"));

            if (bIsSky && !bIncludeSky)
            {
                continue;
            }
            if (bIsWater && !bIncludeWater)
            {
                continue;
            }

            const bool bTransparent = (!bIsSky && !bIsWater) ? IsTransparentSurfaceName(Tex.name) : false;

            FFaceTemp Temp;
            Temp.TexInfo = Face.texinfo;
            Temp.NumTris = int32(Face.numedges) - 2;

            for (int32 I = 0; I < 3; I++)
            {
                Temp.Normal[I] = Model.planes[Face.planenum].normal[I];
            }

            FVector3f Sum(0, 0, 0);

            for (int32 E = Face.numedges; E-- > 0;)
            {
                const bspformat29::Surfedge& Surfedge = Model.surfedges[Face.firstedge + E];
                const bspformat29::Edge& Edge = Model.edges[abs(Surfedge.index)];

                int32 VertexId = Edge.first;
                if (Surfedge.index < 0)
                {
                    VertexId = Edge.second;
                }

                Temp.BspVertexIds.Add(VertexId);

                const bspformat29::Point3f& P = Model.vertices[VertexId];
                const FVector3f Point(-P.x, P.y, P.z);
                Sum += Point;

                FVector2f TexCoord;
                const FVector3f Unflipped(P.x, P.y, P.z);

                float Sraw = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[0][0], Ti.vecs[0][1], Ti.vecs[0][2])) + Ti.vecs[0][3];
                float Traw = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[1][0], Ti.vecs[1][1], Ti.vecs[1][2])) + Ti.vecs[1][3];

                TexCoord.X = Sraw / Tex.width;
                TexCoord.Y = Traw / Tex.height;

                Temp.TexCoords.Add(TexCoord);
                Temp.LightmapST.Add(FVector2f(Sraw, Traw));
            }

            Temp.Center = Sum / float(Temp.BspVertexIds.Num());

            const FIntVector Key = GetChunkKey3D(Temp.Center, ChunkSize);
            FWorldChunkBuild* ChunkPtr = nullptr;

            if (bIsSky)
            {
                ChunkPtr = &SkyChunkMap.FindOrAdd(Key);
            }
            else if (bIsWater)
            {
                ChunkPtr = &WaterChunkMap.FindOrAdd(Key);
            }
            else
            {
                FChunkPair& Pair = BspChunkMap.FindOrAdd(Key);
                ChunkPtr = bTransparent ? &Pair.Transparent : &Pair.Opaque;
            }

            FWorldChunkBuild& Chunk = *ChunkPtr;

            for (int32 J = 0; J < Temp.NumTris; J++)
            {
                const int32 A = 0;
                const int32 B = J + 1;
                const int32 C = J + 2;

                const uint32 VA = GetOrAddLocalVertex(Chunk, Model, Temp.BspVertexIds[A], ImportScale);
                const uint32 VB = GetOrAddLocalVertex(Chunk, Model, Temp.BspVertexIds[B], ImportScale);
                const uint32 VC = GetOrAddLocalVertex(Chunk, Model, Temp.BspVertexIds[C], ImportScale);

                const FVector3f N(Temp.Normal.X, Temp.Normal.Y, Temp.Normal.Z);

                AddWedgeEntry(Chunk.RawMesh, VA, N, Temp.TexCoords[A], ComputeLightmapUVForFace(Model, F, Temp.LightmapST[A].X, Temp.LightmapST[A].Y, LightmapAtlas));
                AddWedgeEntry(Chunk.RawMesh, VB, N, Temp.TexCoords[B], ComputeLightmapUVForFace(Model, F, Temp.LightmapST[B].X, Temp.LightmapST[B].Y, LightmapAtlas));
                AddWedgeEntry(Chunk.RawMesh, VC, N, Temp.TexCoords[C], ComputeLightmapUVForFace(Model, F, Temp.LightmapST[C].X, Temp.LightmapST[C].Y, LightmapAtlas));

                const int32 TextureId = Model.texinfos[Temp.TexInfo].miptex;
                const int32 Slot = GetOrAddMaterialSlot(Chunk, TextureId);
                Chunk.RawMesh.FaceMaterialIndices.Add(Slot);
                Chunk.RawMesh.FaceSmoothingMasks.Add(0);
            }
        }

        const int32 LightmapSize = 128;

        for (const auto& PairIt : BspChunkMap)
        {
            const FIntVector Key = PairIt.Key;
            const FChunkPair& Pair = PairIt.Value;

            if (Pair.Opaque.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_%d_%d_%d"), *MapName, Key.X, Key.Y, Key.Z);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Pair.Opaque, LightmapSize, BspCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

                if (OutBspMeshObjectPaths)
                {
                    OutBspMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }

            if (Pair.Transparent.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_%d_%d_%d_Trans"), *MapName, Key.X, Key.Y, Key.Z);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Pair.Transparent, LightmapSize, BspCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

                if (OutBspMeshObjectPaths)
                {
                    OutBspMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }
        }

        for (const auto& It : WaterChunkMap)
        {
            const FIntVector Key = It.Key;
            const FWorldChunkBuild& Chunk = It.Value;
            if (Chunk.RawMesh.WedgeIndices.Num() <= 0)
            {
                continue;
            }

            const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_Water_%d_%d_%d"), *MapName, Key.X, Key.Y, Key.Z);
            const FString LongPkg = MeshesPath / ChunkName;
            UPackage* Pkg = CreateAssetPackage(LongPkg);
            UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
            BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Chunk, LightmapSize, WaterCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

            if (OutWaterMeshObjectPaths)
            {
                OutWaterMeshObjectPaths->Add(StaticMesh->GetPathName());
            }
        }

        for (const auto& It : SkyChunkMap)
        {
            const FIntVector Key = It.Key;
            const FWorldChunkBuild& Chunk = It.Value;
            if (Chunk.RawMesh.WedgeIndices.Num() <= 0)
            {
                continue;
            }

            const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_Sky_%d_%d_%d"), *MapName, Key.X, Key.Y, Key.Z);
            const FString LongPkg = MeshesPath / ChunkName;
            UPackage* Pkg = CreateAssetPackage(LongPkg);
            UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
            BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Chunk, LightmapSize, SkyCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

            if (OutSkyMeshObjectPaths)
            {
                OutSkyMeshObjectPaths->Add(StaticMesh->GetPathName());
            }
        }
    }

    static void CreateLeafChunks(const FString& MeshesPath, const FString& MapName, const bspformat29::Bsp_29& Model, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>& MaskedTextureNames, float ImportScale, bool bIncludeSky, bool bIncludeWater, const FName& BspCollisionProfile, const FName& MaskedCollisionProfile, const FName& WaterCollisionProfile, const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths, TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths , const bsputils::FLightmapAtlas* LightmapAtlas)
    {
        using namespace bsputils;

        struct FLeafPair
        {
            FWorldChunkBuild Opaque;
            FWorldChunkBuild Transparent;
        };

        TMap<int32, FLeafPair> LeafToChunk;
        TMap<int32, FWorldChunkBuild> WaterLeafToChunk;
        TMap<int32, FWorldChunkBuild> SkyLeafToChunk;

        for (int32 LeafIndex = 0; LeafIndex < Model.leaves.Num(); LeafIndex++)
        {
            const bspformat29::Leaf& Leaf = Model.leaves[LeafIndex];
            if (Leaf.nummarksurfaces == 0)
            {
                continue;
            }
            if (Leaf.contents == ELeafContentType::Solid)
            {
                continue;
            }

            FLeafPair& Pair = LeafToChunk.FindOrAdd(LeafIndex);

            TSet<int32> FaceSet;
            const uint32 NumMarkSurfaces = (uint32)Leaf.nummarksurfaces;
            for (uint32 I = 0; I < NumMarkSurfaces; I++)
            {
                const int32 MsIndex = int32(Leaf.firstmarksurface) + int32(I);
                if (!Model.marksurfaces.IsValidIndex(MsIndex))
                {
                    continue;
                }

                const int32 FaceIndex = int32(Model.marksurfaces[MsIndex].index);
                if (!Model.faces.IsValidIndex(FaceIndex))
                {
                    continue;
                }

                if (FaceSet.Contains(FaceIndex))
                {
                    continue;
                }
                FaceSet.Add(FaceIndex);

                const bspformat29::Face& Face = Model.faces[FaceIndex];
                const bspformat29::TexInfo& Ti = Model.texinfos[Face.texinfo];
                const bspformat29::Texture& Tex = Model.textures[Ti.miptex];
                const bool bIsSky = Tex.name.StartsWith(TEXT("sky"));
                const bool bIsWater = Tex.name.StartsWith(TEXT("*"));

                if (bIsSky && !bIncludeSky)
                {
                    continue;
                }
                if (bIsWater && !bIncludeWater)
                {
                    continue;
                }

                FWorldChunkBuild* ChunkPtr = nullptr;
                if (bIsSky)
                {
                    ChunkPtr = &SkyLeafToChunk.FindOrAdd(LeafIndex);
                }
                else if (bIsWater)
                {
                    ChunkPtr = &WaterLeafToChunk.FindOrAdd(LeafIndex);
                }
                else
                {
                    const bool bTransparent = IsTransparentSurfaceName(Tex.name);
                    ChunkPtr = bTransparent ? &Pair.Transparent : &Pair.Opaque;
                }

                FWorldChunkBuild& Chunk = *ChunkPtr;

                FVector Normal;
                for (int32 N = 0; N < 3; N++)
                {
                    Normal[N] = Model.planes[Face.planenum].normal[N];
                }

                TArray<int32> BspVertexIds;
                TArray<FVector2f> TexCoords;
                TArray<FVector2f> LightmapST;

                for (int32 E = Face.numedges; E-- > 0;)
                {
                    const bspformat29::Surfedge& Surfedge = Model.surfedges[Face.firstedge + E];
                    const bspformat29::Edge& Edge = Model.edges[abs(Surfedge.index)];

                    int32 VertexId = Edge.first;
                    if (Surfedge.index < 0)
                    {
                        VertexId = Edge.second;
                    }

                    BspVertexIds.Add(VertexId);

                    const bspformat29::Point3f& P = Model.vertices[VertexId];
                    const FVector3f Unflipped(P.x, P.y, P.z);

                    FVector2f TexCoord;
float Sraw = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[0][0], Ti.vecs[0][1], Ti.vecs[0][2])) + Ti.vecs[0][3];
                    float Traw = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[1][0], Ti.vecs[1][1], Ti.vecs[1][2])) + Ti.vecs[1][3];
                    TexCoord.X = Sraw / Tex.width;
                    TexCoord.Y = Traw / Tex.height;
                    TexCoords.Add(TexCoord);
                    LightmapST.Add(FVector2f(Sraw, Traw));
                }

                const int32 NumTris = int32(Face.numedges) - 2;
                for (int32 J = 0; J < NumTris; J++)
                {
                    const int32 A = 0;
                    const int32 B = J + 1;
                    const int32 C = J + 2;

                    const uint32 VA = GetOrAddLocalVertex(Chunk, Model, BspVertexIds[A], ImportScale);
                    const uint32 VB = GetOrAddLocalVertex(Chunk, Model, BspVertexIds[B], ImportScale);
                    const uint32 VC = GetOrAddLocalVertex(Chunk, Model, BspVertexIds[C], ImportScale);

                    const FVector3f N(Normal.X, Normal.Y, Normal.Z);
                    AddWedgeEntry(Chunk.RawMesh, VA, N, TexCoords[A], ComputeLightmapUVForFace(Model, FaceIndex, LightmapST[A].X, LightmapST[A].Y, LightmapAtlas));
                    AddWedgeEntry(Chunk.RawMesh, VB, N, TexCoords[B], ComputeLightmapUVForFace(Model, FaceIndex, LightmapST[B].X, LightmapST[B].Y, LightmapAtlas));
                    AddWedgeEntry(Chunk.RawMesh, VC, N, TexCoords[C], ComputeLightmapUVForFace(Model, FaceIndex, LightmapST[C].X, LightmapST[C].Y, LightmapAtlas));

                    const int32 TextureId = Model.texinfos[Face.texinfo].miptex;
                    const int32 Slot = GetOrAddMaterialSlot(Chunk, TextureId);
                    Chunk.RawMesh.FaceMaterialIndices.Add(Slot);
                    Chunk.RawMesh.FaceSmoothingMasks.Add(0);
                }
            }
        }

        const int32 LightmapSize = 128;

        for (const auto& PairIt : LeafToChunk)
        {
            const int32 LeafIndex = PairIt.Key;
            const FLeafPair& Pair = PairIt.Value;

            if (Pair.Opaque.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_leaf_%d"), *MapName, LeafIndex);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Pair.Opaque, LightmapSize, BspCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

                if (OutBspMeshObjectPaths)
                {
                    OutBspMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }

            if (Pair.Transparent.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_leaf_%d_Trans"), *MapName, LeafIndex);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Pair.Transparent, LightmapSize, BspCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

                if (OutBspMeshObjectPaths)
                {
                    OutBspMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }
        }

        for (const auto& It : WaterLeafToChunk)
        {
            const int32 LeafIndex = It.Key;
            const FWorldChunkBuild& Chunk = It.Value;
            if (Chunk.RawMesh.WedgeIndices.Num() <= 0)
            {
                continue;
            }

            const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_Water_leaf_%d"), *MapName, LeafIndex);
            const FString LongPkg = MeshesPath / ChunkName;
            UPackage* Pkg = CreateAssetPackage(LongPkg);
            UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
            BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Chunk, LightmapSize, WaterCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

            if (OutWaterMeshObjectPaths)
            {
                OutWaterMeshObjectPaths->Add(StaticMesh->GetPathName());
            }
        }

        for (const auto& It : SkyLeafToChunk)
        {
            const int32 LeafIndex = It.Key;
            const FWorldChunkBuild& Chunk = It.Value;
            if (Chunk.RawMesh.WedgeIndices.Num() <= 0)
            {
                continue;
            }

            const FString ChunkName = FString::Printf(TEXT("SM_%s_BSP_World_Sky_leaf_%d"), *MapName, LeafIndex);
            const FString LongPkg = MeshesPath / ChunkName;
            UPackage* Pkg = CreateAssetPackage(LongPkg);
            UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
            BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Chunk, LightmapSize, SkyCollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

            if (OutSkyMeshObjectPaths)
            {
                OutSkyMeshObjectPaths->Add(StaticMesh->GetPathName());
            }
        }
    }

    bool CreateSubmodelStaticMesh(const bspformat29::Bsp_29& Model, const FString& MeshesPath, const FString& MeshAssetName, uint8 SubModelId, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>& MaskedTextureNames, float ImportScale, const FName& DefaultCollisionProfile, const FName& MaskedCollisionProfile, FString& OutObjectPath, const bsputils::FLightmapAtlas* LightmapAtlas)
    {
        if (!Model.submodels.IsValidIndex(SubModelId))
        {
            return false;
        }

        FWorldChunkBuild Chunk;
        bool bAnyFace = false;
        bool bAnyTriggerTex = false;

        const bspformat29::SubModel& Sub = Model.submodels[SubModelId];
        for (int32 F = Sub.firstface; F < Sub.firstface + Sub.numfaces; F++)
        {
            if (!Model.faces.IsValidIndex(F))
            {
                continue;
            }

            const bspformat29::Face& Face = Model.faces[F];
            const bspformat29::TexInfo& Ti = Model.texinfos[Face.texinfo];
            const bspformat29::Texture& Tex = Model.textures[Ti.miptex];

            bAnyFace = true;
            if (Tex.name.Equals(TEXT("trigger"), ESearchCase::IgnoreCase))
            {
                bAnyTriggerTex = true;
            }

            FVector Normal;
            for (int32 N = 0; N < 3; N++)
            {
                Normal[N] = Model.planes[Face.planenum].normal[N];
            }

            TArray<int32> BspVertexIds;
            TArray<FVector2f> TexCoords;
            TArray<FVector2f> LightmapST;
            BspVertexIds.Reserve(Face.numedges);
            TexCoords.Reserve(Face.numedges);

            for (int32 E = Face.numedges; E-- > 0;)
            {
                const bspformat29::Surfedge& Surfedge = Model.surfedges[Face.firstedge + E];
                const bspformat29::Edge& Edge = Model.edges[abs(Surfedge.index)];

                int32 VertexId = Edge.first;
                if (Surfedge.index < 0)
                {
                    VertexId = Edge.second;
                }

                BspVertexIds.Add(VertexId);

                const bspformat29::Point3f& P = Model.vertices[VertexId];
                const FVector3f Unflipped(P.x, P.y, P.z);

                FVector2f TexCoord;
                const float S = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[0][0], Ti.vecs[0][1], Ti.vecs[0][2])) + Ti.vecs[0][3];
                const float T = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[1][0], Ti.vecs[1][1], Ti.vecs[1][2])) + Ti.vecs[1][3];
                TexCoord.X = S;
                TexCoord.Y = T;
                LightmapST.Add(FVector2f(S, T));
                TexCoord.X /= Tex.width;
                TexCoord.Y /= Tex.height;
                TexCoords.Add(TexCoord);
            }

            const int32 NumTris = int32(Face.numedges) - 2;
            for (int32 J = 0; J < NumTris; J++)
            {
                const int32 A = 0;
                const int32 B = J + 1;
                const int32 C = J + 2;

                const uint32 VA = GetOrAddLocalVertex(Chunk, Model, BspVertexIds[A], ImportScale);
                const uint32 VB = GetOrAddLocalVertex(Chunk, Model, BspVertexIds[B], ImportScale);
                const uint32 VC = GetOrAddLocalVertex(Chunk, Model, BspVertexIds[C], ImportScale);

                const FVector3f N(Normal.X, Normal.Y, Normal.Z);
                AddWedgeEntry(Chunk.RawMesh, VA, N, TexCoords[A], ComputeLightmapUVForFace(Model, F, LightmapST[A].X, LightmapST[A].Y, LightmapAtlas));
                AddWedgeEntry(Chunk.RawMesh, VB, N, TexCoords[B], ComputeLightmapUVForFace(Model, F, LightmapST[B].X, LightmapST[B].Y, LightmapAtlas));
                AddWedgeEntry(Chunk.RawMesh, VC, N, TexCoords[C], ComputeLightmapUVForFace(Model, F, LightmapST[C].X, LightmapST[C].Y, LightmapAtlas));

                const int32 TextureId = Model.texinfos[Face.texinfo].miptex;
                const int32 Slot = GetOrAddMaterialSlot(Chunk, TextureId);
                Chunk.RawMesh.FaceMaterialIndices.Add(Slot);
                Chunk.RawMesh.FaceSmoothingMasks.Add(0);
            }
        }

        if (!bAnyFace || Chunk.RawMesh.WedgeIndices.Num() == 0)
        {
            return false;
        }

        const FString LongPkg = MeshesPath / MeshAssetName;
        UPackage* Pkg = CreateAssetPackage(LongPkg);
        UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, MeshAssetName);

        const int32 LightmapSize = 64;
        const FName CollisionProfile = bAnyTriggerTex ? UCollisionProfile::NoCollision_ProfileName : DefaultCollisionProfile;
        BuildStaticMesh(StaticMesh, Model, MaterialsByName, &MaskedTextureNames, Chunk, LightmapSize, CollisionProfile, MaskedCollisionProfile, LightmapAtlas == nullptr);

        OutObjectPath = StaticMesh->GetPathName();
        return true;
    }

    void CreateSubmodel(UPackage& package, const uint8 id, const bspformat29::Bsp_29& model, const TMap<FString, UMaterialInterface*>& MaterialsByName)
    {
        using namespace bsputils;

        struct Triface
        {
            int numtris = 0;
            FVector normal;
            TArray<uint32> points;
            int texinfo;
            TArray<FVector2f> texcoords;
        };

        FString submodelName("submodel");
        submodelName += "_";
        submodelName += FString::FromInt(id);

        UStaticMesh* staticmesh = NewObject<UStaticMesh>(&package, FName(*submodelName), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(staticmesh);

        TArray<Triface> faces;

        if (!model.submodels.IsValidIndex(id))
        {
            return;
        }

        const bspformat29::SubModel& SubModel = model.submodels[id];

        for (
            int f = SubModel.firstface;
            f < (SubModel.numfaces + SubModel.firstface);
            f++
            )
        {
            const bspformat29::Face& face = model.faces[f];

            const bspformat29::TexInfo& ti = model.texinfos[face.texinfo];
            const bspformat29::Texture& tex = model.textures[ti.miptex];

            if (tex.name.StartsWith(TEXT("sky")) || tex.name.StartsWith(TEXT("*")))
            {
                // Skip sky surfaces. We wont need them.
                continue;
            }

            Triface triface;
            triface.texinfo = face.texinfo;
            triface.numtris = face.numedges - 2; // make up number of this needed for this face 

            for (int i = 0; i < 3; i++)
            {
                triface.normal[i] = model.planes[face.planenum].normal[i];
            }

            for (int e = face.numedges; e-- > 0;) // extract all vertex
            { 
                const bspformat29::Surfedge& surfedge = model.surfedges[face.firstedge + e];
                const bspformat29::Edge& edge = model.edges[abs(surfedge.index)];

                int32 vertex_id = edge.first;

                if (surfedge.index < 0)
                {
                    vertex_id = edge.second;
                }

                triface.points.Add(uint32(vertex_id));

                FVector2f tex_coord;

                // Generate texture coordinates

                FVector3f point = FVector3f(
                    model.vertices[vertex_id].x,
                    model.vertices[vertex_id].y,
                    model.vertices[vertex_id].z
                );

                tex_coord.X = FVector3f::DotProduct(point, FVector3f(ti.vecs[0][0], ti.vecs[0][1], ti.vecs[0][2])) + ti.vecs[0][3];
                tex_coord.Y = FVector3f::DotProduct(point, FVector3f(ti.vecs[1][0], ti.vecs[1][1], ti.vecs[1][2])) + ti.vecs[1][3];

                tex_coord.X /= tex.width;
                tex_coord.Y /= tex.height;

                triface.texcoords.Add(tex_coord);
            }

            faces.Add(triface);
        }

        FRawMesh* rmesh = new FRawMesh();

        // Vertices
        for (int i = 0; i < model.vertices.Num(); i++)
        {
            FVector3f vec(
                -model.vertices[i].x, // flip X axis
                model.vertices[i].y,
                model.vertices[i].z);

            rmesh->VertexPositions.Add(vec);
        }

        // tris

        for (int i = 0; i < faces.Num(); i++)
        {
            for (int j = 0; j < faces[i].numtris; j++)
            {
                AddWedgeEntry(
                    *rmesh,
                    faces[i].points[0],
                    FVector3f(faces[i].normal.X, faces[i].normal.Y, faces[i].normal.Z),
                    faces[i].texcoords[0],
                    faces[i].texcoords[0]
                );

                for (int c = 1; c < 3; c++)
                {
                    int index = j + c;

                    AddWedgeEntry(
                        *rmesh,
                        faces[i].points[index],
                        FVector3f(faces[i].normal.X, faces[i].normal.Y, faces[i].normal.Z),
                        faces[i].texcoords[index],
                        faces[i].texcoords[index]
                    );
                }

                int32 materialId = model.texinfos[faces[i].texinfo].miptex;

                UMaterialInterface* material = nullptr;

                if (const UMaterialInterface* const* Found = MaterialsByName.Find(model.textures[materialId].name))
                {
                    material = const_cast<UMaterialInterface*>(*Found);
                }

                if (!material)
                {
                    material = GetWorldGridMaterial();
                }

                int32 MaterialIndex = staticmesh->GetStaticMaterials().AddUnique(
                    FStaticMaterial(
                        material,
                        FName(*model.textures[materialId].name),
                        FName(*model.textures[materialId].name)
                    )
                );

                rmesh->FaceMaterialIndices.Add(MaterialIndex);
                rmesh->FaceSmoothingMasks.Add(0); // TODO dont know how that work yet
            }
        }

        FStaticMeshSourceModel* srcModel = &staticmesh->AddSourceModel();

        int lightmapSize = 32;

        if (id == 0)
        {
            lightmapSize = 512; // main model mesh
        }

        srcModel->BuildSettings.MinLightmapResolution = lightmapSize;
        srcModel->BuildSettings.SrcLightmapIndex = 0;
        srcModel->BuildSettings.DstLightmapIndex = 1;
        srcModel->BuildSettings.bGenerateLightmapUVs = true;
        srcModel->BuildSettings.bUseFullPrecisionUVs = true;
        srcModel->RawMeshBulkData->SaveRawMesh(*rmesh);

        staticmesh->SetLightingGuid();
        staticmesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
        //staticmesh->CreateBodySetup();
        staticmesh->SetLightingGuid();
        staticmesh->EnforceLightmapRestrictions(); // Make sure the Lightmap UV point on a valid UVChannel
        staticmesh->Build();
        staticmesh->SetLightingGuid();
        staticmesh->SetLightMapResolution(lightmapSize);
        staticmesh->SetLightMapCoordinateIndex(1);

        // Workflow convenience: treat complex collision as simple for these imported meshes.
        UBodySetup* BodySetup = staticmesh->GetBodySetup();
        if (!BodySetup)
        {
            staticmesh->CreateBodySetup();
            BodySetup = staticmesh->GetBodySetup();
        }
        if (BodySetup)
        {
            BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
            BodySetup->InvalidatePhysicsData();
            BodySetup->CreatePhysicsMeshes();
        }
        staticmesh->PostEditChange();

        package.MarkPackageDirty();

        delete rmesh;
    }

    void ModelToStaticmeshes(const bspformat29::Bsp_29& model, const FString& MeshesPath, const FString& MapName, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>& MaskedTextureNames, bool bChunkWorld, int32 WorldChunkSize, float ImportScale, bool bIncludeSky, bool bIncludeWater, const FName& BspCollisionProfile, const FName& MaskedCollisionProfile, const FName& WaterCollisionProfile, const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths, TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths, const bsputils::FLightmapAtlas* LightmapAtlas)
    {
        if (bChunkWorld)
        {
            CreateWorldChunks(MeshesPath, MapName, model, MaterialsByName, MaskedTextureNames, WorldChunkSize, ImportScale, bIncludeSky, bIncludeWater, BspCollisionProfile, MaskedCollisionProfile, WaterCollisionProfile, SkyCollisionProfile, OutBspMeshObjectPaths, OutWaterMeshObjectPaths, OutSkyMeshObjectPaths, LightmapAtlas);
            return;
        }

        CreateLeafChunks(MeshesPath, MapName, model, MaterialsByName, MaskedTextureNames, ImportScale, bIncludeSky, bIncludeWater, BspCollisionProfile, MaskedCollisionProfile, WaterCollisionProfile, SkyCollisionProfile, OutBspMeshObjectPaths, OutWaterMeshObjectPaths, OutSkyMeshObjectPaths, LightmapAtlas);
    }

    bool AppendNextTextureData(const FString& name, const int frame, const bspformat29::Bsp_29& model, TArray<uint8>& data)
    {
        FString nextName = name;
        nextName[1] += frame;

         for (const auto& it : model.textures)
        {
            if (it.name == nextName)
            {
                data.Append(it.mip0);
                return true;
            }
        }

        return false;
    }
} // namespace bsputils
