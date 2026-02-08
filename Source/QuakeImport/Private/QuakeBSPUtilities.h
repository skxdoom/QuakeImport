// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "QuakeImportCommon.h"

class UTexture2D;
class UPackage;
class UMaterialInterface;

namespace bsputils
{
    enum class ELeafContentType
    {
        Empty = -1,
        Solid = -2,
        Water = -3,
        Slime = -4,
        Lava = -5,
        Sky = -6,
        Origin = -7,
        Clip = -8
    };

    namespace bspformat29
    {
        // bsp data structure
        // only used to deserialize quake bsp files

        constexpr int HEADER_VERSION_29 = 29;   // quake 1 version
        constexpr int HEADER_LUMP_SIZE = 15;   // how many lumps in the header

        constexpr int LUMP_ENTITIES = 0;
        constexpr int LUMP_PLANES = 1;
        constexpr int LUMP_TEXTURES = 2;
        constexpr int LUMP_VERTEXES = 3;
        constexpr int LUMP_VISIBILITY = 4;
        constexpr int LUMP_NODES = 5;
        constexpr int LUMP_TEXINFO = 6;
        constexpr int LUMP_FACES = 7;
        constexpr int LUMP_LIGHTING = 8;
        constexpr int LUMP_CLIPNODES = 9;
        constexpr int LUMP_LEAFS = 10;
        constexpr int LUMP_MARKSURFACES = 11;
        constexpr int LUMP_EDGES = 12;
        constexpr int LUMP_SURFEDGES = 13;
        constexpr int LUMP_MODELS = 14;

        constexpr int MAXLIGHTMAPS = 4;
        constexpr int MAXLEAVES = 8192;

        struct QColor
        {
            uint8 r;
            uint8 g;
            uint8 b;
        };

        struct Point2f
        {
            float x;
            float y;
        };

        struct Point3f
        {
            float x;
            float y;
            float z;
        };

        struct Lump
        {
            int position;
            int length;
        };

        struct Header
        {
            int     version;
            Lump    lumps[HEADER_LUMP_SIZE];
        };

        struct Edge
        {
            int32 first;
            int32 second;
        };

        struct Surfedge
        {
            int index;
        };

        struct Marksurface
        {
            int32 index;
        };

        struct Plane
        {
            float	normal[3];
            float	dist;
            char	type;
        };

        struct Face
        {
            int32   planenum;
            int32   side;

            int32   firstedge;
            int32   numedges;
            int32   texinfo;

            uint8   styles[MAXLIGHTMAPS];
            int32   lightofs;
        };

        struct Leaf
        {
            ELeafContentType    contents;
            int                 visofs;

            int32               mins[3];
            int32               maxs[3];

            int32               firstmarksurface;
            int32               nummarksurfaces;

            char                ambient_level[4];
        };

        struct Node
        {
            int32           planenum;
            int32           children[2];
            int32           mins[3];
            int32           maxs[3];
            int32           firstface;
            int32           numfaces;
        };

        // ---- On-disk structs for BSP29 (used only for deserialization) ----

        struct FileEdge
        {
            int16 first;
            int16 second;
        };

        struct FileMarksurface
        {
            int16 index;
        };

        struct FileFace
        {
            int16   planenum;
            int16   side;

            int32   firstedge;
            int16   numedges;
            int16   texinfo;

            uint8   styles[MAXLIGHTMAPS];
            int32   lightofs;
        };

        struct FileLeaf
        {
            ELeafContentType contents;
            int32            visofs;

            int16            mins[3];
            int16            maxs[3];

            uint16           firstmarksurface;
            uint16           nummarksurfaces;

            char             ambient_level[4];
        };

        struct FileNode
        {
            int32  planenum;
            int16  children[2];
            int16  mins[3];
            int16  maxs[3];
            uint16 firstface;
            uint16 numfaces;
        };

        struct SubModel
        {
            float   mins[3];
            float   maxs[3];
            float   origin[3];
            int     headnode[4];
            int     visleafs;
            int     firstface;
            int     numfaces;
        };

        struct TexInfo
        {
            float   vecs[2][4];
            int     miptex;
            int     flags;
        };

        struct Miptex
        {
            char        name[16];
            unsigned    width;
            unsigned    height;
            unsigned    offsets[4]; // four mip maps stored
        };

        struct Texture
        {
            FString         name;
            unsigned        width;
            unsigned        height;
            TArray<uint8>   mip0;
        };

        // Data storage for BSP version 29
        struct Bsp_29
        {
            TArray<Point3f>     vertices;
            TArray<Edge>        edges;
            TArray<Surfedge>    surfedges;
            TArray<Plane>       planes;
            TArray<Face>        faces;
            TArray<Marksurface> marksurfaces;
            TArray<Leaf>        leaves;
            TArray<Node>        nodes;
            TArray<SubModel>    submodels;
            TArray<TexInfo>     texinfos;
            TArray<Texture>     textures;
            FString             entities;
            TArray<uint8>       lightdata;
            TArray<uint8>       visdata;
        };
    }

    // BSP2 / 2PSB: Quake 1 BSP format extensions that lift many 16-bit limits by
    // switching indices to 32-bit while keeping the same lump directory.
    namespace bspformat2
    {
        constexpr char HEADER_IDENT_BSP2[4] = { 'B', 'S', 'P', '2' };
        constexpr char HEADER_IDENT_2PSB[4] = { '2', 'P', 'S', 'B' };

        // BSP2 / 2PSB isn't completely uniform across tools; some writers store
        // a 4-byte ident followed by a 32-bit version, while others omit the
        // version and start the lump directory immediately.
        struct Header
        {
            char                ident[4];
            int32               version;
            bspformat29::Lump   lumps[bspformat29::HEADER_LUMP_SIZE];
        };

        // Variant header used by some BSP2 / 2PSB files: no explicit version.
        struct HeaderNoVersion
        {
            char                ident[4];
            bspformat29::Lump   lumps[bspformat29::HEADER_LUMP_SIZE];
        };

        struct FileEdge
        {
            int32 first;
            int32 second;
        };

        struct FileMarksurface
        {
            int32 index;
        };

        struct FileFace
        {
            int32 planenum;
            int32 side;

            int32 firstedge;
            int32 numedges;
            int32 texinfo;

            uint8 styles[bspformat29::MAXLIGHTMAPS];
            int32 lightofs;
        };

        struct FileLeaf
        {
            ELeafContentType contents;
            int32            visofs;

            int16            mins[3];
            int16            maxs[3];

            int32            firstmarksurface;
            int32            nummarksurfaces;

            char             ambient_level[4];
        };

        struct FileNode
        {
            int32 planenum;
            int32 children[2];
            int16 mins[3];
            int16 maxs[3];
            int32 firstface;
            int32 numfaces;
        };

        // BSP2 model lump uses 32-bit indices.
        struct FileModel
        {
            float mins[3];
            float maxs[3];
            float origin[3];
            int32 headnode[4];
            int32 visleafs;
            int32 firstface;
            int32 numfaces;
        };
    }

    class BspLoader
    {
    public:

        BspLoader();
        ~BspLoader();

        void Load(const uint8* data, int64 dataSize);
        const bspformat29::Bsp_29* GetBspPtr() const { return m_bsp29; }

    private:

        bspformat29::Bsp_29* m_bsp29;

        const uint8* m_dataStart = nullptr;
        int64 m_dataSize = 0;

        template<typename T>
        bool DeserializeLump(const uint8*& data, const bspformat29::Lump& lump, TArray<T>& out);

        void LoadTextures(const uint8*& data, const bspformat29::Lump& lump);
        void LoadEntities(const uint8*& data, const bspformat29::Lump& lump);
    };

    template<typename T>
    bool BspLoader::DeserializeLump(const uint8*& data, const bspformat29::Lump& lump, TArray<T>& out)
    {
        const int64 Pos = int64(lump.position);
        const int64 Len = int64(lump.length);
        if (Pos < 0 || Len < 0 || Pos + Len > m_dataSize)
        {
            UE_LOG(LogTemp, Warning, TEXT("BSP Import: Lump out of bounds (pos=%d len=%d size=%lld)"), lump.position, lump.length, m_dataSize);
            return false;
        }

        if ((Len % int64(sizeof(T))) != 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("BSP Import: Lump size mismatch (len=%d elem=%d)"), lump.length, int32(sizeof(T)));
            return false;
        }

        const int64 Count64 = Len / int64(sizeof(T));
        if (Count64 < 0 || Count64 > int64(MAX_int32))
        {
            UE_LOG(LogTemp, Warning, TEXT("BSP Import: Lump element count invalid (%lld)"), Count64);
            return false;
        }

        const int32 Count = int32(Count64);
        out.Reset(Count);
        out.SetNumUninitialized(Count);
        FMemory::Memcpy(out.GetData(), data + Pos, size_t(Len));
        return true;
    }

    // UNREALED Import functions

    struct FLightmapAtlasFace
    {
        int32 X = 0;
        int32 Y = 0;
        int32 W = 0;
        int32 H = 0;
        int32 TexMinS = 0;
        int32 TexMinT = 0;
    };

    struct FLightmapAtlas
    {
        int32 AtlasW = 0;
        int32 AtlasH = 0;
        TMap<int32, FLightmapAtlasFace> FaceToAtlas;
        FString LightmapTextureObjectPath;
    };

    bool BuildLightmapAtlas(const bspformat29::Bsp_29& Model, const FString& LightmapsPath, const FString& MapName, const FString& LitFilePath, bool bOverwrite, FLightmapAtlas& OutAtlas);

    
    // From a Quake BSP model, import submodels to individual staticmeshes.
    // If bChunkWorld is true, submodel_0 (world) is split into multiple meshes.
    // Chunking can be grid based (WorldChunkSize) or leaf based (when WorldChunkSize is ignored).
    // OutWorldMeshObjectPaths will be filled with object paths for the created world chunks (or submodel_0 if not chunked).
    void ModelToStaticmeshes(const bspformat29::Bsp_29& model, const FString& MeshesPath, const FString& MapName, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>& MaskedTextureNames, bool bChunkWorld, int32 WorldChunkSize, float ImportScale, bool bIncludeSky, bool bIncludeWater, const FName& BspCollisionProfile, const FName& MaskedCollisionProfile, const FName& WaterCollisionProfile, const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths, TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths, const FLightmapAtlas* LightmapAtlas);

    bool CreateSubmodelStaticMesh(const bspformat29::Bsp_29& model, const FString& MeshesPath, const FString& MeshAssetName, uint8 SubModelId, const TMap<FString, UMaterialInterface*>& MaterialsByName, const TSet<FString>& MaskedTextureNames, float ImportScale, const FName& DefaultCollisionProfile, const FName& MaskedCollisionProfile, FString& OutObjectPath, const FLightmapAtlas* LightmapAtlas);

    // Append texture pixel data to array
    bool AppendNextTextureData(const FString& name, const int frame, const bspformat29::Bsp_29& model, TArray<uint8>& data);

} // namespace bsputils
