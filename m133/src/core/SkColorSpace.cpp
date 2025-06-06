/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/private/base/SkTemplates.h"
#include "modules/skcms/skcms.h"
#include "src/core/SkChecksum.h"
#include "src/core/SkColorSpacePriv.h"

#include <cmath>
#include <cstring>

namespace SkNamedPrimaries {

bool GetCicp(CicpId primaries, SkColorSpacePrimaries& sk_primaries) {
    // Rec. ITU-T H.273, Table 2.
    switch (primaries) {
        case CicpId::kRec709:
            sk_primaries = kRec709;
            return true;
        case CicpId::kRec470SystemM:
            sk_primaries = kRec470SystemM;
            return true;
        case CicpId::kRec470SystemBG:
            sk_primaries = kRec470SystemBG;
            return true;
        case CicpId::kRec601:
            sk_primaries = kRec601;
            return true;
        case CicpId::kSMPTE_ST_240:
            sk_primaries = kSMPTE_ST_240;
            return true;
        case CicpId::kGenericFilm:
            sk_primaries = kGenericFilm;
            return true;
        case CicpId::kRec2020:
            sk_primaries = kRec2020;
            return true;
        case CicpId::kSMPTE_ST_428_1:
            sk_primaries = kSMPTE_ST_428_1;
            return true;
        case CicpId::kSMPTE_RP_431_2:
            sk_primaries = kSMPTE_RP_431_2;
            return true;
        case CicpId::kSMPTE_EG_432_1:
            sk_primaries = kSMPTE_EG_432_1;
            return true;
        case CicpId::kITU_T_H273_Value22:
            sk_primaries = kITU_T_H273_Value22;
            return true;
        default:
            // Reserved or unimplemented.
            break;
    }
    return false;
}

}  // namespace SkNamedPrimaries

namespace SkNamedTransferFn {

bool GetCicp(SkNamedTransferFn::CicpId transfer_characteristics, skcms_TransferFunction& trfn) {
    // Rec. ITU-T H.273, Table 3.
    switch (transfer_characteristics) {
        case SkNamedTransferFn::CicpId::kRec709:
            trfn = kRec709;
            return true;
        case SkNamedTransferFn::CicpId::kRec470SystemM:
            trfn = kRec470SystemM;
            return true;
        case SkNamedTransferFn::CicpId::kRec470SystemBG:
            trfn = kRec470SystemBG;
            return true;
        case SkNamedTransferFn::CicpId::kRec601:
            trfn = kRec601;
            return true;
        case SkNamedTransferFn::CicpId::kSMPTE_ST_240:
            trfn = kSMPTE_ST_240;
            return true;
        case SkNamedTransferFn::CicpId::kLinear:
            trfn = SkNamedTransferFn::kLinear;
            return true;
        case SkNamedTransferFn::CicpId::kIEC61966_2_4:
            trfn = kIEC61966_2_4;
            break;
        case SkNamedTransferFn::CicpId::kIEC61966_2_1:
            trfn = SkNamedTransferFn::kIEC61966_2_1;
            return true;
        case SkNamedTransferFn::CicpId::kRec2020_10bit:
            trfn = kRec2020_10bit;
            return true;
        case SkNamedTransferFn::CicpId::kRec2020_12bit:
            trfn = kRec2020_12bit;
            return true;
        case SkNamedTransferFn::CicpId::kPQ:
            trfn = SkNamedTransferFn::kPQ;
            return true;
        case SkNamedTransferFn::CicpId::kSMPTE_ST_428_1:
            trfn = kSMPTE_ST_428_1;
            return true;
        case SkNamedTransferFn::CicpId::kHLG:
            trfn = SkNamedTransferFn::kHLG;
            return true;
        default:
            // Reserved or unimplemented.
            break;
    }
    return false;
}

}  // namespace SkNamedTransferFn

bool SkColorSpacePrimaries::toXYZD50(skcms_Matrix3x3* toXYZ_D50) const {
    return skcms_PrimariesToXYZD50(fRX, fRY, fGX, fGY, fBX, fBY, fWX, fWY, toXYZ_D50);
}

SkColorSpace::SkColorSpace(const skcms_TransferFunction& transferFn,
                           const skcms_Matrix3x3& toXYZD50)
        : fTransferFn(transferFn)
        , fToXYZD50(toXYZD50) {
    fTransferFnHash = SkChecksum::Hash32(&fTransferFn, 7*sizeof(float));
    fToXYZD50Hash = SkChecksum::Hash32(&fToXYZD50, 9*sizeof(float));
    hasCicp = false;
}

static bool xyz_almost_equal(const skcms_Matrix3x3& mA, const skcms_Matrix3x3& mB) {
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (!color_space_almost_equal(mA.vals[r][c], mB.vals[r][c])) {
                return false;
            }
        }
    }

    return true;
}

sk_sp<SkColorSpace> SkColorSpace::MakeRGB(const skcms_TransferFunction& transferFn,
                                          const skcms_Matrix3x3& toXYZ) {
    if (skcms_TransferFunction_getType(&transferFn) == skcms_TFType_Invalid) {
        return nullptr;
    }

    const skcms_TransferFunction* tf = &transferFn;

    if (is_almost_srgb(transferFn)) {
        if (xyz_almost_equal(toXYZ, SkNamedGamut::kSRGB)) {
            return SkColorSpace::MakeSRGB();
        }
        tf = &SkNamedTransferFn::kSRGB;
    } else if (is_almost_2dot2(transferFn)) {
        tf = &SkNamedTransferFn::k2Dot2;
    } else if (is_almost_linear(transferFn)) {
        if (xyz_almost_equal(toXYZ, SkNamedGamut::kSRGB)) {
            return SkColorSpace::MakeSRGBLinear();
        }
        tf = &SkNamedTransferFn::kLinear;
    }

    return sk_sp<SkColorSpace>(new SkColorSpace(*tf, toXYZ));
}

sk_sp<SkColorSpace> SkColorSpace::MakeCICP(SkNamedPrimaries::CicpId color_primaries,
                                           SkNamedTransferFn::CicpId transfer_characteristics) {
    skcms_TransferFunction trfn;
    if (!SkNamedTransferFn::GetCicp(transfer_characteristics, trfn)) {
        return nullptr;
    }

    SkColorSpacePrimaries primaries;
    if (!SkNamedPrimaries::GetCicp(color_primaries, primaries)) {
        return nullptr;
    }

    skcms_Matrix3x3 primaries_matrix;
    if (!primaries.toXYZD50(&primaries_matrix)) {
        return nullptr;
    }

    return SkColorSpace::MakeRGB(trfn, primaries_matrix);
}

class SkColorSpaceSingletonFactory {
public:
    static SkColorSpace* Make(const skcms_TransferFunction& transferFn,
                              const skcms_Matrix3x3& to_xyz) {
        return new SkColorSpace(transferFn, to_xyz);
    }
};

SkColorSpace* sk_srgb_singleton() {
    static SkColorSpace* cs = SkColorSpaceSingletonFactory::Make(SkNamedTransferFn::kSRGB,
                                                                 SkNamedGamut::kSRGB);
    return cs;
}

SkColorSpace* sk_srgb_linear_singleton() {
    static SkColorSpace* cs = SkColorSpaceSingletonFactory::Make(SkNamedTransferFn::kLinear,
                                                                 SkNamedGamut::kSRGB);
    return cs;
}

sk_sp<SkColorSpace> SkColorSpace::MakeSRGB() {
    return sk_ref_sp(sk_srgb_singleton());
}

sk_sp<SkColorSpace> SkColorSpace::MakeSRGBLinear() {
    return sk_ref_sp(sk_srgb_linear_singleton());
}

void SkColorSpace::computeLazyDstFields() const {
    fLazyDstFieldsOnce([this] {

        // Invert 3x3 gamut, defaulting to sRGB if we can't.
        {
            if (!skcms_Matrix3x3_invert(&fToXYZD50, &fFromXYZD50)) {
                SkAssertResult(skcms_Matrix3x3_invert(&skcms_sRGB_profile()->toXYZD50,
                                                      &fFromXYZD50));
            }
        }

        // Invert transfer function, defaulting to sRGB if we can't.
        {
            if (!skcms_TransferFunction_invert(&fTransferFn, &fInvTransferFn)) {
                fInvTransferFn = *skcms_sRGB_Inverse_TransferFunction();
            }
        }

    });
}

bool SkColorSpace::isNumericalTransferFn(skcms_TransferFunction* coeffs) const {
    // TODO: Change transferFn/invTransferFn to just operate on skcms_TransferFunction (all callers
    // already pass pointers to an skcms struct). Then remove this function, and update the two
    // remaining callers to do the right thing with transferFn and classify.
    this->transferFn(coeffs);
    return skcms_TransferFunction_getType(coeffs) == skcms_TFType_sRGBish;
}

void SkColorSpace::transferFn(float gabcdef[7]) const {
    memcpy(gabcdef, &fTransferFn, 7*sizeof(float));
}

void SkColorSpace::transferFn(skcms_TransferFunction* fn) const {
    *fn = fTransferFn;
}

void SkColorSpace::invTransferFn(skcms_TransferFunction* fn) const {
    this->computeLazyDstFields();
    *fn = fInvTransferFn;
}

bool SkColorSpace::toXYZD50(skcms_Matrix3x3* toXYZD50) const {
    *toXYZD50 = fToXYZD50;
    return true;
}

void SkColorSpace::gamutTransformTo(const SkColorSpace* dst, skcms_Matrix3x3* src_to_dst) const {
    dst->computeLazyDstFields();
    *src_to_dst = skcms_Matrix3x3_concat(&dst->fFromXYZD50, &fToXYZD50);
}

bool SkColorSpace::isSRGB() const {
    return sk_srgb_singleton() == this;
}

bool SkColorSpace::gammaCloseToSRGB() const {
    // Nearly-equal transfer functions were snapped at construction time, so just do an exact test
    return memcmp(&fTransferFn, &SkNamedTransferFn::kSRGB, 7*sizeof(float)) == 0;
}

bool SkColorSpace::gammaIsLinear() const {
    // Nearly-equal transfer functions were snapped at construction time, so just do an exact test
    return memcmp(&fTransferFn, &SkNamedTransferFn::kLinear, 7*sizeof(float)) == 0;
}

sk_sp<SkColorSpace> SkColorSpace::makeLinearGamma() const {
    if (this->gammaIsLinear()) {
        return sk_ref_sp(const_cast<SkColorSpace*>(this));
    }
    return SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear, fToXYZD50);
}

sk_sp<SkColorSpace> SkColorSpace::makeSRGBGamma() const {
    if (this->gammaCloseToSRGB()) {
        return sk_ref_sp(const_cast<SkColorSpace*>(this));
    }
    return SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, fToXYZD50);
}

sk_sp<SkColorSpace> SkColorSpace::makeColorSpin() const {
    skcms_Matrix3x3 spin = {{
        { 0, 0, 1 },
        { 1, 0, 0 },
        { 0, 1, 0 },
    }};

    skcms_Matrix3x3 spun = skcms_Matrix3x3_concat(&fToXYZD50, &spin);

    return sk_sp<SkColorSpace>(new SkColorSpace(fTransferFn, spun));
}

void SkColorSpace::SetIccCicp(const skcms_CICP cicp) {
    fcicp = cicp;
    hasCicp = true;
}

bool SkColorSpace::GetIccCicp(skcms_CICP* cicp) const {
    if (hasCicp) {
        *cicp = fcicp;
        return true;
    }
    return false;
}

void SkColorSpace::toProfile(skcms_ICCProfile* profile) const {
    skcms_Init               (profile);
    skcms_SetTransferFunction(profile, &fTransferFn);
    skcms_SetXYZD50          (profile, &fToXYZD50);
}

sk_sp<SkColorSpace> SkColorSpace::Make(const skcms_ICCProfile& profile) {
    // TODO: move below ≈sRGB test?
    if (!profile.has_toXYZD50 || !profile.has_trc) {
        return nullptr;
    }

    if (skcms_ApproximatelyEqualProfiles(&profile, skcms_sRGB_profile())) {
        return SkColorSpace::MakeSRGB();
    }

    // TODO: can we save this work and skip lazily inverting the matrix later?
    skcms_Matrix3x3 inv;
    if (!skcms_Matrix3x3_invert(&profile.toXYZD50, &inv)) {
        return nullptr;
    }

    // We can't work with tables or mismatched parametric curves,
    // but if they all look close enough to sRGB, that's fine.
    // TODO: should we maybe do this unconditionally to snap near-sRGB parametrics to sRGB?
    const skcms_Curve* trc = profile.trc;
    if (trc[0].table_entries != 0 ||
        trc[1].table_entries != 0 ||
        trc[2].table_entries != 0 ||
        0 != memcmp(&trc[0].parametric, &trc[1].parametric, sizeof(trc[0].parametric)) ||
        0 != memcmp(&trc[0].parametric, &trc[2].parametric, sizeof(trc[0].parametric)))
    {
        if (skcms_TRCs_AreApproximateInverse(&profile, skcms_sRGB_Inverse_TransferFunction())) {
            return SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, profile.toXYZD50);
        }
        return nullptr;
    }

    return SkColorSpace::MakeRGB(profile.trc[0].parametric, profile.toXYZD50);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

enum Version {
    k0_Version, // Initial (deprecated) version, no longer supported
    k1_Version, // Simple header (version tag) + 16 floats

    kCurrent_Version = k1_Version,
};

struct ColorSpaceHeader {
    uint8_t fVersion = kCurrent_Version;

    // Other fields were only used by k0_Version. Could be re-purposed in future versions.
    uint8_t fReserved0 = 0;
    uint8_t fReserved1 = 0;
    uint8_t fReserved2 = 0;
};

size_t SkColorSpace::writeToMemory(void* memory) const {
    if (memory) {
        *((ColorSpaceHeader*) memory) = ColorSpaceHeader();
        memory = SkTAddOffset<void>(memory, sizeof(ColorSpaceHeader));

        memcpy(memory, &fTransferFn, 7 * sizeof(float));
        memory = SkTAddOffset<void>(memory, 7 * sizeof(float));

        memcpy(memory, &fToXYZD50, 9 * sizeof(float));
    }

    return sizeof(ColorSpaceHeader) + 16 * sizeof(float);
}

sk_sp<SkData> SkColorSpace::serialize() const {
    sk_sp<SkData> data = SkData::MakeUninitialized(this->writeToMemory(nullptr));
    this->writeToMemory(data->writable_data());
    return data;
}

sk_sp<SkColorSpace> SkColorSpace::Deserialize(const void* data, size_t length) {
    if (length < sizeof(ColorSpaceHeader)) {
        return nullptr;
    }

    ColorSpaceHeader header = *((const ColorSpaceHeader*) data);
    data = SkTAddOffset<const void>(data, sizeof(ColorSpaceHeader));
    length -= sizeof(ColorSpaceHeader);
    if (header.fVersion != k1_Version) {
        return nullptr;
    }

    if (length < 16 * sizeof(float)) {
        return nullptr;
    }

    skcms_TransferFunction transferFn;
    memcpy(&transferFn, data, 7 * sizeof(float));
    data = SkTAddOffset<const void>(data, 7 * sizeof(float));

    skcms_Matrix3x3 toXYZ;
    memcpy(&toXYZ, data, 9 * sizeof(float));
    return SkColorSpace::MakeRGB(transferFn, toXYZ);
}

bool SkColorSpace::Equals(const SkColorSpace* x, const SkColorSpace* y) {
    if (x == y) {
        return true;
    }

    if (!x || !y) {
        return false;
    }

    if (x->hash() == y->hash()) {
    #if defined(SK_DEBUG)
        // Do these floats function equivalently?
        // This returns true more often than simple float comparison   (NaN vs. NaN) and,
        // also returns true more often than simple bitwise comparison (+0 vs. -0) and,
        // even returns true more often than those two OR'd together   (two different NaNs).
        auto equiv = [](float X, float Y) {
            return (X==Y)
                || (std::isnan(X) && std::isnan(Y));
        };

        for (int i = 0; i < 7; i++) {
            float X = (&x->fTransferFn.g)[i],
                  Y = (&y->fTransferFn.g)[i];
            SkASSERTF(equiv(X,Y), "Hash collision at tf[%d], !equiv(%g,%g)\n", i, X,Y);
        }
        for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            float X = x->fToXYZD50.vals[r][c],
                  Y = y->fToXYZD50.vals[r][c];
            SkASSERTF(equiv(X,Y), "Hash collision at toXYZD50[%d][%d], !equiv(%g,%g)\n", r,c, X,Y);
        }
    #endif
        return true;
    }
    return false;
}
