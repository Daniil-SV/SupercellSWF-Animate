#pragma once

#include <string>
#include <math.h>

#include <AnimateSDK/app/DOM/Utils/DOMTypes.h>

#include <AnimateSDK/app/DOM/FrameElement/IClassicText.h>
#include <AnimateSDK/app/DOM/FrameElement/IParagraph.h>
#include <AnimateSDK/app/DOM/FrameElement/ITextRun.h>
#include <AnimateSDK/app/DOM/FrameElement/ITextBehaviour.h>

using namespace DOM::FrameElement;

namespace sc {
	namespace Adobe {
		struct TextFieldInfo {
		public:
			bool operator==(TextFieldInfo const& other) const;

		public:
			DOM::Utils::RECT bound;

			// IClassicText

			std::u16string text;
			AA_MODE_PROP renderingMode;

			// IClassicText -> IParagraph

			PARAGRAPH_STYLE style;

			// IClassicText -> IParagraph -> ITextRun -> ITextStyle

			DOM::Utils::COLOR fontColor = { 0xFF, 0xFF, 0xFF, 0xFF };
			uint16_t fontSize;
			std::string fontStyle;
			std::u16string fontName;
			FCM::Boolean autoKern;
			DOM::FrameElement::LineMode lineMode;

			// Glow filter
			bool isOutlined = false;
			DOM::Utils::COLOR outlineColor = { 0xFF, 0xFF, 0xFF, 0xFF };
		};
	}
}