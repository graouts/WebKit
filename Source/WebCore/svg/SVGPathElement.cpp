/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGPathElement.h"

#include "CSSPathValue.h"
#include "ContainerNodeInlines.h"
#include "LegacyRenderSVGPath.h"
#include "LegacyRenderSVGResource.h"
#include "MatchResult.h"
#include "MutableStyleProperties.h"
#include "RenderSVGPath.h"
#include "ResolvedStyle.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGMPathElement.h"
#include "SVGNames.h"
#include "SVGPathUtilities.h"
#include "SVGPoint.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StylePropertiesInlines.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGPathElement);

class PathCache {
public:
    static PathCache& NODELETE singleton();

    std::optional<DataRef<SVGPathByteStream::Data>> get(const AtomString& attributeValue) const;
    void add(const AtomString& attributeValue, DataRef<SVGPathByteStream::Data>);
    void clear();

private:
    friend class NeverDestroyed<PathCache, MainThreadAccessTraits>;
    PathCache() = default;

    HashMap<AtomString, DataRef<SVGPathByteStream::Data>> m_cache;
    uint64_t m_sizeInBytes { 0 };
    static constexpr uint64_t maxItemSizeInBytes = 5 * 1024; // 5 Kb.
    static constexpr uint64_t maxCacheSizeInBytes = 150 * 1024; // 150 Kb.
};

PathCache& PathCache::singleton()
{
    static MainThreadNeverDestroyed<PathCache> cache;
    return cache;
}

std::optional<DataRef<SVGPathByteStream::Data>> PathCache::get(const AtomString& attributeValue) const
{
    return m_cache.getOptional(attributeValue);
}

void PathCache::add(const AtomString& attributeValue, DataRef<SVGPathByteStream::Data> data)
{
    size_t newDataSize = data->size();
    if (newDataSize > maxItemSizeInBytes) [[unlikely]]
        return;

    m_sizeInBytes += newDataSize;
    while (m_sizeInBytes > maxCacheSizeInBytes) {
        ASSERT(!m_cache.isEmpty());
        auto iteratorToRemove = m_cache.random();
        ASSERT(iteratorToRemove != m_cache.end());
        ASSERT(m_sizeInBytes >= iteratorToRemove->value->size());
        m_sizeInBytes -= iteratorToRemove->value->size();
        m_cache.remove(iteratorToRemove);
    }
    m_cache.add(attributeValue, WTF::move(data));
}

void PathCache::clear()
{
    m_cache.clear();
    m_sizeInBytes = 0;
}

inline SVGPathElement::SVGPathElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::pathTag));

    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::dAttr, &SVGPathElement::m_path>();
    }
}

Ref<SVGPathElement> SVGPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGPathElement(tagName, document));
}

void SVGPathElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::dAttr) {
        // This has to be decided before the attribute is parsed, while the computed `d` still refers
        // to the previous path data. Setting the current value again cannot change the computed `d`,
        // though re-parsing it produces new path data, which is swapped in when possible so the computed
        // `d` keeps referring to the attribute's own path data.
        if (canUpdateComputedDInPlace())
            m_dAttributeStyleUpdate = DAttributeStyleUpdate::InPlace;
        else
            m_dAttributeStyleUpdate = oldValue == newValue ? DAttributeStyleUpdate::None : DAttributeStyleUpdate::Invalidate;

        auto& cache = PathCache::singleton();
        Ref pathBaseVal = protect(m_path)->baseVal();
        if (newValue.isEmpty())
            pathBaseVal->clearByteStreamData();
        else if (auto data = cache.get(newValue))
            pathBaseVal->updateByteStreamData(WTF::move(data.value()));
        else if (pathBaseVal->parse(newValue))
            cache.add(newValue, pathBaseVal->existingPathByteStream().data());
        else
            protect(protect(document())->svgExtensions())->reportError(makeString("Problem parsing d=\""_s, newValue, "\""_s));
    } else if (oldValue != newValue && hasPresentationalHintsForAttribute(name)) {
        // Some other presentation attribute changed, so the whole presentational hint style has to
        // be collected again rather than just the `d` property.
        m_otherPresentationalHintsAreDirty = true;
    }

    SVGGeometryElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
    m_dAttributeStyleUpdate = DAttributeStyleUpdate::Invalidate;
}

void SVGPathElement::clearCache()
{
    PathCache::singleton().clear();
}

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::dAttr);
        InstanceInvalidationGuard guard(*this);
        invalidateMPathDependencies();

        if (auto* path = dynamicDowncast<RenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();

        if (auto* path = dynamicDowncast<LegacyRenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();

        updateSVGRendererForElementChange();
        if (document().settings().cssDPropertyEnabled()) {
            // Unless the computed `d` can be updated in place, only record that `d` went stale. Its value
            // has to be read later, when the presentational hint style is rebuilt during style resolution:
            // a SMIL animation of `d` has not necessarily committed its animated value by the time it gets here.
            switch (m_dAttributeStyleUpdate) {
            case DAttributeStyleUpdate::Invalidate:
                m_dPresentationalHintIsDirty = true;
                setPresentationalHintStyleIsDirty();
                break;
            case DAttributeStyleUpdate::InPlace:
                m_dPresentationalHintIsDirty = true;
                setPresentationalHintStyleIsDirty(InvalidateStyle::No);
                updateComputedDInPlace();
                break;
            case DAttributeStyleUpdate::None:
                break;
            }
        }
        invalidateResourceImageBuffersIfNeeded();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

void SVGPathElement::invalidateMPathDependencies()
{
    // <mpath> can only reference <path> but this dependency is not handled in
    // markForLayoutAndParentResourceInvalidation so we update any mpath dependencies manually.
    for (auto& element : referencingElements()) {
        if (RefPtr mpathElement = dynamicDowncast<SVGMPathElement>(element.get()))
            mpathElement->targetPathChanged();
    }
}

Node::NeedsPostConnectionSteps SVGPathElement::insertionSteps(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGGeometryElement::insertionSteps(insertionType, parentOfInsertedTree);
    invalidateMPathDependencies();
    return result;
}

void SVGPathElement::removingSteps(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGGeometryElement::removingSteps(removalType, oldParentOfRemovedTree);
    invalidateMPathDependencies();
}

float SVGPathElement::getTotalLength() const
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    return getTotalLengthOfSVGPathByteStream(pathByteStream());
}

ExceptionOr<Ref<SVGPoint>> SVGPathElement::getPointAtLength(float distance) const
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    // Spec: If it is not able to compute the total length of path, then throw.
    if (pathByteStream().isEmpty())
        return Exception { ExceptionCode::InvalidStateError, "The element's path is empty."_s };

    // Spec: Clamp distance to [0, length].
    distance = clampTo<float>(distance, 0, getTotalLength());

    // Spec: Return a newly created, detached SVGPoint object.
    return SVGPoint::create(getPointAtLengthOfSVGPathByteStream(pathByteStream(), distance));
}

unsigned SVGPathElement::getPathSegAtLength(float length) const
{
    protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    return getSVGPathSegAtLengthFromSVGPathByteStream(pathByteStream(), length);
}

FloatRect SVGPathElement::getBBox(StyleUpdateStrategy styleUpdateStrategy)
{
    if (styleUpdateStrategy == StyleUpdateStrategy::Allow)
        protect(document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, this);

    // FIXME: Eventually we should support getBBox for detached elements.
    // FIXME: If the path is null it means we're calling getBBox() before laying out this element,
    // which is an error.

    if (CheckedPtr path = dynamicDowncast<RenderSVGPath>(renderer()); path && path->hasPath())
        return path->path().boundingRect();

    if (CheckedPtr path = dynamicDowncast<LegacyRenderSVGPath>(renderer()); path && path->hasPath())
        return path->path().boundingRect();

    return { };
}

RenderPtr<RenderElement> SVGPathElement::createElementRenderer(Style::ComputedStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGPath>(*this, WTF::move(style));
    return createRenderer<LegacyRenderSVGPath>(*this, WTF::move(style));
}

const SVGPathByteStream& SVGPathElement::pathByteStream() const
{
    if (document().settings().cssDPropertyEnabled()) {
        if (CheckedPtr renderer = this->renderer()) {
            if (auto& pathFunction = renderer->style().d().tryPath())
                return pathFunction->parameters.data.byteStream;
            return SVGPathByteStream::empty();
        }

        if (CheckedPtr style = const_cast<SVGPathElement&>(*this).computedStyle()) {
            if (auto& pathFunction = style->d().tryPath())
                return pathFunction->parameters.data.byteStream;
            return SVGPathByteStream::empty();
        }
    }

    return Ref { m_path }->currentPathByteStream();
}

Path SVGPathElement::path() const
{
    if (document().settings().cssDPropertyEnabled()) {
        if (CheckedPtr renderer = this->renderer()) {
            CheckedRef style = renderer->style();
            if (auto& pathFunction = style->d().tryPath())
                return Style::path(pathFunction->parameters, FloatRect { }, style->usedZoomForLength());
            return { };
        }
    }

    return Ref { m_path }->currentPath();
}

void SVGPathElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == SVGNames::dAttr && document().settings().cssDPropertyEnabled())
        collectDPresentationalHint(style);
    else
        SVGGeometryElement::collectPresentationalHintsForAttribute(name, value, style);
}

void SVGPathElement::collectExtraStyleForPresentationalHints(MutableStyleProperties& style)
{
    if (!document().settings().cssDPropertyEnabled())
        return;
    if (!style.hasProperty(CSSPropertyD))
        collectDPresentationalHint(style);
}

void SVGPathElement::collectDPresentationalHint(MutableStyleProperties& style)
{
    ASSERT(document().settings().cssDPropertyEnabled());
    addPropertyToPresentationalHintStyle(style, cssPropertyIdForSVGAttributeName(SVGNames::dAttr), dPresentationalHintValue());
}

Ref<CSSValue> SVGPathElement::dPresentationalHintValue()
{
    // In the case of the `d` property, we want to avoid providing a string value since it will require
    // the path data to be parsed again and path data can be unwieldy.
    // The fill rule value passed here is not relevant for the `d` property.
    return CSSPathValue::create(CSS::PathFunction { CSS::Keyword::Nonzero { }, CSS::Path::Data { Ref { m_path }->currentPathByteStream() } });
}

bool SVGPathElement::updatePresentationalHintStyleForChangedProperties()
{
    // Both flags have to be consumed here, whether or not the fast path is taken, since the full
    // rebuild that follows covers every attribute.
    bool dIsDirty = std::exchange(m_dPresentationalHintIsDirty, false);
    bool othersAreDirty = std::exchange(m_otherPresentationalHintsAreDirty, false);

    // Zooming or panning an SVG chart rewrites `d` on every frame while the other presentation
    // attributes stay put, so swap the new path in rather than re-collecting (and re-parsing) them.
    return dIsDirty && !othersAreDirty
        && replacePresentationalHintStyleProperty(CSSPropertyD, dPresentationalHintValue());
}

std::optional<Style::UnadjustedStyle> SVGPathElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const Style::ComputedStyle* shadowHostStyle)
{
    auto unadjustedStyle = SVGGeometryElement::resolveCustomStyle(resolutionContext, shadowHostStyle);

    m_computedDIsFromAttribute = [&] {
        if (!unadjustedStyle || !unadjustedStyle->matchResult || correspondingElement())
            return false;

        // The computed `d` shares its path data with the attribute when the presentational hint won.
        auto& pathFunction = unadjustedStyle->style->d().tryPath();
        if (!pathFunction || pathFunction->parameters.data.byteStream.data().ptr() != Ref { m_path }->currentPathByteStream().data().ptr())
            return false;

        // Any other declaration that could set `d`, even to an identical value, makes the attribute
        // no longer the sole source of the computed value.
        RefPtr presentationalHintStyle = this->presentationalHintStyle();
        auto declaresD = [&](auto& declarations) {
            return std::ranges::any_of(declarations, [&](auto& matchedProperties) {
                Ref properties = matchedProperties.properties;
                if (properties.ptr() == presentationalHintStyle.get())
                    return false;
                return properties->findPropertyIndex(CSSPropertyD) != -1 || properties->findPropertyIndex(CSSPropertyAll) != -1;
            });
        };
        auto& matchResult = *unadjustedStyle->matchResult;
        return !declaresD(matchResult.userAgentDeclarations) && !declaresD(matchResult.userDeclarations) && !declaresD(matchResult.authorDeclarations);
    }();

    return unadjustedStyle;
}

bool SVGPathElement::presentationalHintChangeInvalidatesStyle(const QualifiedName& name) const
{
    return !(name == SVGNames::dAttr && m_dAttributeStyleUpdate == DAttributeStyleUpdate::InPlace);
}

// Changing only the `d` attribute can leave the computed style as it is but for `d` itself, provided that
// the attribute is what determines the computed `d` and nothing else observes the change. In that case the
// computed value is updated in place, which avoids a full style resolution for this element on every
// change, the way rewriting `d` behaves without the CSS `d` property.
bool SVGPathElement::canUpdateComputedDInPlace() const
{
    if (!document().settings().cssDPropertyEnabled() || !m_computedDIsFromAttribute)
        return false;

    // Style that is already invalid is about to be fully resolved anyway.
    if (needsStyleRecalc())
        return false;

    // A SMIL animation of `d` provides the path through the animated value.
    if (Ref { m_path }->isAnimating())
        return false;

    CheckedPtr renderer = this->renderer();
    if (!is<RenderSVGPath>(renderer) && !is<LegacyRenderSVGPath>(renderer))
        return false;

    // Make sure the computed style is still the one m_computedDIsFromAttribute was determined for.
    auto& style = renderer->style();
    auto& pathFunction = style.d().tryPath();
    if (!pathFunction || pathFunction->parameters.data.byteStream.data().ptr() != Ref { m_path }->currentPathByteStream().data().ptr())
        return false;

    // Transitions and animations need to see the change to start or be recomputed, and the before-change
    // style they rely on would otherwise hold a stale `d`.
    if (!style.transitions().isInitial() || hasKeyframeEffects({ }) || lastStyleChangeEventStyle({ }))
        return false;

    return true;
}

void SVGPathElement::updateComputedDInPlace()
{
    CheckedPtr renderer = this->renderer();
    ASSERT(renderer);
    auto& style = renderer->mutableStyle();
    auto pathFunction = *style.d().tryPath();
    pathFunction->data = { Ref { m_path }->currentPathByteStream() };
    style.setD(Style::SVGPathData { WTF::move(pathFunction) });
}

void SVGPathElement::pathDidChange()
{
    invalidateMPathDependencies();
}

}
