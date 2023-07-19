#pragma once
#include "../include.h"
#include "PixelFormat.h"
#include "ShaderFunction.h"
#include "VertexDescriptor.h"
#include "DepthStencil.h"
#include "BlendState.h"
#include <vector>

namespace FV
{
	struct RenderPipelineColorAttachmentDescriptor
	{
		uint32_t index;
		PixelFormat pixelFormat;
		BlendState blendState;
	};

	enum class PrimitiveType
	{
		Point,
		Line,
		LineStrip,
		Triangle,
		TriangleStrip,
	};

	enum class IndexType
	{
		UInt16,
		UInt32,
	};

	enum class TriangleFillMode
	{
		Fill,
		Lines,
	};

	enum class CullMode
	{
		None,
		Front,
		Back,
	};

	enum class Winding
	{
		Clockwise,
		CounterClockwise,
	};

	enum class DepthClipMode
	{
		Clip,
		Clamp,
	};

	struct RenderPipelineDescriptor
	{
		std::shared_ptr<ShaderFunction> vertexFunction;
		std::shared_ptr<ShaderFunction> fragmentFunction;
		VertexDescriptor vertexDescriptor;
		std::vector<RenderPipelineColorAttachmentDescriptor> colorAttachments;
		PixelFormat depthStencilAttachmentPixelFormat;

		PrimitiveType primitiveTopology;

		TriangleFillMode triangleFillMode = TriangleFillMode::Fill;
		bool rasterizationEnabled = true;
	};

	class GraphicsDevice;
	class RenderPipelineState
	{
	public:
		virtual ~RenderPipelineState() {}
		virtual std::shared_ptr<GraphicsDevice> device() const = 0;
	};
}
