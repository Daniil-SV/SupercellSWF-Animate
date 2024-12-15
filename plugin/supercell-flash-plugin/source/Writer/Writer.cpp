#include "Writer.h"

#include "Module/Module.h"
#include "MovieclipWriter.h"
#include "ShapeWriter.h"
#include "Module/SCPluginException.h"

#include "atlas_generator/Generator.h"
#include "atlas_generator/PackagingException.h"

using namespace Animate::Publisher;

namespace sc {
	namespace Adobe {
		SCWriter::SCWriter()
		{
			const SCConfig& config = SCPlugin::Publisher::ActiveConfig();
			SCPlugin& context = SCPlugin::Instance();

			if (config.exportToExternal && !fs::exists(config.exportToExternalPath))
			{
				throw SCPluginException("TID_SWF_MISSING_EXTERNAL_FILE", config.exportToExternalPath.wstring().c_str());
			}
		}

		SCWriter::~SCWriter()
		{
			if (fs::exists(sprite_temp_path)) {
				remove(sprite_temp_path);
			}
		}

		SharedMovieclipWriter* SCWriter::AddMovieclip(SymbolContext& symbol) {
			return new SCMovieclipWriter(*this, symbol);
		}

		SharedShapeWriter* SCWriter::AddShape(SymbolContext& symbol) {
			return new SCShapeWriter(*this, symbol);
		}

		void SCWriter::AddModifier(uint16_t id, MaskedLayerState type) {
			flash::MovieClipModifier& modifier = swf.movieclip_modifiers.emplace_back();
			modifier.id = id;

			switch (type)
			{
			case MaskedLayerState::MASK_LAYER:
				modifier.type = flash::MovieClipModifier::Type::Mask;
				break;
			case MaskedLayerState::MASKED_LAYERS:
				modifier.type = flash::MovieClipModifier::Type::Masked;
				break;
			case MaskedLayerState::MASKED_LAYERS_END:
				modifier.type = flash::MovieClipModifier::Type::Unmasked;
				break;
			default:
				break;
			}
		}

		void SCWriter::AddTextField(uint16_t id, SymbolContext& symbol, TextElement& object) {
			using namespace Animate::DOM;
			const SCConfig& config = SCPlugin::Publisher::ActiveConfig();

			flash::TextField& textfield = swf.textfields.emplace_back();

			std::string text = FCM::Locale::ToUtf8(object.text);
			std::string fontName = FCM::Locale::ToUtf8(object.fontName);

			textfield.id = id;

			textfield.text = flash::SWFString(text);

			textfield.font_name = flash::SWFString(fontName);
			textfield.font_color = *(uint32_t*)&object.fontColor;
			textfield.font_size = (uint8_t)object.fontSize;

			switch (object.style.alignment)
			{
			case FrameElement::AlignMode::ALIGN_MODE_CENTER:
				textfield.font_horizontal_align = flash::TextField::Align::Center;
				break;
			case FrameElement::AlignMode::ALIGN_MODE_JUSTIFY:
				textfield.font_horizontal_align = flash::TextField::Align::Justify;
				break;
			case FrameElement::AlignMode::ALIGN_MODE_LEFT:
				textfield.font_horizontal_align = flash::TextField::Align::Left;
				break;
			case FrameElement::AlignMode::ALIGN_MODE_RIGHT:
				textfield.font_horizontal_align = flash::TextField::Align::Right;
				break;
			default:
				break;
			}

			textfield.left = (int16_t)floor(object.bound.topLeft.x);
			textfield.top = (int16_t)floor(object.bound.topLeft.y);

			textfield.right = (int16_t)floor(object.bound.bottomRight.x);
			textfield.bottom = (int16_t)floor(object.bound.bottomRight.y);

			if (object.fontStyle != FrameElement::REGULAR_STYLE_STR) {
				if (object.fontStyle != FrameElement::ITALIC_STYLE_STR) {
					textfield.is_italic = true;
				}
				else if (object.fontStyle != FrameElement::BOLD_STYLE_STR) {
					textfield.is_bold = true;
				}
				else if (object.fontStyle != FrameElement::BOLD_ITALIC_STYLE_STR) {
					textfield.is_italic = true;
					textfield.is_bold = true;
				}
			}

			textfield.is_outlined = object.isOutlined;
			if (object.isOutlined)
			{
				textfield.outline_color = (static_cast<uint32_t>(object.outlineColor.red) << 24) |
					(static_cast<uint32_t>(object.outlineColor.green) << 16) |
					(static_cast<uint32_t>(object.outlineColor.blue) << 8) |
					static_cast<uint32_t>(object.outlineColor.alpha);
			}

			textfield.is_multiline =
				object.lineMode == FrameElement::LineMode::LINE_MODE_SINGLE ? false : true;

			if (object.renderingMode.aaMode == FrameElement::AAMode::ANTI_ALIAS_MODE_DEVICE) {
				textfield.use_device_font = true;
			}

			if (config.backwardCompatibility) return;

			textfield.auto_kern = object.autoKern == FCM::Boolean(true);
		}

		void SCWriter::LoadExternal() {
			using namespace Animate::DOM;

			const SCConfig& config = SCPlugin::Publisher::ActiveConfig();

			const fs::path& filepath = config.exportToExternalPath;

			flash::SupercellSWF base_swf;
			base_swf.load(filepath);

			uint16_t idOffset = 0;
			{
				for (flash::MovieClip& object : base_swf.movieclips) {
					if (object.id > idOffset) {
						idOffset = object.id;
					}
				}

				for (flash::MovieClipModifier& object : base_swf.movieclip_modifiers) {
					if (object.id > idOffset) {
						idOffset = object.id;
					}
				}

				for (flash::TextField& object : base_swf.textfields) {
					if (object.id > idOffset) {
						idOffset = object.id;
					}
				}

				for (flash::Shape& object : base_swf.shapes) {
					if (object.id > idOffset) {
						idOffset = object.id;
					}
				}
			}
			idOffset++;

			// Display object processing
			for (flash::MovieClip& object : swf.movieclips) {
				object.id += idOffset;

				for (flash::DisplayObjectInstance& bind : object.childrens) {
					bind.id += idOffset;
				}

				object.bank_index += base_swf.matrixBanks.size();

				base_swf.movieclips.push_back(object);
			}

			for (flash::MovieClipModifier& object : swf.movieclip_modifiers) {
				object.id += idOffset;

				base_swf.movieclip_modifiers.push_back(object);
			}

			for (flash::TextField& object : swf.textfields) {
				object.id += idOffset;

				base_swf.textfields.push_back(object);
			}

			for (flash::Shape& object : swf.shapes) {
				object.id += idOffset;

				for (flash::ShapeDrawBitmapCommand& bitmap : object.commands) {
					bitmap.texture_index += base_swf.textures.size();
				}

				base_swf.shapes.push_back(object);
			}

			// Common resources processing
			for (flash::MatrixBank& bank : swf.matrixBanks) {
				base_swf.matrixBanks.push_back(bank);
			}

			// Additional texture preprocessing
			for (flash::SWFTexture& texture : base_swf.textures)
			{
				texture.encoding(config.textureEncoding);
			}

			for (flash::SWFTexture& texture : swf.textures) {
				base_swf.textures.push_back(texture);
			}
			for (flash::ExportName& object : swf.exports) {
				object.id += idOffset;
				base_swf.exports.push_back(object);
			}

			swf = base_swf;
		}

		void SCWriter::ProcessCommandTransform(
			flash::ShapeDrawBitmapCommand& command,
			wk::AtlasGenerator::Item::Transformation& transform,
			GraphicItem& item
		)
		{
			using namespace wk;
			using namespace wk::AtlasGenerator;

			wk::Matrix2D matrix = item.transformation();

			for (flash::ShapeDrawBitmapCommandVertex& vertex : command.vertices)
			{
				PointUV uv(vertex.u, vertex.v);
				PointF xy(vertex.x, vertex.y);
				transform.transform_point(uv);

				vertex.u = uv.u / (float)swf.textures[command.texture_index].image()->width();
				vertex.v = uv.v / (float)swf.textures[command.texture_index].image()->height();

				vertex.x = (matrix.a * xy.x) + (matrix.c * xy.y) + matrix.tx;
				vertex.y = (matrix.b * xy.x) + (matrix.d * xy.y) + matrix.ty;
			}
		}

		void SCWriter::ProcessSpriteItem(
			flash::Shape & shape,
			wk::AtlasGenerator::Item& atlas_item,
			SpriteItem& sprite_item
		)
		{
			using namespace wk;

			flash::ShapeDrawBitmapCommand& shape_command = shape.commands.emplace_back();
			shape_command.texture_index = atlas_item.texture_index;

			for (AtlasGenerator::Vertex& vertex : atlas_item.vertices)
			{
				auto& shape_vertex = shape_command.vertices.emplace_back();

				shape_vertex.x = vertex.xy.x;
				shape_vertex.y = vertex.xy.y;
				shape_vertex.u = vertex.uv.u;
				shape_vertex.v = vertex.uv.v;
			}

			ProcessCommandTransform(shape_command, atlas_item.transform, sprite_item);
		}

		void SCWriter::ProcessSlicedItem(
			flash::Shape& shape,
			wk::AtlasGenerator::Item& atlas_item,
			SlicedItem& sliced_item
		)
		{
			// TODO: 9slice

			//uint8_t begin = (uint8_t)AtlasGenerator::Item::SlicedArea::BottomLeft;
			//uint8_t end = (uint8_t)AtlasGenerator::Item::SlicedArea::TopRight;
			//for (uint8_t i = begin; end >= i; i++)
			//{
			//	Rect<int32_t> xy;
			//	Rect<uint16_t> uv;
			//
			//	Matrix2x3<float> matrix = sliced_item.transformation();
			//
			//	float rotation_sin = matrix.b;
			//	float rotation_cos = matrix.c;
			//
			//	AtlasGenerator::Item::Transformation transform(
			//		atan2(rotation_sin, rotation_cos),
			//		{ (int32_t)std::ceil(matrix.tx), (int32_t)std::ceil(matrix.ty) }
			//	);
			//
			//	atlas_item.get_sliced_area(
			//		(AtlasGenerator::Item::SlicedArea)i,
			//		sliced_item.Guides(),
			//		xy,
			//		uv,
			//		transform
			//	);
			//
			//	if (xy.width <= 0 || xy.height <= 0)
			//	{
			//		continue;
			//	}
			//
			//	ShapeDrawBitmapCommand& shape_command = shape.commands.emplace_back();
			//	shape_command.texture_index = atlas_item.texture_index;
			//
			//	{
			//		ShapeDrawBitmapCommandVertex& vertex = shape_command.vertices.emplace_back();
			//		vertex.u = uv.x;
			//		vertex.v = uv.y;
			//		vertex.x = xy.x;
			//		vertex.y = xy.y;
			//	}
			//
			//	{
			//		ShapeDrawBitmapCommandVertex& vertex = shape_command.vertices.emplace_back();
			//		vertex.u = uv.x + uv.width;
			//		vertex.v = uv.y;
			//		vertex.x = xy.x + xy.width;
			//		vertex.y = xy.y;
			//	}
			//
			//	{
			//		ShapeDrawBitmapCommandVertex& vertex = shape_command.vertices.emplace_back();
			//		vertex.u = uv.x + uv.width;
			//		vertex.v = uv.y + uv.height;
			//		vertex.x = xy.x + xy.width;
			//		vertex.y = xy.y + uv.height;
			//	}
			//
			//	{
			//		ShapeDrawBitmapCommandVertex& vertex = shape_command.vertices.emplace_back();
			//		vertex.u = uv.x;
			//		vertex.v = uv.y + uv.height;
			//		vertex.x = xy.x;
			//		vertex.y = xy.y + uv.height;
			//	}
			//
			//	for (ShapeDrawBitmapCommandVertex& vertex : shape_command.vertices)
			//	{
			//		Point<float> vertex_uv(vertex.u, vertex.v);
			//		atlas_item.transform.transform_point(vertex_uv);
			//
			//		vertex.x = vertex.x * matrix.a;
			//		vertex.y = vertex.y * matrix.d;
			//
			//		vertex.u = vertex_uv.u / (float)swf.textures[shape_command.texture_index].image()->width();
			//		vertex.v = vertex_uv.v / (float)swf.textures[shape_command.texture_index].image()->height();
			//	}
			//}
		}

		void SCWriter::ProcessFilledItem(
			flash::Shape& shape,
			wk::AtlasGenerator::Item& atlas_item,
			FilledItem& filled_item
		)
		{
			auto atlas_point = atlas_item.get_colorfill().value();

			for (const FilledItemContour& contour : filled_item.contours)
			{
				flash::ShapeDrawBitmapCommand& shape_command = shape.commands.emplace_back();
				shape_command.texture_index = atlas_item.texture_index;

				for (const Point2D& point : contour.Contour())
				{
					flash::ShapeDrawBitmapCommandVertex& shape_vertex = shape_command.vertices.emplace_back();

					shape_vertex.u = atlas_point.uv.x;
					shape_vertex.v = atlas_point.uv.y;
					shape_vertex.x = point.x;
					shape_vertex.y = point.y;

					ProcessCommandTransform(shape_command, atlas_item.transform, filled_item);
				}
			}
		}

		void SCWriter::FinalizeAtlas()
		{
			using namespace wk;

			const SCConfig& config = SCPlugin::Publisher::ActiveConfig();
			SCPlugin& context = SCPlugin::Instance();

			StatusComponent* status = context.Window()->CreateStatusBarComponent(
				context.locale.GetString("TID_STATUS_SPRITE_PACK")
			);

			std::vector<AtlasGenerator::Item> items;

			for (GraphicGroup& group : m_graphic_groups)
			{
				for (size_t i = 0; group.Size() > i; i++)
				{
					GraphicItem& item = group.GetItem(i);

					if (item.IsSprite())
					{
						SpriteItem& sprite_item = *(SpriteItem*)&item;

						items.emplace_back(
							sprite_item.image(),
							item.IsSliced()
						);
					}
					else if (item.IsFilledShape())
					{
						FilledItem& filled_item = *(FilledItem*)&item;

						items.emplace_back(filled_item.Color());
					}
					else
					{
						throw Exception("Not implemented");
					}
				}
			}

			AtlasGenerator::Config generator_config(
				config.textureMaxWidth,
				config.textureMaxHeight,
				config.textureScaleFactor,
				2
			);

			int itemCount = (int)items.size();
			status->SetRange(itemCount);

			generator_config.progress = [&status](uint32_t value) {
				status->SetProgress(value);
			};

			AtlasGenerator::Generator generator(generator_config);

			uint16_t texture_count = 0;

			try
			{
				texture_count = generator.generate(items);
			}
			catch (const AtlasGenerator::PackagingException& exception)
			{
				// [AtlasGenerator] Reason / symbol name
				// or in case of unknown exception just reason
				if (exception.reason() == AtlasGenerator::PackagingException::Reason::Unknown)
				{
					throw SCPluginException(
						Localization::Format(
							u"[AtlasGenerator] %ls",
							context.locale.GetString("TID_SWF_ATLAS_UNKNOWN").c_str()
						)
					);
				}

				std::u16string reason;
				std::u16string symbol_name;

				switch (exception.reason())
				{
				case AtlasGenerator::PackagingException::Reason::UnsupportedImage:
					reason = context.locale.GetString("TID_SWF_ATLAS_UNSUPORTED_IMAGE");
					break;
				case AtlasGenerator::PackagingException::Reason::InvalidPolygon:
					reason = context.locale.GetString("TID_SWF_ATLAS_INVALID_POLYGON");
					break;
				case AtlasGenerator::PackagingException::Reason::TooBigImage:
					reason = context.locale.GetString("TID_SWF_ATLAS_TOO_BIG_IMAGE");
					break;
				default:
					break;
				}

				if (exception.index() != SIZE_MAX)
				{
					size_t atlas_item_index = 0;
					size_t group_index = 0;
					for (; m_graphic_groups.size() >= group_index; group_index++)
					{
						GraphicGroup& group = m_graphic_groups[group_index];
						for (size_t group_item_index = 0; group.Size() > group_item_index; group_item_index++)
						{
							if (atlas_item_index == exception.index()) goto RESULT_GROUP;

							atlas_item_index++;
						}
					}
				RESULT_GROUP:
					symbol_name = m_graphic_groups[group_index].symbol.name;
				}
				else
				{
					symbol_name = context.locale.GetString("TID_SWF_ATLAS_UNKNOWN_SYMBOL");
				}

				throw SCPluginException(
					Localization::Format(
						u"[AtlasGenerator] %ls %ls", reason.c_str(), symbol_name.c_str()
					)
				);
			}

			context.Window()->DestroyStatusBar(status);

			for (uint16_t i = 0; texture_count > i; i++) {
				cv::Mat& atlas = generator.get_atlas(i);

				cv::cvtColor(atlas, atlas, cv::COLOR_BGRA2RGBA);

				flash::SWFTexture& texture = swf.textures.emplace_back();
				size_t textureDataSize = atlas.total() * atlas.elemSize();
				SharedMemoryStream textureData(atlas.ptr(), textureDataSize);
				texture.load_from_buffer(textureData, atlas.cols, atlas.rows, flash::SWFTexture::PixelFormat::RGBA8);

				if (config.textureEncoding == flash::SWFTexture::TextureEncoding::Raw)
				{
					switch (config.textureQuality)
					{
					case SCConfig::Quality::Highest:
						texture.pixel_format(flash::SWFTexture::PixelFormat::RGBA8);
						break;
					case SCConfig::Quality::High:
					case SCConfig::Quality::Medium:
						texture.pixel_format(flash::SWFTexture::PixelFormat::RGBA4);
						break;
					case SCConfig::Quality::Low:
						texture.pixel_format(flash::SWFTexture::PixelFormat::RGB5_A1);
						break;
					default:
						break;
					}
				}
				else
				{
					texture.encoding(flash::SWFTexture::TextureEncoding::KhronosTexture);
				}
			}

			uint16_t command_index = 0;
			for (uint32_t shape_index = 0; swf.shapes.size() > shape_index; shape_index++)
			{
				flash::Shape& shape = swf.shapes[shape_index];
				GraphicGroup& group = m_graphic_groups[shape_index];

				for (uint32_t group_item_index = 0; group.Size() > group_item_index; group_item_index++)
				{
					AtlasGenerator::Item& atlas_item = items[command_index];
					GraphicItem& item = group.GetItem(group_item_index);

					if (item.IsSprite())
					{
						if (item.IsSliced())
						{
							SlicedItem& sliced_item = *(SlicedItem*)&item;
							ProcessSlicedItem(
								shape, atlas_item, sliced_item
							);
						}
						else
						{
							SpriteItem& sprite_item = *(SpriteItem*)&item;
							ProcessSpriteItem(
								shape, atlas_item, sprite_item
							);
						}
					}
					else if (item.IsFilledShape() && atlas_item.get_colorfill().has_value())
					{
						FilledItem& filled_item = *(FilledItem*)&item;
						ProcessFilledItem(
							shape, atlas_item, filled_item
						);
					}

					command_index++;
				}
			}
		}

		void SCWriter::Finalize() {
			const SCConfig& config = SCPlugin::Publisher::ActiveConfig();
			SCPlugin& context = SCPlugin::Instance();

			enum FinalizeStep : uint8_t
			{
				INIT = 0,
				ATLAS_FINALIZE = 1,
				EXTERNAL_LOADING = 2,
				SWF_SAVING = 3,
				END = 4
			};

			StatusComponent* status = context.Window()->CreateStatusBarComponent(
				context.locale.GetString("TID_STATUS_INIT"),
				u"",
				END
			);

			status->SetProgress(ATLAS_FINALIZE);
			status->SetStatusLabel(context.locale.GetString("TID_STATUS_TEXTURE_SAVE"));
			FinalizeAtlas();

			if (config.exportToExternal) {
				status->SetProgress(EXTERNAL_LOADING);
				status->SetStatusLabel(context.locale.GetString("TID_EXTERNAL_FILE_LOAD"));
				LoadExternal();
			}

			swf.use_external_texture = config.hasExternalTexture;
			swf.use_low_resolution = config.hasLowresTexture;
			swf.use_multi_resolution = config.hasMultiresTexture;
			swf.multi_resolution_suffix = flash::SWFString(config.multiResolutionSuffix);
			swf.low_resolution_suffix = flash::SWFString(config.lowResolutionSuffix);
			swf.use_precision_matrix = config.hasPrecisionMatrices;
			swf.save_custom_property = config.writeCustomProperties;

			// Raw textures can be stored only inside texture files
			if (config.textureEncoding == flash::SWFTexture::TextureEncoding::Raw)
			{
				swf.use_external_texture_files = false;
			}
			else
			{
				swf.use_external_texture_files = config.hasExternalCompressedTexture;
			}

			fs::path filepath = fs::path(config.outputFilepath).replace_extension("sc");
			fs::path basename = filepath.filename();

			status->SetProgress(EXTERNAL_LOADING);
			status->SetStatusLabel(
				context.locale.GetString(
					"TID_FILE_SAVE", basename.u16string().c_str()
				)
			);

			swf.save(filepath, config.compression);

			context.Window()->DestroyStatusBar(status);
		}

		wk::Ref<cv::Mat> SCWriter::GetBitmap(const SpriteElement& item)
		{
			std::u16string name = item.Name();

			if (m_cached_images.count(name))
			{
				return m_cached_images[name];
			}

			item.ExportImage(sprite_temp_path);

			wk::Ref<cv::Mat> image = wk::CreateRef<cv::Mat>(cv::imread(sprite_temp_path.string(), cv::IMREAD_UNCHANGED));
			m_cached_images[name] = image;

			return image;
		}

		void SCWriter::AddGraphicGroup(const GraphicGroup& group)
		{
			m_graphic_groups.push_back(group);
		}
	}
}