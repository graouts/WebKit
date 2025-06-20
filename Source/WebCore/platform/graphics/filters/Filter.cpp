/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Filter.h"

#include "FilterEffect.h"
#include "FilterImage.h"
#include "FilterResults.h"
#include "FilterStyle.h"
#include "ImageBuffer.h"

namespace WebCore {

Filter::Filter(Filter::Type filterType, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : FilterFunction(filterType, renderingResourceIdentifier)
{
}

Filter::Filter(Filter::Type filterType, const FloatSize& filterScale, const FloatRect& filterRegion, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : FilterFunction(filterType, renderingResourceIdentifier)
    , m_filterScale(filterScale)
    , m_filterRegion(filterRegion)
{
}

FloatPoint Filter::scaledByFilterScale(const FloatPoint& point) const
{
    return point.scaled(m_filterScale.width(), m_filterScale.height());
}

FloatSize Filter::scaledByFilterScale(const FloatSize& size) const
{
    return size * m_filterScale;
}

FloatRect Filter::scaledByFilterScale(const FloatRect& rect) const
{
    auto scaledRect = rect;
    scaledRect.scale(m_filterScale);
    return scaledRect;
}

FloatRect Filter::maxEffectRect(const FloatRect& primitiveSubregion) const
{
    return intersection(primitiveSubregion, m_filterRegion);
}

FloatRect Filter::clipToMaxEffectRect(const FloatRect& imageRect, const FloatRect& primitiveSubregion) const
{
    auto maxEffectRect = this->maxEffectRect(primitiveSubregion);
    return intersection(imageRect, maxEffectRect);
}

bool Filter::clampFilterRegionIfNeeded()
{
    auto scaledFilterRegion = scaledByFilterScale(m_filterRegion);

    FloatSize clampingScale(1, 1);
    if (!ImageBuffer::sizeNeedsClamping(scaledFilterRegion.size(), clampingScale))
        return false;

    m_filterScale = m_filterScale * clampingScale;
    return true;
}

RenderingMode Filter::renderingMode() const
{
    if (m_filterRenderingModes.contains(FilterRenderingMode::Accelerated))
        return RenderingMode::Accelerated;
    
    ASSERT(m_filterRenderingModes.contains(FilterRenderingMode::Software));
    return RenderingMode::Unaccelerated;
}

void Filter::setFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes)
{
    m_filterRenderingModes = preferredFilterRenderingModes & supportedFilterRenderingModes();
    ASSERT(m_filterRenderingModes.contains(FilterRenderingMode::Software));
}

RefPtr<FilterImage> Filter::apply(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, FilterResults& results)
{
    RefPtr<FilterImage> input;

    if (sourceImage) {
        auto absoluteSourceImageRect = enclosingIntRect(scaledByFilterScale(sourceImageRect));
        input = FilterImage::create(m_filterRegion, sourceImageRect, absoluteSourceImageRect, Ref { *sourceImage }, results.allocator());
        if (!input)
            return nullptr;
    }

    auto result = apply(input.get(), results);
    if (!result)
        return nullptr;

    result->correctPremultipliedPixelBuffer();
    result->transformToColorSpace(DestinationColorSpace::SRGB());
    return result;
}

FilterStyleVector Filter::createFilterStyles(GraphicsContext& context, const FloatRect& sourceImageRect) const
{
    auto input = FilterStyle { std::nullopt, m_filterRegion, sourceImageRect };
    auto result = createFilterStyles(context, input);
    if (result.isEmpty())
        return { };

    result.reverse();
    result.shrinkToFit();
    return result;
}

} // namespace WebCore
