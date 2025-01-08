#include "Writer.h"
#include "ShapeWriter.h"
#include "Module/Module.h"

#include <CDT.h>

#include "core/hashing/ncrypto/xxhash.h"
#include "core/hashing/hash.h"
#include "core/stb/stb.h"

using namespace Animate::Publisher;

namespace sc {
	namespace Adobe {

		static inline void bl_assert(BLResult result)
		{
			if (result != BL_SUCCESS)
			{
				BLResultCode code = (BLResultCode)result;
				assert(code == BL_SUCCESS);
			}
		}

		void SCShapeWriter::AddGraphic(const Animate::Publisher::BitmapElement& item) {
			wk::RawImageRef image = m_writer.GetBitmap(item);

			m_group.AddElement<BitmapItem>(m_symbol, image, item.Transformation());
		}

		void SCShapeWriter::AddFilledElement(const Animate::Publisher::FilledElement& shape) {
			for (const auto& region : shape.fill) {
				AddFilledShapeRegion(region, shape.Transformation());
			}

			for (const auto& region : shape.stroke) {
				AddFilledShapeRegion(region, shape.Transformation());
			}
		}

		void SCShapeWriter::AddTriangulatedRegion(
			const Animate::Publisher::FilledElementPath& contour,
			const std::vector<Animate::Publisher::FilledElementPath>& holes,
			const Animate::DOM::Utils::COLOR& color)
		{
			CDT::Triangulation<float> cdt;

			std::vector<CDT::V2d<float>> vertices;
			std::vector<CDT::Edge> edges;

			{
				std::vector<Animate::Publisher::Point2D> points;
				contour.Rasterize(points);

				for (Animate::Publisher::Point2D& point : points)
				{
					vertices.push_back({ point.x, point.y });
				}
			}

			// Contour
			for (uint32_t i = 0; vertices.size() > i; i++) {
				uint32_t secondIndex = i + 1;
				if (secondIndex >= vertices.size()) {
					secondIndex = 0;
				}
				edges.push_back(CDT::Edge(i, secondIndex));
			}

			// Holes
			for (const auto& hole : holes) {
				std::vector<Animate::Publisher::Point2D> points;
				hole.Rasterize(points);

				for (uint32_t i = 0; points.size() > i; i++) {
					uint32_t secondIndex = vertices.size() + i + 1;
					if (secondIndex >= points.size() + vertices.size()) {
						secondIndex = vertices.size();
					}
					edges.push_back(CDT::Edge(vertices.size() + i, secondIndex));
				}

				for (const auto& point : points) {
					vertices.push_back({ point.x, point.y });
				}
			}

			CDT::RemoveDuplicatesAndRemapEdges(vertices, edges);

			cdt.insertVertices(vertices);
			cdt.insertEdges(edges);

			cdt.eraseOuterTrianglesAndHoles();

			std::vector<FilledItemContour> contours;

			for (const CDT::Triangle& triangle : cdt.triangles) {
				auto& point1 = cdt.vertices[triangle.vertices[0]];
				auto& point2 = cdt.vertices[triangle.vertices[1]];
				auto& point3 = cdt.vertices[triangle.vertices[2]];

				std::vector<Animate::Publisher::Point2D> triangle_shape(
					{
						{point1.x, point1.y},
						{point2.x, point2.y},
						{point3.x, point3.y},
						{point3.x, point3.y},
					}
					);

				contours.emplace_back(triangle_shape);
			}

			m_group.AddElement<FilledItem>(m_symbol, contours, color);
		}

		void SCShapeWriter::AddRasterizedRegion(
			const FilledElementRegion& region,
			float resolution
		)
		{
			wk::RawImageRef sprite;
			wk::Point offset;
			DrawRegion(region, resolution, sprite, offset);

			const Animate::DOM::Utils::MATRIX2D transform = {
				1 / resolution,
				0,
				0,
				1 / resolution,
				(FCM::Float)offset.x,
				(FCM::Float)offset.y
			};

			m_group.AddElement<BitmapItem>(m_symbol, sprite, transform, true);
		}

		void SCShapeWriter::CreatePath(
			const Animate::Publisher::FilledElementPath& path,
			wk::PointF offset,
			BLPath& contour,
			float resolution
		)
		{
			uint8_t inited = false;
			for (size_t i = 0; path.Count() > i; i++)
			{
				const FilledElementPathSegment& segment = path.GetSegment(i);

				switch (segment.SegmentType())
				{
				case FilledElementPathSegment::Type::Line:
				{
					const auto& seg = (const FilledElementPathLineSegment&)segment;

					if (!inited++)
					{
						contour.moveTo(
							seg.begin.x + offset.x,
							seg.begin.y + offset.y
						);
					}

					contour.lineTo(
						seg.end.x + offset.x,
						seg.end.y + offset.y
					);
				}

				break;
				case FilledElementPathSegment::Type::Cubic:
				{
					const auto& seg = (const FilledElementPathCubicSegment&)segment;

					if (!inited++)
					{
						contour.moveTo(
							seg.begin.x + offset.x,
							seg.begin.y + offset.y
						);
					}

					contour.cubicTo(
						seg.control_l.x + offset.x, seg.control_l.y + offset.y,
						seg.control_r.x + offset.x, seg.control_r.y + offset.y,
						seg.end.x + offset.x, seg.end.y + offset.y
					);
				}
				break;
				case FilledElementPathSegment::Type::Quad:
				{
					const auto& seg = (const FilledElementPathQuadSegment&)segment;

					if (!inited++)
					{
						contour.moveTo(
							seg.begin.x + offset.x,
							seg.begin.y + offset.y
						);
					}

					contour.quadTo(
						seg.control.x + offset.x, seg.control.y + offset.y,
						seg.end.x + offset.x, seg.end.y + offset.y
					);
				}
				break;
				default:
					break;
				}
			}

			
			if (resolution != 1.0f)
			{
				BLMatrix2D matrix(
					resolution, 0, 0, resolution,
					0, 0
				);
				contour.transform(matrix);
			}
		}

		bool SCShapeWriter::IsComplexShapeRegion(const FilledElementRegion& region)
		{
			for (size_t i = 0; region.contour.Count() > i; i++)
			{
				const FilledElementPathSegment& segment = region.contour.GetSegment(i);

				if (segment.SegmentType() != FilledElementPathSegment::Type::Line)
				{
					return true;
				}
			}

			for (const FilledElementPath& path : region.holes)
			{
				for (size_t i = 0; path.Count() > i; i++)
				{
					const FilledElementPathSegment& segment = path.GetSegment(i);

					if (segment.SegmentType() != FilledElementPathSegment::Type::Line)
					{
						return true;
					}
				}
			}

			return false;
		}

		bool SCShapeWriter::IsValidFilledShapeRegion(const FilledElementRegion& region)
		{
			if (region.type == FilledElementRegion::ShapeType::SolidColor)
			{
				// Skip all regions with zero mask_alpha
				if (region.solid.color.alpha <= 0)
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			return true;
		}

		void SCShapeWriter::AddFilledShapeRegion(
			const FilledElementRegion& region,
			const Animate::DOM::Utils::MATRIX2D& matrix
		) {
			if (!IsValidFilledShapeRegion(region)) return;

			bool should_rasterize =
				region.type != FilledElementRegion::ShapeType::SolidColor ||
				IsComplexShapeRegion(region);

			bool is_contour =
				!should_rasterize &&
				region.contour.Count() <= 4 && !(region.contour.Count() > 6) &&
				region.holes.empty();

			bool should_triangulate =
				!should_rasterize &&
				region.contour.Count() > 4;

			FilledElementRegion transformed_region = region;
			transformed_region.Transform(matrix);
			
			if (should_rasterize)
			{
				AddRasterizedRegion(region);
				return;
			}

			ReleaseVectorGraphic();
			if (is_contour)
			{
				std::vector<Point2D> points;
				region.contour.Rasterize(points);

				std::vector<FilledItemContour> contour = { FilledItemContour(points) };
				m_group.AddElement<FilledItem>(m_symbol, contour, region.solid.color);
			}
			else if (should_triangulate)
			{
				AddTriangulatedRegion(region.contour, region.holes, region.solid.color);
			}
		}

		void SCShapeWriter::AddSlicedElements(const Animate::Publisher::Slice9Element& slice)
		{
			// 9Slice sprites are usually very pixelated
			// So we need to scale their resolution by 2
			// But xy coordinates must be remains the same

			// So first we create a bigger guide

			const float resolution = SCShapeWriter::RasterizationResolution;
			auto guides = slice.Guides();
			Animate::DOM::Utils::RECT element_guides =
			{
				{guides.topLeft.x * resolution, guides.topLeft.y * resolution},
				{guides.bottomRight.x * resolution, guides.bottomRight.y * resolution}
			};

			Animate::DOM::Utils::RECT bound{
				{std::numeric_limits<float>::min(),
				std::numeric_limits<float>::min()},
				{std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max()}
			};

			// Then create copy of elements
			// And make their points bigger
			std::vector<FilledElement> transformed_elements;
			const auto& elements = slice.Elements();
			for (size_t i = 0; elements.Size() > i; i++)
			{
				StaticElement& element = elements[i];
				if (!element.IsFilledArea()) continue;

				FilledElement& transformed_element = transformed_elements.emplace_back((const FilledElement&)element);
				transformed_element.Transform(
					element.Transformation()
				);

				transformed_element.Transform(
					{
						resolution,
						0.0f,
						0.0f,
						resolution,
						0.0f,
						0.0f
					}
				);

				bound = bound + transformed_element.Bound();

				//for (auto& region : transformed_element.fill)
				//{
				//	SCShapeWriter::RoundRegion(region);
				//}
				//
				//for (auto& region : transformed_element.stroke)
				//{
				//	SCShapeWriter::RoundRegion(region);
				//}
			}

			wk::Point offset(bound.bottomRight.x, bound.bottomRight.y);
			SCShapeWriter::RoundDomRectangle(bound);
			wk::RawImageRef sprite = wk::CreateRef<wk::RawImage>(
				std::ceil(bound.topLeft.x - offset.x),
				std::ceil(bound.topLeft.y - offset.y),
				wk::Image::PixelDepth::RGBA8
			);

			for (const FilledElement& element : transformed_elements)
			{
				for (const FilledElementRegion region : element.fill)
				{
					if (!IsValidFilledShapeRegion(region)) continue;

					DrawRegionTo(sprite, region, offset);
				}
			}

			// Scale back
			const Animate::DOM::Utils::MATRIX2D transform = {
				1.f / resolution,
				0.0f,
				0.0f,
				1.f / resolution,
				0,
				0
			};

			m_group.AddElement<SlicedItem>(m_symbol, sprite, transform, offset, element_guides);
		}

		void SCShapeWriter::RoundDomRectangle(Animate::DOM::Utils::RECT& rect)
		{
			auto round_number = [](float number)
				{
					float base_number = std::trunc(number);
					float decimal = std::abs(number - base_number);

					if (decimal >= 0.5f)
					{
						if (base_number >= 0.0f)
						{
							return base_number + 1.0f;
						}
						else
						{
							return base_number - 1.0f;
						}
					}

					return (float)base_number;
				};

			rect.bottomRight.x = round_number(rect.bottomRight.x);
			rect.bottomRight.y = round_number(rect.bottomRight.y);
			rect.topLeft.x = round_number(rect.topLeft.x);
			rect.topLeft.y = round_number(rect.topLeft.y);
		}

		std::size_t SCShapeWriter::GenerateHash() const
		{
			wk::hash::XxHash code;

			for (size_t i = 0; m_group.Size() > i; i++)
			{
				const GraphicItem& item = (const GraphicItem&)m_group[i];
				code.update(item);
			}

			return code.digest();
		}

		bool SCShapeWriter::Finalize(uint16_t id, bool required, bool /*new_symbol*/) {
			ReleaseVectorGraphic();

			if (m_group.Size() == 0)
			{
				if (required)
				{
					// Small workaround to avoid crashes with empty shapes but keep it required
					flash::MovieClip& movieclip = m_writer.swf.movieclips.emplace_back();
					movieclip.id = id;

					return true;
				}
				else
				{
					return false;
				}
			}

			flash::Shape& shape = m_writer.swf.shapes.emplace_back();
			shape.id = id;

			m_writer.AddGraphicGroup(m_group);

			return true;
		}

		void SCShapeWriter::CreateCanvas(const Animate::DOM::Utils::RECT bound, float resolution)
		{
			canvas = wk::CreateUnique<RasterizingContext>();
			canvas->image = wk::CreateRef<wk::RawImage>(
				std::ceil(bound.topLeft.x - bound.bottomRight.x),
				std::ceil(bound.topLeft.y - bound.bottomRight.y),
				wk::Image::PixelDepth::RGBA8,
				wk::Image::ColorSpace::Linear
			);
			BLResult result = canvas->canvas_image.createFromData(
				canvas->image->width(), canvas->image->height(),
				BLFormat::BL_FORMAT_PRGB32, canvas->image->data(), canvas->image->pixel_size() * canvas->image->width()
			);
			bl_assert(result);

			canvas->ctx = BLContext(canvas->canvas_image);
		}

		void SCShapeWriter::ReleaseCanvas()
		{
			bl_assert(canvas->ctx.end());
			canvas.reset();
		}

		void SCShapeWriter::ReleaseVectorGraphic()
		{
			//if (m_vector_graphics.empty()) return;


		}

		void SCShapeWriter::DrawRegion(const Animate::Publisher::FilledElementRegion& region, wk::PointF offset, float resolution)
		{
			// Contour drawing
			{
				BLPath contour;
				SCShapeWriter::CreatePath(region.contour, offset, contour, resolution);

				BLResult result = BL_SUCCESS;
				if (region.type == FilledElementRegion::ShapeType::SolidColor)
				{
					result = canvas->ctx.fillPath(contour, BLRgba32(region.solid.color.blue, region.solid.color.green, region.solid.color.red, region.solid.color.alpha));
				}
				bl_assert(result);
			}

			// Hole drawing
			{
				canvas->ctx.setCompOp(BLCompOp::BL_COMP_OP_CLEAR);
				for (const auto& hole : region.holes)
				{
					BLPath contour;
					SCShapeWriter::CreatePath(hole, offset, contour, resolution);

					BLResult result = canvas->ctx.fillPath(
						contour, BLRgba32(0)
					);
					bl_assert(result);
				}

				canvas->ctx.setCompOp(BLCompOp::BL_COMP_OP_SRC_OVER);
			}
		}

		void SCShapeWriter::DrawRegionTo(
			const wk::RawImageRef target,
			const Animate::Publisher::FilledElementRegion& region,
			wk::Point offset,
			float resolution
		)
		{
			wk::RawImageRef sprite;
			wk::Point region_offset;
			DrawRegion(region, resolution, sprite, region_offset);

			for (uint16_t h = 0; sprite->height() > h; h++)
			{
				for (uint16_t w = 0; sprite->width() > w; w++)
				{
					auto& source = sprite->at<wk::ColorRGBA>(w, h);
					if (!source.a) continue;

					uint16_t dstW = w + (region_offset.x - offset.x);
					uint16_t dstH = h + (region_offset.y - offset.y);

					if (dstW >= target->width() || dstH >= target->height())
					{
						continue;
					}

					auto& dest = target->at<wk::ColorRGBA>(dstW, dstH);

					float alpha_fac = (255 - (float)source.a) / 255;
					float alpha = (float)source.a + (float)dest.a * alpha_fac;

					float red = (float)source.r + (float)dest.r * alpha_fac;
					float green = (float)source.g + (float)dest.g * alpha_fac;
					float blue = (float)source.b + (float)dest.b * alpha_fac;

					dest.r = std::ceil(red);
					dest.g = std::ceil(green);
					dest.b = std::ceil(blue);
					dest.a = std::ceil(alpha);
				}
			}
		}

		void SCShapeWriter::DrawRegion(const Animate::Publisher::FilledElementRegion& region, float resolution, wk::RawImageRef& result, wk::Point& result_offset)
		{
			Animate::DOM::Utils::RECT bound = region.Bound();
			wk::PointF offset(-std::min(bound.topLeft.x, bound.bottomRight.x), -std::min(bound.topLeft.y, bound.bottomRight.y));
			result_offset.x = bound.bottomRight.x;
			result_offset.y = bound.bottomRight.y;

			SCShapeWriter::RoundDomRectangle(bound);
			CreateCanvas(bound, resolution);

			DrawRegion(region, offset, resolution);

			result = canvas->image;
			ReleaseCanvas();
		}

		void SCShapeWriter::RoundRegion(Animate::Publisher::FilledElementRegion& path)
		{
			SCShapeWriter::RoundPath(path.contour);

			for (auto& hole : path.holes)
			{
				SCShapeWriter::RoundPath(hole);
			}
		}

		void SCShapeWriter::RoundPath(Animate::Publisher::FilledElementPath& path)
		{
			for (size_t i = 0; path.Count() > i; i++)
			{
				auto& segment = path.GetSegment(i);

				switch (segment.SegmentType())
				{
				case FilledElementPathSegment::Type::Line:
				{
					auto& seg = (FilledElementPathLineSegment&)segment;
					seg.begin.x = std::ceil(seg.begin.x);
					seg.begin.y = std::ceil(seg.begin.y);
					seg.end.x = std::ceil(seg.end.x);
					seg.end.y = std::ceil(seg.end.y);
				}
				break;
				case FilledElementPathSegment::Type::Cubic:
				{
					auto& seg = (FilledElementPathCubicSegment&)segment;
					seg.begin.x = std::ceil(seg.begin.x);
					seg.begin.y = std::ceil(seg.begin.y);

					seg.control_l.x = std::ceil(seg.control_l.x);
					seg.control_l.y = std::ceil(seg.control_l.y);
					seg.control_r.x = std::ceil(seg.control_r.x);
					seg.control_r.y = std::ceil(seg.control_r.y);
					
					seg.end.x = std::ceil(seg.end.x);
					seg.end.y = std::ceil(seg.end.y);
				}
				break;

				case FilledElementPathSegment::Type::Quad:
				{
					auto& seg = (FilledElementPathQuadSegment&)segment;
					seg.begin.x = std::ceil(seg.begin.x);
					seg.begin.y = std::ceil(seg.begin.y);

					seg.control.x = std::ceil(seg.control.x);
					seg.control.y = std::ceil(seg.control.y);

					seg.end.x = std::ceil(seg.end.x);
					seg.end.y = std::ceil(seg.end.y);
				}
				break;

				default:
					break;
				}
			}
			
		}
	}
}

namespace wk::hash
{
	template<>
	struct Hash_t<sc::Adobe::GraphicItem>
	{
		template<typename T>
		static void update(wk::hash::HashStream<T>& stream, const sc::Adobe::GraphicItem& item)
		{
			stream.update(item.Transformation2D());
			if (item.IsSprite())
			{
				const sc::Adobe::BitmapItem& sprite = (const sc::Adobe::BitmapItem&)item;
				const auto& image = sprite.Image();
				stream.update((const wk::Image&)image);
			
				if (sprite.Is9Sliced())
				{
					const sc::Adobe::SlicedItem& sliced = (const sc::Adobe::SlicedItem&)item;
					stream.update(sliced.Guides());
				}
			}
			else if (item.IsSolidColor())
			{
				const sc::Adobe::FilledItem& fill = (const sc::Adobe::FilledItem&)item;
				
				stream.update(fill.Color());
				
				for (auto& contour : fill.contours)
				{
					for (auto& point : contour.Contour())
					{
						stream.update(point.x);
						stream.update(point.y);
					}
				}
			}
		}
	};
}