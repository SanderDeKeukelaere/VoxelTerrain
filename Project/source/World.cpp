#include "pch.h"
#include "World.h"
#include "Vector3Int.h"
#include "Block.h"
#include "TextureManager.h"
#include "Utils.h"
#include "Vector2Int.h"
#include "BlockManager.h"

dae::World::World()
{
	m_IsBlockPredicate = [&](const Vector3Int& position) -> bool
	{
		const Vector2Int chunkPos{ position.x / m_MapSize, position.z / m_MapSize };
		
		auto it{ m_Chunks.find(chunkPos) };
		if (it == m_Chunks.end()) return false;

		Chunk* chunk{ it->second };

		const Vector3Int lookUpPos{ position.x - chunkPos.x * m_MapSize, position.y, position.z - chunkPos.y * m_MapSize };

		return lookUpPos.y < m_MapHeight && lookUpPos.y >= 0 &&
			chunk->pBlocks[lookUpPos.x + lookUpPos.z * m_MapSize + lookUpPos.y * m_MapSize * m_MapSize];
	};
}

dae::World::~World()
{
	for (auto& chunkPair : m_Chunks)
	{
		delete chunkPair.second;
	}
}

void dae::World::Update(const Vector3& camPosition)
{
	const Vector2Int camChunk{ static_cast<int>(camPosition.x / m_MapSize), static_cast<int>(camPosition.z / m_MapSize) };

	const int destroyRange{ m_RenderDistance + 2 };
	const int createRange{ m_RenderDistance + 1 };

	for (int x{ -destroyRange }; x <= destroyRange; ++x)
	{
		for (int z{ -destroyRange }; z <= destroyRange; ++z)
		{
			if (abs(x) <= createRange && abs(z) <= createRange)
			{
				const Vector2Int chunkPos{ camChunk + Vector2Int{ x,z } };

				auto it = m_Chunks.find(chunkPos);
				if (it != m_Chunks.end())
				{
					it->second->canRender = abs(x) <= m_RenderDistance && abs(z) <= m_RenderDistance;
				}
				else
				{
					LoadChunk(chunkPos);
				}
			}
			else
			{
				auto it = m_Chunks.find(camChunk + Vector2Int{ x,z });
				if (it == m_Chunks.end()) continue;

				Chunk* chunkToErase{ (*it).second };
				m_Chunks.erase(it);
				delete chunkToErase;
			}
		}
	}

	for (int x{ -m_RenderDistance }; x <= m_RenderDistance; ++x)
	{
		for (int z{ -m_RenderDistance }; z <= m_RenderDistance; ++z)
		{
			LoadTrees(camChunk.x + x, camChunk.y + z);
		}
	}
}

void dae::World::Render(ID3D11DeviceContext* pDeviceContext, const Matrix& viewProjection, Face* pFace) const
{
	for (auto& chunkPair : m_Chunks)
	{
		Chunk* chunk{ chunkPair.second };

		if (!chunk || !chunk->canRender) continue;

		for (int x{}; x < m_MapSize; ++x)
		{
			for (int z{}; z < m_MapSize; ++z)
			{
				for (int y{ m_MapHeight - 1 }; y >= m_SeaLevel - 1; --y)
				{
					Block* pBlock{ chunk->pBlocks[x + z * m_MapSize + y * m_MapSize * m_MapSize] };

					if (!pBlock) continue;

					const Vector3Int position{ chunkPair.first.x * m_MapSize + x, y, chunkPair.first.y * m_MapSize + z };

					if (m_IsBlockPredicate(position + Vector3Int::UnitX) && m_IsBlockPredicate(position - Vector3Int::UnitX) &&
						m_IsBlockPredicate(position + Vector3Int::UnitY) && m_IsBlockPredicate(position - Vector3Int::UnitY) &&
						m_IsBlockPredicate(position + Vector3Int::UnitZ) && m_IsBlockPredicate(position - Vector3Int::UnitZ))
					{
						if (position.y < chunk->heightMap[x + z * m_MapSize]) break;
						continue;
					}

					pBlock->Render(pDeviceContext, m_IsBlockPredicate, position, viewProjection, pFace);
				}
			}
		}
	}
}

void dae::World::LoadChunk(const Vector2Int& chunkPos)
{
	Chunk* pChunk{ new Chunk{} };

	for (int x{}; x < m_MapSize; ++x)
	{
		for (int z{}; z < m_MapSize; ++z)
		{
			const int worldPosX{ chunkPos.x * m_MapSize + x };
			const int worldPosZ{ chunkPos.y * m_MapSize + z };

			float worldLevel{};
			float maxValue{};
			for (int i{ 1 }; i <= m_NrOctaves; ++i)
			{
				maxValue += 1.0f / i;
				worldLevel += Utils::PerlinFunction(static_cast<float>(worldPosX) / m_MapSize, static_cast<float>(worldPosZ) / m_MapSize, i, m_MapZoom, m_MapOffset) / i;
			}
			worldLevel /= (maxValue / 2.0f);
			worldLevel /= 2.0f;
			worldLevel += 0.5f;
			worldLevel *= m_MapHeight;

			bool hasWater{};

			pChunk->heightMap[x + z * m_MapSize] = worldLevel > m_SeaLevel ? static_cast<int>(worldLevel) : m_SeaLevel;

			const BlockType biomeBlock{ GetBiomeBlock(worldPosX, worldPosZ) };

			for (int y{ m_MapHeight }; y >= 0; --y)
			{
				if (y > worldLevel && y > m_SeaLevel) continue;

				BlockType blockType{};

				/*if (y > snowLevel)
				{
					textureType = TextureManager::BlockType::SNOW;
				}
				else*/
				if (y == static_cast<int>(worldLevel))
				{
					if (hasWater)
					{
						blockType = BlockType::SAND;
					}
					else
					{
						if (y < m_SeaLevel + m_BeachSize)
						{
							blockType = BlockType::SAND;
						}
						else
						{
							blockType = biomeBlock;
						}
					}

				}
				else if (y > worldLevel)
				{
					blockType = BlockType::WATER;

					hasWater = true;
				}
				else if(y < m_SeaLevel + m_BeachSize)
				{
					blockType = BlockType::SAND;
				}
				else
				{
					if (biomeBlock == BlockType::GRASS)
					{
						blockType = BlockType::DIRT;
					}
					else
					{
						blockType = biomeBlock;
					}
				}

				Block* pBlock{ BlockManager::GetInstance()->GetBlock(blockType) };

				pChunk->pBlocks[x + z * m_MapSize + y * m_MapSize * m_MapSize] = pBlock;
			}
		}
	}

	m_Chunks[chunkPos] = pChunk;
}


BlockType dae::World::GetBiomeBlock(int x, int z)
{
	float temperature{};
	float humidity{};

	constexpr float biomeZoom{ 190.0f };
	constexpr float temperatureOffset{ 25.0f};
	constexpr float humidityOffset{ 75.0f };

	float maxValue{};
	for (int i{ 1 }; i <= m_NrOctaves; ++i)
	{
		maxValue += 1.0f / i;
		temperature += Utils::PerlinFunction(static_cast<float>(x) / m_MapSize, static_cast<float>(z) / m_MapSize, i, biomeZoom, temperatureOffset) / i;
		humidity += Utils::PerlinFunction(static_cast<float>(x) / m_MapSize, static_cast<float>(z) / m_MapSize, i, biomeZoom, humidityOffset) / i;
	}

	temperature /= (maxValue / 2.0f);
	humidity /= (maxValue / 2.0f);

	temperature /= 2.0f;
	humidity /= 2.0f;

	temperature += 0.5f;
	humidity += 0.5f;

	if (temperature > 0.5f)
	{
		if (humidity > 0.5f)
		{
			return BlockType::GRASS;
		}
		else
		{
			return BlockType::SAND;
		}
	}
	else
	{
		if (humidity > 0.5f)
		{
			return BlockType::GRASS;
		}
		else
		{
			return BlockType::SNOW;
		}
	}
}

void dae::World::LoadTrees(int chunkX, int chunkY)
{
	Chunk* pChunk{ m_Chunks[Vector2Int{ chunkX, chunkY }] };

	if (pChunk->hasTrees) return;

	pChunk->hasTrees = true;

	for (int x{}; x < m_MapSize; ++x)
	{
		for (int z{}; z < m_MapSize; ++z)
		{
			const int blockWorldPosX{ x + chunkX * m_MapSize };
			const int blockWorldPosZ{ z + chunkY * m_MapSize };

			if (!HasTree(Vector2Int{ blockWorldPosX,  blockWorldPosZ })) continue;

			const int y{ pChunk->heightMap[x + z * m_MapSize] };

			if (y <= m_SeaLevel + m_BeachSize) continue;

			switch (GetBiomeBlock(blockWorldPosX, blockWorldPosZ))
			{
			case BlockType::GRASS:
				CreateTree(Vector3Int{ blockWorldPosX, y + 1, blockWorldPosZ }, pChunk);
				break;
			case BlockType::SAND:
				CreateCactus(Vector3Int{ blockWorldPosX, y + 1, blockWorldPosZ }, pChunk);
				break;
			}

			break;
		}
	}
}

bool dae::World::HasTree(const Vector2Int& position)
{
	constexpr float treeZoom{ 0.01f };

	float worldLevel{};
	float maxValue{};
	for (int i{ 1 }; i <= m_NrOctaves; ++i)
	{
		maxValue += 1.0f / i;
		worldLevel += Utils::PerlinFunction(static_cast<float>(position.x) / m_MapSize, static_cast<float>(position.y) / m_MapSize, i, treeZoom, m_MapOffset) / i;
	}
	worldLevel /= (maxValue / 2.0f);

	return worldLevel / 2.0f + 0.5f > 0.7f;
}

void dae::World::CreateTree(const Vector3Int& position, Chunk* pChunk)
{
	Vector2Int chunkPos{ position.x / m_MapSize, position.z / m_MapSize };
	Vector3Int blockInChunkPos{ position.x - chunkPos.x * m_MapSize, position.y, position.z - chunkPos.y * m_MapSize };

	Block* pWoodBlock{ BlockManager::GetInstance()->GetBlock(BlockType::WOOD) };

	for (int i{}; i < 5; ++i)
	{
		if (blockInChunkPos.y + i < m_MapHeight)
			AddBlock(position.x, position.y + i, position.z, pWoodBlock);
	}

	Block* pLeavesBlock{ BlockManager::GetInstance()->GetBlock(BlockType::LEAVES) };

	for (int x{ -2 }; x <= 2; ++x)
	{
		for (int z{ -2 }; z <= 2; ++z)
		{
			if (position.y + 3 < m_MapHeight)
				AddBlock(position.x + x, position.y + 3, position.z + z, pLeavesBlock);
		
			if (position.y + 4 < m_MapHeight)
				AddBlock(position.x + x, position.y + 4, position.z + z, pLeavesBlock);
		}
	}

	for (int x{ -1 }; x <= 1; ++x)
	{
		for (int z{ -1 }; z <= 1; ++z)
		{
			if(blockInChunkPos.y + 5 < m_MapHeight)
				AddBlock(position.x + x, position.y + 5, position.z + z, pLeavesBlock);

			if (abs(x) && abs(z)) continue;

			if (blockInChunkPos.y + 6 < m_MapHeight)
				AddBlock(position.x + x, position.y + 6, position.z + z, pLeavesBlock);
		}
	}
}

void dae::World::CreateCactus(const Vector3Int& position, Chunk* pChunk)
{
	Vector2Int chunkPos{ position.x / m_MapSize, position.z / m_MapSize };
	Vector3Int blockInChunkPos{ position.x - chunkPos.x * m_MapSize, position.y, position.z - chunkPos.y * m_MapSize };

	Block* pBlock{ BlockManager::GetInstance()->GetBlock(BlockType::CACTUS) };

	for (int i{}; i < 3; ++i)
	{
		const int blockIdx{ blockInChunkPos.x + blockInChunkPos.z * m_MapSize + (position.y + i) * m_MapSize * m_MapSize };

		if (blockInChunkPos.y + i < m_MapHeight)
			if (!pChunk->pBlocks[blockIdx])
				pChunk->pBlocks[blockIdx] = pBlock;
	}
}

void dae::World::AddBlock(int x, int y, int z, Block* pBlock)
{
	const Vector2Int chunkPos{ x / m_MapSize, z / m_MapSize };
	const Vector3Int positionInChunk{ x - chunkPos.x * m_MapSize, y, z - chunkPos.y * m_MapSize };

	Chunk* pChunk{ m_Chunks[chunkPos] };

	if (!pChunk) return;

	const int blockIdx{ positionInChunk.x + positionInChunk.z * m_MapSize + y * m_MapSize * m_MapSize };

	if (!pChunk->pBlocks[blockIdx])
		pChunk->pBlocks[blockIdx] = pBlock;
}
