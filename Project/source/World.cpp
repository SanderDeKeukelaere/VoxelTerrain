#include "pch.h"
#include "World.h"
#include "Vector3Int.h"
#include "Block.h"
#include "TextureManager.h"
#include "Utils.h"

dae::World::World()
{
	for (int x{}; x < m_MapSize; ++x)
	{
		for (int z{}; z < m_MapSize; ++z)
		{
			float worldLevel{};
			float maxValue{};
			for (int i{ 1 }; i <= m_NrOctaves; ++i)
			{
				maxValue += 1.0f / i;
				worldLevel += Utils::PerlinFunction(static_cast<float>(x) / m_MapSize, static_cast<float>(z) / m_MapSize, i, m_MapZoom, m_MapOffset) / i;
			}
			worldLevel /= (maxValue / 2.0f);
			worldLevel /= 2.0f;
			worldLevel += 0.5f;
			worldLevel *= m_MapHeight;

			constexpr int seaLevel{ 30 };
			for (int y{ 0 }; y < m_MapHeight; ++y)
			{
				if (y > worldLevel && y > seaLevel) continue;

				Texture* pTexture{};

				if (y == static_cast<int>(worldLevel))
				{
					pTexture = TextureManager::GetInstance()->GetTexture(TextureManager::TextureType::GRASS);
				}
				else if (y > worldLevel)
				{
					pTexture = TextureManager::GetInstance()->GetTexture(TextureManager::TextureType::WATER);
				}
				else
				{
					pTexture = TextureManager::GetInstance()->GetTexture(TextureManager::TextureType::DIRT);
				}

				m_pBlocks[x + z * m_MapSize + y * m_MapSize * m_MapSize] = new Block{ { x, y, z }, pTexture };
			}
		}
	}

	m_IsBlockPredicate = [&](const Vector3Int& position) -> bool
	{
		return position.x >= 0 && position.z >= 0 &&
			position.x < m_MapSize&& position.z < m_MapSize&&
			position.y < m_MapHeight&& position.y >= 0 &&
			m_pBlocks[position.x + position.z * m_MapSize + position.y * m_MapSize * m_MapSize];
	};
}

dae::World::~World()
{
	for (Block* pBlock : m_pBlocks)
	{
		delete pBlock;
	}
}

void dae::World::Render(ID3D11DeviceContext* pDeviceContext, const Matrix& viewProjection, Face* pFace) const
{
	for (int x{}; x < m_MapSize; ++x)
	{
		for (int z{}; z < m_MapSize; ++z)
		{
			for (int y{}; y < m_MapHeight; ++y)
			{
				Block* pBlock{ m_pBlocks[x + z * m_MapSize + y * m_MapSize * m_MapSize] };

				if (!pBlock) continue;

				const Vector3Int position{ x, y, z };

				if (m_IsBlockPredicate(position + Vector3Int::UnitX) && m_IsBlockPredicate(position - Vector3Int::UnitX) &&
					m_IsBlockPredicate(position + Vector3Int::UnitY) && m_IsBlockPredicate(position - Vector3Int::UnitY) &&
					m_IsBlockPredicate(position + Vector3Int::UnitZ) && m_IsBlockPredicate(position - Vector3Int::UnitZ))
					continue;

				pBlock->Render(pDeviceContext, m_IsBlockPredicate, viewProjection, pFace);
			}
		}
	}
}
