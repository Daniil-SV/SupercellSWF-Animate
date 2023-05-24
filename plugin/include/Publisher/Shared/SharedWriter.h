#pragma once

#include "SharedMovieclipWriter.h"
#include "SharedShapeWriter.h"
#include "Publisher/PublisherConfig.h"

#include <filesystem>
namespace fs = std::filesystem;

using namespace FCM;

namespace sc {
    namespace Adobe {
        class SharedWriter {
        public:
            virtual Result Init(PIFCMCallback callback, const PublisherConfig& config) = 0;
            virtual SharedMovieclipWriter* _FCMCALL AddMovieclip() = 0;
            virtual SharedShapeWriter* _FCMCALL AddShape() = 0;

            virtual void Finalize() = 0;
        };
    }
}