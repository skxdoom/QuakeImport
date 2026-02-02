// Fill out your copyright notice in the Description page of Project Settings.

// QuakeImport
#include "BspUtilities.h"
#include "QuakeCommon.h"

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

        if (!m_dataStart || m_dataSize < int64(sizeof(bspformat29::Header)))
        {
            return;
        }

        bspformat29::Header header;

        QuakeCommon::ReadData<bspformat29::Header>(m_dataStart, 0, header);

        if (header.version != bspformat29::HEADER_VERSION_29)
        {
            return;
        }

        m_bsp29 = new bspformat29::Bsp_29();

        // Validate lump bounds before deserializing to avoid crashes on malformed/unsupported BSPs.
        for (int LumpIndex = 0; LumpIndex < bspformat29::HEADER_LUMP_SIZE; LumpIndex++)
        {
            const bspformat29::Lump& L = header.lumps[LumpIndex];
            const int64 Pos = int64(L.position);
            const int64 Len = int64(L.length);
            if (Pos < 0 || Len < 0 || Pos + Len > m_dataSize)
            {
                UE_LOG(LogTemp, Warning, TEXT("BSP Import: Lump %d out of bounds (pos=%d len=%d size=%lld)"), LumpIndex, L.position, L.length, m_dataSize);
                return;
            }
        }

        DeserializeLump<bspformat29::Point3f>(m_dataStart, header.lumps[bspformat29::LUMP_VERTEXES], m_bsp29->vertices);
        DeserializeLump<bspformat29::Edge>(data, header.lumps[bspformat29::LUMP_EDGES], m_bsp29->edges);
        DeserializeLump<bspformat29::Surfedge>(data, header.lumps[bspformat29::LUMP_SURFEDGES], m_bsp29->surfedges);
        DeserializeLump<bspformat29::Face>(data, header.lumps[bspformat29::LUMP_FACES], m_bsp29->faces);
        DeserializeLump<uint8>(data, header.lumps[bspformat29::LUMP_LIGHTING], m_bsp29->lightdata);
        DeserializeLump<bspformat29::Plane>(data, header.lumps[bspformat29::LUMP_PLANES], m_bsp29->planes);
        DeserializeLump<bspformat29::Marksurface>(data, header.lumps[bspformat29::LUMP_MARKSURFACES], m_bsp29->marksurfaces);
        DeserializeLump<bspformat29::Leaf>(data, header.lumps[bspformat29::LUMP_LEAFS], m_bsp29->leaves);
        DeserializeLump<bspformat29::Node>(data, header.lumps[bspformat29::LUMP_NODES], m_bsp29->nodes);
        DeserializeLump<bspformat29::SubModel>(data, header.lumps[bspformat29::LUMP_MODELS], m_bsp29->submodels);
        DeserializeLump<bspformat29::TexInfo>(data, header.lumps[bspformat29::LUMP_TEXINFO], m_bsp29->texinfos);
        DeserializeLump<uint8>(data, header.lumps[bspformat29::LUMP_VISIBILITY], m_bsp29->visdata);

        LoadTextures(data, header.lumps[bspformat29::LUMP_TEXTURES]);
        LoadEntities(data, header.lumps[bspformat29::LUMP_ENTITIES]);
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
    }

    struct FWorldChunkBuild
    {
        FRawMesh RawMesh;
        TMap<int32, int32> BspVertexToLocal;
        TArray<int32> SlotToTextureId;
        TMap<int32, int32> TextureIdToSlot;
    };

    static FIntPoint GetChunkKey2D(const FVector3f& Center, int32 ChunkSize)
    {
        if (ChunkSize <= 0)
        {
            return FIntPoint(0, 0);
        }

        const int32 X = FMath::FloorToInt(float(Center.X) / float(ChunkSize));
        const int32 Y = FMath::FloorToInt(float(Center.Y) / float(ChunkSize));
        return FIntPoint(X, Y);
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

    static void BuildStaticMesh(UStaticMesh* StaticMesh, const bspformat29::Bsp_29& Model, const TMap<FString, UMaterialInterface*>& MaterialsByName, const FWorldChunkBuild& Chunk, int32 LightmapSize, bool bEnableCollision)
    {
        if (!StaticMesh)
        {
            return;
        }

        for (int32 Slot = 0; Slot < Chunk.SlotToTextureId.Num(); Slot++)
        {
            const int32 TextureId = Chunk.SlotToTextureId[Slot];
            const FString& MatName = Model.textures[TextureId].name;
            const FString SafeSlotName = SanitizeSurfaceNameForAsset(MatName);

            UMaterialInterface* Material = nullptr;

            if (const UMaterialInterface* const* Found = MaterialsByName.Find(MatName))
            {
                Material = const_cast<UMaterialInterface*>(*Found);
            }

            if (!Material)
            {
                Material = UMaterial::GetDefaultMaterial(MD_Surface);
            }

            StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, FName(*SafeSlotName), FName(*SafeSlotName)));
        }

        FStaticMeshSourceModel* SrcModel = &StaticMesh->AddSourceModel();
        SrcModel->BuildSettings.MinLightmapResolution = LightmapSize;
        SrcModel->BuildSettings.SrcLightmapIndex = 0;
        SrcModel->BuildSettings.DstLightmapIndex = 1;
        SrcModel->BuildSettings.bGenerateLightmapUVs = true;
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
            if (bEnableCollision)
            {
                BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
                BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
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

    static void CreateWorldChunks(const FString& MeshesPath, const bspformat29::Bsp_29& Model, const TMap<FString, UMaterialInterface*>& MaterialsByName, int32 ChunkSize, float ImportScale, TArray<FString>* OutWorldMeshObjectPaths)
    {
        using namespace bsputils;

        struct FFaceTemp
        {
            int32 NumTris = 0;
            FVector Normal;
            TArray<int32> BspVertexIds;
            int32 TexInfo = 0;
            TArray<FVector2f> TexCoords;
            FVector3f Center;
        };

        struct FChunkPair
        {
            FWorldChunkBuild Opaque;
            FWorldChunkBuild Transparent;
        };

        TMap<FIntPoint, FChunkPair> ChunkMap;

        const int32 FirstFace = Model.submodels[0].firstface;
        const int32 FaceCount = Model.submodels[0].numfaces;

        for (int32 F = FirstFace; F < FirstFace + FaceCount; F++)
        {
            const bspformat29::Face& Face = Model.faces[F];
            const bspformat29::TexInfo& Ti = Model.texinfos[Face.texinfo];
            const bspformat29::Texture& Tex = Model.textures[Ti.miptex];

            if (Tex.name.StartsWith(TEXT("sky")))
            {
                continue;
            }

            const bool bTransparent = IsTransparentSurfaceName(Tex.name);

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

                TexCoord.X = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[0][0], Ti.vecs[0][1], Ti.vecs[0][2])) + Ti.vecs[0][3];
                TexCoord.Y = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[1][0], Ti.vecs[1][1], Ti.vecs[1][2])) + Ti.vecs[1][3];

                TexCoord.X /= Tex.width;
                TexCoord.Y /= Tex.height;

                Temp.TexCoords.Add(TexCoord);
            }

            Temp.Center = Sum / float(Temp.BspVertexIds.Num());

            const FIntPoint Key = GetChunkKey2D(Temp.Center, ChunkSize);
            FChunkPair& Pair = ChunkMap.FindOrAdd(Key);
            FWorldChunkBuild& Chunk = bTransparent ? Pair.Transparent : Pair.Opaque;

            for (int32 J = 0; J < Temp.NumTris; J++)
            {
                const int32 A = 0;
                const int32 B = J + 1;
                const int32 C = J + 2;

                const uint32 VA = GetOrAddLocalVertex(Chunk, Model, Temp.BspVertexIds[A], ImportScale);
                const uint32 VB = GetOrAddLocalVertex(Chunk, Model, Temp.BspVertexIds[B], ImportScale);
                const uint32 VC = GetOrAddLocalVertex(Chunk, Model, Temp.BspVertexIds[C], ImportScale);

                const FVector3f N(Temp.Normal.X, Temp.Normal.Y, Temp.Normal.Z);

                AddWedgeEntry(Chunk.RawMesh, VA, N, Temp.TexCoords[A], Temp.TexCoords[A]);
                AddWedgeEntry(Chunk.RawMesh, VB, N, Temp.TexCoords[B], Temp.TexCoords[B]);
                AddWedgeEntry(Chunk.RawMesh, VC, N, Temp.TexCoords[C], Temp.TexCoords[C]);

                const int32 TextureId = Model.texinfos[Temp.TexInfo].miptex;
                const int32 Slot = GetOrAddMaterialSlot(Chunk, TextureId);
                Chunk.RawMesh.FaceMaterialIndices.Add(Slot);
                Chunk.RawMesh.FaceSmoothingMasks.Add(0);
            }
        }

        const int32 LightmapSize = 128;

        for (const auto& PairIt : ChunkMap)
        {
            const FIntPoint Key = PairIt.Key;
            const FChunkPair& Pair = PairIt.Value;

            if (Pair.Opaque.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_chunk_%d_%d"), Key.X, Key.Y);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, Pair.Opaque, LightmapSize, true);

                if (OutWorldMeshObjectPaths)
                {
                    OutWorldMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }

            if (Pair.Transparent.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_chunk_%d_%d_Trans"), Key.X, Key.Y);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, Pair.Transparent, LightmapSize, false);

                if (OutWorldMeshObjectPaths)
                {
                    OutWorldMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }
        }
    }

    static void CreateLeafChunks(const FString& MeshesPath, const bspformat29::Bsp_29& Model, const TMap<FString, UMaterialInterface*>& MaterialsByName, float ImportScale, TArray<FString>* OutWorldMeshObjectPaths)
    {
        using namespace bsputils;

        struct FLeafPair
        {
            FWorldChunkBuild Opaque;
            FWorldChunkBuild Transparent;
        };

        TMap<int32, FLeafPair> LeafToChunk;

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
            for (uint32 I = 0; I < Leaf.nummarksurfaces; I++)
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
                if (Tex.name.StartsWith(TEXT("sky")))
                {
                    continue;
                }

                const bool bTransparent = IsTransparentSurfaceName(Tex.name);
                FWorldChunkBuild& Chunk = bTransparent ? Pair.Transparent : Pair.Opaque;

                FVector Normal;
                for (int32 N = 0; N < 3; N++)
                {
                    Normal[N] = Model.planes[Face.planenum].normal[N];
                }

                TArray<int32> BspVertexIds;
                TArray<FVector2f> TexCoords;

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
                    TexCoord.X = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[0][0], Ti.vecs[0][1], Ti.vecs[0][2])) + Ti.vecs[0][3];
                    TexCoord.Y = FVector3f::DotProduct(Unflipped, FVector3f(Ti.vecs[1][0], Ti.vecs[1][1], Ti.vecs[1][2])) + Ti.vecs[1][3];
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
                    AddWedgeEntry(Chunk.RawMesh, VA, N, TexCoords[A], TexCoords[A]);
                    AddWedgeEntry(Chunk.RawMesh, VB, N, TexCoords[B], TexCoords[B]);
                    AddWedgeEntry(Chunk.RawMesh, VC, N, TexCoords[C], TexCoords[C]);

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
                const FString ChunkName = FString::Printf(TEXT("SM_leaf_%d"), LeafIndex);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, Pair.Opaque, LightmapSize, true);

                if (OutWorldMeshObjectPaths)
                {
                    OutWorldMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }

            if (Pair.Transparent.RawMesh.WedgeIndices.Num() > 0)
            {
                const FString ChunkName = FString::Printf(TEXT("SM_leaf_%d_Trans"), LeafIndex);
                const FString LongPkg = MeshesPath / ChunkName;
                UPackage* Pkg = CreateAssetPackage(LongPkg);
                UStaticMesh* StaticMesh = GetOrCreateStaticMesh(*Pkg, ChunkName);
                BuildStaticMesh(StaticMesh, Model, MaterialsByName, Pair.Transparent, LightmapSize, false);

                if (OutWorldMeshObjectPaths)
                {
                    OutWorldMeshObjectPaths->Add(StaticMesh->GetPathName());
                }
            }
        }
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

        for (
            int f = model.submodels[id].firstface;
            f < (model.submodels[id].numfaces + model.submodels[id].firstface);
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

                short vertex_id = edge.first;

                if (surfedge.index < 0)
                {
                    vertex_id = edge.second;
                }

                triface.points.Add(vertex_id);

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
                    material = UMaterial::GetDefaultMaterial(MD_Surface);
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

    void ModelToStaticmeshes(const bspformat29::Bsp_29& model, const FString& MeshesPath, const TMap<FString, UMaterialInterface*>& MaterialsByName, bool bChunkWorld, int32 WorldChunkSize, float ImportScale, TArray<FString>* OutWorldMeshObjectPaths)
    {
        if (bChunkWorld)
        {
            CreateWorldChunks(MeshesPath, model, MaterialsByName, WorldChunkSize, ImportScale, OutWorldMeshObjectPaths);
            return;
        }

        CreateLeafChunks(MeshesPath, model, MaterialsByName, ImportScale, OutWorldMeshObjectPaths);
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
