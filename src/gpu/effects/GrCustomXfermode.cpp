/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/effects/GrCustomXfermode.h"

#include "src/gpu/GrCaps.h"
#include "src/gpu/GrFragmentProcessor.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrProcessor.h"
#include "src/gpu/GrShaderCaps.h"
#include "src/gpu/GrXferProcessor.h"
#include "src/gpu/glsl/GrGLSLBlend.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"

bool GrCustomXfermode::IsSupportedMode(SkBlendMode mode) {
    return (int)mode  > (int)SkBlendMode::kLastCoeffMode &&
           (int)mode <= (int)SkBlendMode::kLastMode;
}

///////////////////////////////////////////////////////////////////////////////
// Static helpers
///////////////////////////////////////////////////////////////////////////////

static constexpr GrBlendEquation hw_blend_equation(SkBlendMode mode) {
    constexpr int kEqOffset = (kOverlay_GrBlendEquation - (int)SkBlendMode::kOverlay);
    static_assert(kOverlay_GrBlendEquation == (int)SkBlendMode::kOverlay + kEqOffset);
    static_assert(kDarken_GrBlendEquation == (int)SkBlendMode::kDarken + kEqOffset);
    static_assert(kLighten_GrBlendEquation == (int)SkBlendMode::kLighten + kEqOffset);
    static_assert(kColorDodge_GrBlendEquation == (int)SkBlendMode::kColorDodge + kEqOffset);
    static_assert(kColorBurn_GrBlendEquation == (int)SkBlendMode::kColorBurn + kEqOffset);
    static_assert(kHardLight_GrBlendEquation == (int)SkBlendMode::kHardLight + kEqOffset);
    static_assert(kSoftLight_GrBlendEquation == (int)SkBlendMode::kSoftLight + kEqOffset);
    static_assert(kDifference_GrBlendEquation == (int)SkBlendMode::kDifference + kEqOffset);
    static_assert(kExclusion_GrBlendEquation == (int)SkBlendMode::kExclusion + kEqOffset);
    static_assert(kMultiply_GrBlendEquation == (int)SkBlendMode::kMultiply + kEqOffset);
    static_assert(kHSLHue_GrBlendEquation == (int)SkBlendMode::kHue + kEqOffset);
    static_assert(kHSLSaturation_GrBlendEquation == (int)SkBlendMode::kSaturation + kEqOffset);
    static_assert(kHSLColor_GrBlendEquation == (int)SkBlendMode::kColor + kEqOffset);
    static_assert(kHSLLuminosity_GrBlendEquation == (int)SkBlendMode::kLuminosity + kEqOffset);

    // There's an illegal GrBlendEquation that corresponds to no SkBlendMode, hence the extra +1.
    static_assert(kGrBlendEquationCnt == (int)SkBlendMode::kLastMode + 1 + 1 + kEqOffset);

    return static_cast<GrBlendEquation>((int)mode + kEqOffset);
#undef EQ_OFFSET
}

static bool can_use_hw_blend_equation(GrBlendEquation equation,
                                      GrProcessorAnalysisCoverage coverage, const GrCaps& caps) {
    if (!caps.advancedBlendEquationSupport()) {
        return false;
    }
    if (GrProcessorAnalysisCoverage::kLCD == coverage) {
        return false; // LCD coverage must be applied after the blend equation.
    }
    if (caps.isAdvancedBlendEquationDisabled(equation)) {
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Xfer Processor
///////////////////////////////////////////////////////////////////////////////

class CustomXP : public GrXferProcessor {
public:
    CustomXP(SkBlendMode mode, GrBlendEquation hwBlendEquation)
        : INHERITED(kCustomXP_ClassID)
        , fMode(mode)
        , fHWBlendEquation(hwBlendEquation) {}

    CustomXP(SkBlendMode mode, GrProcessorAnalysisCoverage coverage)
            : INHERITED(kCustomXP_ClassID, /*willReadDstColor=*/true, coverage)
            , fMode(mode)
            , fHWBlendEquation(kIllegal_GrBlendEquation) {
    }

    const char* name() const override { return "Custom Xfermode"; }

    SkString getShaderDfxInfo() const override;

    std::unique_ptr<ProgramImpl> makeProgramImpl() const override;

    GrXferBarrierType xferBarrierType(const GrCaps&) const override;

private:
    bool hasHWBlendEquation() const { return kIllegal_GrBlendEquation != fHWBlendEquation; }

    void onAddToKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;

    void onGetBlendInfo(BlendInfo*) const override;

    bool onIsEqual(const GrXferProcessor& xpBase) const override;

    const SkBlendMode      fMode;
    const GrBlendEquation  fHWBlendEquation;

    using INHERITED = GrXferProcessor;
};

SkString CustomXP::getShaderDfxInfo() const
{
    SkString format;
    format.printf("ShaderDfx_CustomXP_%d_%d", hasHWBlendEquation(), fMode);
    return format;
}

void CustomXP::onAddToKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const {
    uint32_t key = 0;
    if (this->hasHWBlendEquation()) {
        SkASSERT(caps.advBlendEqInteraction() > 0);  // 0 will mean !xp.hasHWBlendEquation().
        key |= caps.advBlendEqInteraction();
        static_assert(GrShaderCaps::kLast_AdvBlendEqInteraction < 4);
    } else {
        key |= static_cast<int>(fMode) << 3;
    }
    b->add32(key);
}

std::unique_ptr<GrXferProcessor::ProgramImpl> CustomXP::makeProgramImpl() const {
    SkASSERT(this->willReadDstColor() != this->hasHWBlendEquation());

    class Impl : public ProgramImpl {
    private:
        void emitOutputsForBlendState(const EmitArgs& args) override {
            const CustomXP& xp = args.fXP.cast<CustomXP>();
            SkASSERT(xp.hasHWBlendEquation());

            GrGLSLXPFragmentBuilder* fragBuilder = args.fXPFragBuilder;
            fragBuilder->enableAdvancedBlendEquationIfNeeded(xp.fHWBlendEquation);

            // Apply coverage by multiplying it into the src color before blending. This will "just
            // work" automatically. (See analysisProperties())
            fragBuilder->codeAppendf("%s = %s * %s;",
                                     args.fOutputPrimary,
                                     args.fInputCoverage,
                                     args.fInputColor);
        }

        void emitBlendCodeForDstRead(GrGLSLXPFragmentBuilder* fragBuilder,
                                     GrGLSLUniformHandler* uniformHandler,
                                     const char* srcColor,
                                     const char* srcCoverage,
                                     const char* dstColor,
                                     const char* outColor,
                                     const char* outColorSecondary,
                                     const GrXferProcessor& proc) override {
            const CustomXP& xp = proc.cast<CustomXP>();
            SkASSERT(!xp.hasHWBlendEquation());

            GrGLSLBlend::AppendMode(fragBuilder, srcColor, dstColor, outColor, xp.fMode);

            // Apply coverage.
            DefaultCoverageModulation(fragBuilder,
                                      srcCoverage,
                                      dstColor,
                                      outColor,
                                      outColorSecondary,
                                      xp);
        }
    };

    return std::make_unique<Impl>();
}

bool CustomXP::onIsEqual(const GrXferProcessor& other) const {
    const CustomXP& s = other.cast<CustomXP>();
    return fMode == s.fMode && fHWBlendEquation == s.fHWBlendEquation;
}

GrXferBarrierType CustomXP::xferBarrierType(const GrCaps& caps) const {
    if (this->hasHWBlendEquation() && !caps.advancedCoherentBlendEquationSupport()) {
        return kBlend_GrXferBarrierType;
    }
    return kNone_GrXferBarrierType;
}

void CustomXP::onGetBlendInfo(BlendInfo* blendInfo) const {
    if (this->hasHWBlendEquation()) {
        blendInfo->fEquation = fHWBlendEquation;
    }
}

///////////////////////////////////////////////////////////////////////////////

// See the comment above GrXPFactory's definition about this warning suppression.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif
class CustomXPFactory : public GrXPFactory {
public:
    constexpr CustomXPFactory(SkBlendMode mode)
            : fMode(mode), fHWBlendEquation(hw_blend_equation(mode)) {}

private:
    sk_sp<const GrXferProcessor> makeXferProcessor(const GrProcessorAnalysisColor&,
                                                   GrProcessorAnalysisCoverage,
                                                   const GrCaps&,
                                                   GrClampType) const override;

    AnalysisProperties analysisProperties(const GrProcessorAnalysisColor&,
                                          const GrProcessorAnalysisCoverage&,
                                          const GrCaps&,
                                          GrClampType) const override;

    GR_DECLARE_XP_FACTORY_TEST

    SkBlendMode fMode;
    GrBlendEquation fHWBlendEquation;

    using INHERITED = GrXPFactory;
};
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

sk_sp<const GrXferProcessor> CustomXPFactory::makeXferProcessor(
        const GrProcessorAnalysisColor&,
        GrProcessorAnalysisCoverage coverage,
        const GrCaps& caps,
        GrClampType clampType) const {
    SkASSERT(GrCustomXfermode::IsSupportedMode(fMode));
    if (can_use_hw_blend_equation(fHWBlendEquation, coverage, caps)) {
        return sk_sp<GrXferProcessor>(new CustomXP(fMode, fHWBlendEquation));
    }
    return sk_sp<GrXferProcessor>(new CustomXP(fMode, coverage));
}

GrXPFactory::AnalysisProperties CustomXPFactory::analysisProperties(
        const GrProcessorAnalysisColor&, const GrProcessorAnalysisCoverage& coverage,
        const GrCaps& caps, GrClampType clampType) const {
    /*
      The general SVG blend equation is defined in the spec as follows:

        Dca' = B(Sc, Dc) * Sa * Da + Y * Sca * (1-Da) + Z * Dca * (1-Sa)
        Da'  = X * Sa * Da + Y * Sa * (1-Da) + Z * Da * (1-Sa)

      (Note that Sca, Dca indicate RGB vectors that are premultiplied by alpha,
       and that B(Sc, Dc) is a mode-specific function that accepts non-multiplied
       RGB colors.)

      For every blend mode supported by this class, i.e. the "advanced" blend
      modes, X=Y=Z=1 and this equation reduces to the PDF blend equation.

      It can be shown that when X=Y=Z=1, these equations can modulate alpha for
      coverage.


      == Color ==

      We substitute Y=Z=1 and define a blend() function that calculates Dca' in
      terms of premultiplied alpha only:

        blend(Sca, Dca, Sa, Da) = {Dca : if Sa == 0,
                                   Sca : if Da == 0,
                                   B(Sca/Sa, Dca/Da) * Sa * Da + Sca * (1-Da) + Dca * (1-Sa) : if
      Sa,Da != 0}

      And for coverage modulation, we use a post blend src-over model:

        Dca'' = f * blend(Sca, Dca, Sa, Da) + (1-f) * Dca

      (Where f is the fractional coverage.)

      Next we show that canTweakAlphaForCoverage() is true by proving the
      following relationship:

        blend(f*Sca, Dca, f*Sa, Da) == f * blend(Sca, Dca, Sa, Da) + (1-f) * Dca

      General case (f,Sa,Da != 0):

        f * blend(Sca, Dca, Sa, Da) + (1-f) * Dca
          = f * (B(Sca/Sa, Dca/Da) * Sa * Da + Sca * (1-Da) + Dca * (1-Sa)) + (1-f) * Dca  [Sa,Da !=
      0, definition of blend()]
          = B(Sca/Sa, Dca/Da) * f*Sa * Da + f*Sca * (1-Da) + f*Dca * (1-Sa) + Dca - f*Dca
          = B(Sca/Sa, Dca/Da) * f*Sa * Da + f*Sca - f*Sca * Da + f*Dca - f*Dca * Sa + Dca - f*Dca
          = B(Sca/Sa, Dca/Da) * f*Sa * Da + f*Sca - f*Sca * Da - f*Dca * Sa + Dca
          = B(Sca/Sa, Dca/Da) * f*Sa * Da + f*Sca * (1-Da) - f*Dca * Sa + Dca
          = B(Sca/Sa, Dca/Da) * f*Sa * Da + f*Sca * (1-Da) + Dca * (1 - f*Sa)
          = B(f*Sca/f*Sa, Dca/Da) * f*Sa * Da + f*Sca * (1-Da) + Dca * (1 - f*Sa)  [f!=0]
          = blend(f*Sca, Dca, f*Sa, Da)  [definition of blend()]

      Corner cases (Sa=0, Da=0, and f=0):

        Sa=0: f * blend(Sca, Dca, Sa, Da) + (1-f) * Dca
                = f * Dca + (1-f) * Dca  [Sa=0, definition of blend()]
                = Dca
                = blend(0, Dca, 0, Da)  [definition of blend()]
                = blend(f*Sca, Dca, f*Sa, Da)  [Sa=0]

        Da=0: f * blend(Sca, Dca, Sa, Da) + (1-f) * Dca
                = f * Sca + (1-f) * Dca  [Da=0, definition of blend()]
                = f * Sca  [Da=0]
                = blend(f*Sca, 0, f*Sa, 0)  [definition of blend()]
                = blend(f*Sca, Dca, f*Sa, Da)  [Da=0]

        f=0: f * blend(Sca, Dca, Sa, Da) + (1-f) * Dca
               = Dca  [f=0]
               = blend(0, Dca, 0, Da)  [definition of blend()]
               = blend(f*Sca, Dca, f*Sa, Da)  [f=0]

      == Alpha ==

      We substitute X=Y=Z=1 and define a blend() function that calculates Da':

        blend(Sa, Da) = Sa * Da + Sa * (1-Da) + Da * (1-Sa)
                      = Sa * Da + Sa - Sa * Da + Da - Da * Sa
                      = Sa + Da - Sa * Da

      We use the same model for coverage modulation as we did with color:

        Da'' = f * blend(Sa, Da) + (1-f) * Da

      And show that canTweakAlphaForCoverage() is true by proving the following
      relationship:

        blend(f*Sa, Da) == f * blend(Sa, Da) + (1-f) * Da


        f * blend(Sa, Da) + (1-f) * Da
          = f * (Sa + Da - Sa * Da) + (1-f) * Da
          = f*Sa + f*Da - f*Sa * Da + Da - f*Da
          = f*Sa - f*Sa * Da + Da
          = f*Sa + Da - f*Sa * Da
          = blend(f*Sa, Da)
    */
    if (can_use_hw_blend_equation(fHWBlendEquation, coverage, caps)) {
        if (caps.blendEquationSupport() == GrCaps::kAdvancedCoherent_BlendEquationSupport) {
            return AnalysisProperties::kCompatibleWithCoverageAsAlpha;
        } else {
            return AnalysisProperties::kCompatibleWithCoverageAsAlpha |
                   AnalysisProperties::kRequiresNonOverlappingDraws |
                   AnalysisProperties::kUsesNonCoherentHWBlending;
        }
    }
    return AnalysisProperties::kCompatibleWithCoverageAsAlpha |
           AnalysisProperties::kReadsDstInShader;
}

GR_DEFINE_XP_FACTORY_TEST(CustomXPFactory);
#if GR_TEST_UTILS
const GrXPFactory* CustomXPFactory::TestGet(GrProcessorTestData* d) {
    int mode = d->fRandom->nextRangeU((int)SkBlendMode::kLastCoeffMode + 1,
                                      (int)SkBlendMode::kLastSeparableMode);

    return GrCustomXfermode::Get((SkBlendMode)mode);
}
#endif

///////////////////////////////////////////////////////////////////////////////

const GrXPFactory* GrCustomXfermode::Get(SkBlendMode mode) {
    static constexpr const CustomXPFactory gOverlay(SkBlendMode::kOverlay);
    static constexpr const CustomXPFactory gDarken(SkBlendMode::kDarken);
    static constexpr const CustomXPFactory gLighten(SkBlendMode::kLighten);
    static constexpr const CustomXPFactory gColorDodge(SkBlendMode::kColorDodge);
    static constexpr const CustomXPFactory gColorBurn(SkBlendMode::kColorBurn);
    static constexpr const CustomXPFactory gHardLight(SkBlendMode::kHardLight);
    static constexpr const CustomXPFactory gSoftLight(SkBlendMode::kSoftLight);
    static constexpr const CustomXPFactory gDifference(SkBlendMode::kDifference);
    static constexpr const CustomXPFactory gExclusion(SkBlendMode::kExclusion);
    static constexpr const CustomXPFactory gMultiply(SkBlendMode::kMultiply);
    static constexpr const CustomXPFactory gHue(SkBlendMode::kHue);
    static constexpr const CustomXPFactory gSaturation(SkBlendMode::kSaturation);
    static constexpr const CustomXPFactory gColor(SkBlendMode::kColor);
    static constexpr const CustomXPFactory gLuminosity(SkBlendMode::kLuminosity);
    switch (mode) {
        case SkBlendMode::kOverlay:
            return &gOverlay;
        case SkBlendMode::kDarken:
            return &gDarken;
        case SkBlendMode::kLighten:
            return &gLighten;
        case SkBlendMode::kColorDodge:
            return &gColorDodge;
        case SkBlendMode::kColorBurn:
            return &gColorBurn;
        case SkBlendMode::kHardLight:
            return &gHardLight;
        case SkBlendMode::kSoftLight:
            return &gSoftLight;
        case SkBlendMode::kDifference:
            return &gDifference;
        case SkBlendMode::kExclusion:
            return &gExclusion;
        case SkBlendMode::kMultiply:
            return &gMultiply;
        case SkBlendMode::kHue:
            return &gHue;
        case SkBlendMode::kSaturation:
            return &gSaturation;
        case SkBlendMode::kColor:
            return &gColor;
        case SkBlendMode::kLuminosity:
            return &gLuminosity;
        default:
            SkASSERT(!GrCustomXfermode::IsSupportedMode(mode));
            return nullptr;
    }
}
