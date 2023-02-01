#include "pch.h"
#include "Block.h"
#include "Face.h"

dae::Block::Block(const Vector3 position)
	: m_TranslationMatrix{ Matrix::CreateTranslation(position) }
{
	for (int i{}; i <= static_cast<int>(Face::FaceDirection::BOTTOM); ++i)
	{
		switch (static_cast<Face::FaceDirection>(i))
		{
		case Face::FaceDirection::FORWARD:
		{
			m_FaceTransformations[i] = Matrix::CreateTranslation({ 0.0f, 0.0f, -0.5f });
			break;
		}
		case Face::FaceDirection::RIGHT:
		{
			m_FaceTransformations[i] = Matrix::CreateRotationY(90.0f * TO_RADIANS) * Matrix::CreateTranslation({ -0.5f, 0.0f, 0.0f });
			break;
		}
		case Face::FaceDirection::LEFT:
		{
			m_FaceTransformations[i] = Matrix::CreateRotationY(-90.0f * TO_RADIANS) * Matrix::CreateTranslation({ 0.5f, 0.0f, 0.0f });
			break;
		}
		case Face::FaceDirection::BACK:
		{
			m_FaceTransformations[i] = Matrix::CreateRotationY(180.0f * TO_RADIANS) * Matrix::CreateTranslation({ 0.0f, 0.0f, 0.5f });
			break;
		}
		case Face::FaceDirection::UP:
		{
			m_FaceTransformations[i] = Matrix::CreateRotationX(-90.0f * TO_RADIANS) * Matrix::CreateTranslation({ 0.0f, 0.5f, 0.0f });
			break;
		}
		case Face::FaceDirection::BOTTOM:
		{
			m_FaceTransformations[i] = Matrix::CreateRotationX(90.0f * TO_RADIANS) * Matrix::CreateTranslation({ 0.0f, -0.5f, 0.0f });
			break;
		}
		}
	}
}

void dae::Block::Render(ID3D11DeviceContext* pDeviceContext, const Matrix& viewProjection, Face* pFace) const
{
	const Matrix worldViewProjection{ m_TranslationMatrix * viewProjection };

	for (int i{}; i <= static_cast<int>(Face::FaceDirection::BOTTOM); ++i)
	{
		const Matrix& faceTransformation{ m_FaceTransformations[i] };

		pFace->Render(pDeviceContext, faceTransformation * m_TranslationMatrix, faceTransformation * worldViewProjection);
	}
}