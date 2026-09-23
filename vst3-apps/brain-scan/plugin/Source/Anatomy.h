/*  BRAIN SCAN — procedural anatomy in Hounsfield units (260905.1).

    Peter: "it would be great if the bone-related presets had some actual
    bones, and the skull looked more like a skull". The phantoms (SINUS, SPINE,
    PULSE …) keep their formulas — a phantom is a calibration object, and its
    sound is its promise. The BODIES are built here: a head (with and without
    its soft tissue), a thorax, a lumbar spine, a thigh, a jaw. Every tissue
    carries its real CT number, so a radiographer's window means what it says:
    air −1000, fat about −90, soft tissue 40, trabecular bone a few hundred,
    cortical bone above a thousand, enamel at the top of the scale.

    Coordinates: x is the phase axis (patient's left is +x, radiological
    convention), y increases ANTERIOR so the axial slice reads face-up the
    way a CT is displayed, z increases superior. Each model states the size of
    its cube in centimetres. The returned value is HU; toUnit() maps
    −1000..2000 onto the volume's [0, 1].
*/
#pragma once
#include <cmath>
#include <cstdint>

namespace bs { namespace an {

inline float clamp01 (float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float toUnit (float hu) { return clamp01 ((hu + 1000.0f) / 3000.0f); }
inline float huOfUnit (float u) { return u * 3000.0f - 1000.0f; }

//  HU of the tissues, in one place
constexpr float AIR = -1000, LUNG = -860, FAT = -95, WATER = 0, CSF = 8, WHITE = 30, SOFT = 40,
                GREY = 38, MUSCLE = 44, BLOOD = 46, LIVER = 60, DISC = 80, CARTILAGE = 110,
                TRAB = 320, CORT = 1250, DENTINE = 1700, ENAMEL = 2000, COUCH = 130;

float head     (float x, float y, float z, bool withSoftTissue);   // 24 cm cube
float thorax   (float x, float y, float z);                        // 36 cm cube
float vertebra (float x, float y, float z);                        // 12 cm cube, three lumbar levels
float femur    (float x, float y, float z);                        // 16 cm cube, a thigh
float jaw      (float x, float y, float z);                        // 12 cm cube, mandible and teeth

} } // namespace bs::an
