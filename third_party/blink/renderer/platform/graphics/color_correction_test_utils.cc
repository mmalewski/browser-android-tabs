// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"

#include "base/sys_byteorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// This function is a compact version of SkHalfToFloat from Skia. If many
// color correction tests fail at the same time, please check if SkHalf format
// has changed.
static float Float16ToFloat(const uint16_t& f16) {
  union FloatUIntUnion {
    uint32_t fUInt;
    float fFloat;
  };
  FloatUIntUnion magic = {126 << 23};
  FloatUIntUnion o;
  if (((f16 >> 10) & 0x001f) == 0) {
    o.fUInt = magic.fUInt + (f16 & 0x03ff);
    o.fFloat -= magic.fFloat;
  } else {
    o.fUInt = (f16 & 0x03ff) << 13;
    if (((f16 >> 10) & 0x001f) == 0x1f)
      o.fUInt |= (255 << 23);
    else
      o.fUInt |= ((127 - 15 + ((f16 >> 10) & 0x001f)) << 23);
  }
  o.fUInt |= ((f16 >> 15) << 31);
  return o.fFloat;
}

bool ColorCorrectionTestUtils::IsNearlyTheSame(float expected,
                                               float actual,
                                               float tolerance) {
  EXPECT_LE(actual, expected + tolerance);
  EXPECT_GE(actual, expected - tolerance);
  return true;
}

void ColorCorrectionTestUtils::CompareColorCorrectedPixels(
    const void* actual_pixels,
    const void* expected_pixels,
    int num_pixels,
    ImageDataStorageFormat src_storage_format,
    PixelsAlphaMultiply alpha_multiplied,
    UnpremulRoundTripTolerance premul_unpremul_tolerance) {
  bool test_passed = true;
  int srgb_color_correction_tolerance = 3;
  float wide_gamut_color_correction_tolerance = 0.01;
  if (premul_unpremul_tolerance == kNoUnpremulRoundTripTolerance)
    wide_gamut_color_correction_tolerance = 0;

  switch (src_storage_format) {
    case kUint8ClampedArrayStorageFormat: {
      if (premul_unpremul_tolerance == kUnpremulRoundTripTolerance) {
        // Premul->unpremul->premul round trip does not introduce any error when
        // rounding intermediate results. However, we still might see some error
        // introduced in consecutive color correction operations (error <= 3).
        // For unpremul->premul->unpremul round trip, we do premul and compare
        // the result.
        const uint8_t* actual_pixels_u8 =
            static_cast<const uint8_t*>(actual_pixels);
        const uint8_t* expected_pixels_u8 =
            static_cast<const uint8_t*>(expected_pixels);
        for (int i = 0; test_passed && i < num_pixels; i++) {
          test_passed &=
              (actual_pixels_u8[i * 4 + 3] == expected_pixels_u8[i * 4 + 3]);
          int alpha_multiplier =
              alpha_multiplied ? 1 : expected_pixels_u8[i * 4 + 3];
          for (int j = 0; j < 3; j++) {
            test_passed &= IsNearlyTheSame(
                actual_pixels_u8[i * 4 + j] * alpha_multiplier,
                expected_pixels_u8[i * 4 + j] * alpha_multiplier,
                srgb_color_correction_tolerance);
          }
        }
      } else {
        EXPECT_EQ(std::memcmp(actual_pixels, expected_pixels, num_pixels * 4),
                  0);
      }
      break;
    }

    case kUint16ArrayStorageFormat: {
      const uint16_t* actual_pixels_u16 =
          static_cast<const uint16_t*>(actual_pixels);
      const uint16_t* expected_pixels_u16 =
          static_cast<const uint16_t*>(expected_pixels);
      for (int i = 0; test_passed && i < num_pixels; i++) {
        for (int j = 0; j < 4; j++) {
          test_passed &=
              IsNearlyTheSame(Float16ToFloat(actual_pixels_u16[i * 4 + j]),
                              Float16ToFloat(expected_pixels_u16[i * 4 + j]),
                              wide_gamut_color_correction_tolerance);
        }
      }
      break;
    }

    case kFloat32ArrayStorageFormat: {
      const float* actual_pixels_f32 = static_cast<const float*>(actual_pixels);
      const float* expected_pixels_f32 =
          static_cast<const float*>(expected_pixels);
      for (int i = 0; test_passed && i < num_pixels; i++) {
        for (int j = 0; j < 4; j++) {
          test_passed &= IsNearlyTheSame(actual_pixels_f32[i * 4 + j],
                                         expected_pixels_f32[i * 4 + j],
                                         wide_gamut_color_correction_tolerance);
        }
      }
      break;
    }

    default:
      NOTREACHED();
  }
  EXPECT_EQ(test_passed, true);
}

bool ColorCorrectionTestUtils::ConvertPixelsToColorSpaceAndPixelFormatForTest(
    void* src_data,
    int num_elements,
    CanvasColorSpace src_color_space,
    ImageDataStorageFormat src_storage_format,
    CanvasColorSpace dst_color_space,
    CanvasPixelFormat dst_pixel_format,
    std::unique_ptr<uint8_t[]>& converted_pixels,
    SkColorSpaceXform::ColorFormat color_format_for_f16_canvas) {
  unsigned num_pixels = num_elements / 4;
  // Setting SkColorSpaceXform::apply parameters
  SkColorSpaceXform::ColorFormat src_color_format =
      SkColorSpaceXform::kRGBA_8888_ColorFormat;

  uint16_t* u16_buffer = static_cast<uint16_t*>(src_data);
  switch (src_storage_format) {
    case kUint8ClampedArrayStorageFormat:
      break;

    case kUint16ArrayStorageFormat:
      src_color_format =
          SkColorSpaceXform::ColorFormat::kRGBA_U16_BE_ColorFormat;
      for (int i = 0; i < num_elements; i++)
        u16_buffer[i] = base::ByteSwap(u16_buffer[i]);
      break;

    case kFloat32ArrayStorageFormat:
      src_color_format = SkColorSpaceXform::kRGBA_F32_ColorFormat;
      break;

    default:
      NOTREACHED();
      return false;
  }

  SkColorSpaceXform::ColorFormat dst_color_format =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (dst_pixel_format == kF16CanvasPixelFormat)
    dst_color_format = color_format_for_f16_canvas;

  sk_sp<SkColorSpace> src_sk_color_space = nullptr;
  src_sk_color_space =
      CanvasColorParams(src_color_space,
                        (src_storage_format == kUint8ClampedArrayStorageFormat)
                            ? kRGBA8CanvasPixelFormat
                            : kF16CanvasPixelFormat,
                        kNonOpaque)
          .GetSkColorSpaceForSkSurfaces();
  if (!src_sk_color_space.get())
    src_sk_color_space = SkColorSpace::MakeSRGB();

  sk_sp<SkColorSpace> dst_sk_color_space =
      CanvasColorParams(dst_color_space, dst_pixel_format, kNonOpaque)
          .GetSkColorSpaceForSkSurfaces();
  if (!dst_sk_color_space.get())
    dst_sk_color_space = SkColorSpace::MakeSRGB();

  std::unique_ptr<SkColorSpaceXform> xform = SkColorSpaceXform::New(
      src_sk_color_space.get(), dst_sk_color_space.get());
  bool conversion_result =
      xform->apply(dst_color_format, converted_pixels.get(), src_color_format,
                   src_data, num_pixels, kUnpremul_SkAlphaType);

  if (src_storage_format == kUint16ArrayStorageFormat) {
    for (int i = 0; i < num_elements; i++)
      u16_buffer[i] = base::ByteSwap(u16_buffer[i]);
  }
  return conversion_result;
}

bool ColorCorrectionTestUtils::MatchColorSpace(
    SkColorSpace* src_color_space,
    SkColorSpace* dst_color_space,
    float xyz_d50_component_tolerance) {
  if ((!src_color_space && dst_color_space) ||
      (src_color_space && !dst_color_space))
    return false;
  if (src_color_space) {
    const SkMatrix44* src_matrix = src_color_space->toXYZD50();
    const SkMatrix44* dst_matrix = dst_color_space->toXYZD50();
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (fabs(src_matrix->get(i, j) - dst_matrix->get(i, j)) >
            xyz_d50_component_tolerance) {
          return false;
        }
      }
    }
  }
  return true;
}

bool ColorCorrectionTestUtils::MatchSkImages(sk_sp<SkImage> src_image,
                                             sk_sp<SkImage> dst_image,
                                             unsigned uint8_tolerance,
                                             float f16_tolerance,
                                             float xyz_d50_component_tolerance,
                                             bool compare_alpha) {
  if ((!src_image && dst_image) || (src_image && !dst_image))
    return false;
  if (!src_image)
    return true;
  if ((src_image->width() != dst_image->width()) ||
      (src_image->height() != dst_image->height())) {
    return false;
  }

  if (compare_alpha && src_image->alphaType() != dst_image->alphaType())
    return false;
  if (src_image->makeRasterImage()->colorType() !=
      dst_image->makeRasterImage()->colorType()) {
    return false;
  }
  if (!MatchColorSpace(src_image->colorSpace(), dst_image->colorSpace(),
                       xyz_d50_component_tolerance)) {
    return false;
  }

  bool test_passed = true;
  int num_pixels = src_image->width() * src_image->height();
  int num_components = compare_alpha ? 4 : 3;

  SkImageInfo src_info = SkImageInfo::Make(
      src_image->width(), src_image->height(), kN32_SkColorType,
      src_image->alphaType(), src_image->refColorSpace());

  SkImageInfo dst_info = SkImageInfo::Make(
      dst_image->width(), dst_image->height(), kN32_SkColorType,
      src_image->alphaType(), dst_image->refColorSpace());

  if (src_image->colorType() != kRGBA_F16_SkColorType) {
    std::unique_ptr<uint8_t[]> src_pixels(new uint8_t[num_pixels * 4]());
    std::unique_ptr<uint8_t[]> dst_pixels(new uint8_t[num_pixels * 4]());

    src_image->readPixels(src_info, src_pixels.get(), src_info.minRowBytes(), 0,
                          0);
    dst_image->readPixels(dst_info, dst_pixels.get(), dst_info.minRowBytes(), 0,
                          0);

    for (int i = 0; test_passed && i < num_pixels; i++) {
      for (int j = 0; j < num_components; j++) {
        test_passed &= IsNearlyTheSame(src_pixels[i * 4 + j],
                                       dst_pixels[i * 4 + j], uint8_tolerance);
      }
    }
    return test_passed;
  }

  std::unique_ptr<uint16_t[]> src_pixels(new uint16_t[num_pixels * 4]());
  std::unique_ptr<uint16_t[]> dst_pixels(new uint16_t[num_pixels * 4]());

  src_info = src_info.makeColorType(kRGBA_F16_SkColorType);
  dst_info = dst_info.makeColorType(kRGBA_F16_SkColorType);

  src_image->readPixels(src_info, src_pixels.get(), src_info.minRowBytes(), 0,
                        0);
  dst_image->readPixels(dst_info, dst_pixels.get(), dst_info.minRowBytes(), 0,
                        0);

  for (int i = 0; test_passed && i < num_pixels; i++) {
    for (int j = 0; j < num_components; j++) {
      test_passed &=
          IsNearlyTheSame(Float16ToFloat(src_pixels[i * 4 + j]),
                          Float16ToFloat(dst_pixels[i * 4 + j]), f16_tolerance);
    }
  }
  return test_passed;
}

}  // namespace blink
