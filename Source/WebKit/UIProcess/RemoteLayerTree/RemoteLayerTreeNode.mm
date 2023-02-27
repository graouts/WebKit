/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteLayerTreeNode.h"

#import "RemoteLayerTreeLayers.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIView.h>
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#import <WebCore/AcceleratedEffectStack.h>
#import <WebCore/ScrollingThread.h>
#endif

namespace WebKit {

static NSString *const WKRemoteLayerTreeNodePropertyKey = @"WKRemoteLayerTreeNode";

RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::GraphicsLayer::PlatformLayerID layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<CALayer> layer)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer(WTFMove(layer))
{
    initializeLayer();
    [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
}

#if PLATFORM(IOS_FAMILY)
RemoteLayerTreeNode::RemoteLayerTreeNode(WebCore::GraphicsLayer::PlatformLayerID layerID, Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier, RetainPtr<UIView> uiView)
    : m_layerID(layerID)
    , m_remoteContextHostingIdentifier(hostIdentifier)
    , m_layer([uiView.get() layer])
    , m_uiView(WTFMove(uiView))
{
    initializeLayer();
}
#endif

RemoteLayerTreeNode::~RemoteLayerTreeNode()
{
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    if (m_effectStack)
        m_effectStack->clear(layer());
#endif
    [layer() setValue:nil forKey:WKRemoteLayerTreeNodePropertyKey];
}

std::unique_ptr<RemoteLayerTreeNode> RemoteLayerTreeNode::createWithPlainLayer(WebCore::GraphicsLayer::PlatformLayerID layerID)
{
    RetainPtr<CALayer> layer = adoptNS([[WKCompositingLayer alloc] init]);
    return makeUnique<RemoteLayerTreeNode>(layerID, std::nullopt, WTFMove(layer));
}

void RemoteLayerTreeNode::detachFromParent()
{
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    [interactionRegionsLayer() removeFromSuperlayer];
#endif

#if PLATFORM(IOS_FAMILY)
    if (auto view = uiView()) {
        [view removeFromSuperview];
        return;
    }
#endif
    [layer() removeFromSuperlayer];
}

void RemoteLayerTreeNode::setEventRegion(const WebCore::EventRegion& eventRegion)
{
    m_eventRegion = eventRegion;
}

void RemoteLayerTreeNode::initializeLayer()
{
    [layer() setValue:[NSValue valueWithPointer:this] forKey:WKRemoteLayerTreeNodePropertyKey];
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    m_interactionRegionsLayer = adoptNS([[CALayer alloc] init]);
    [m_interactionRegionsLayer setName:@"InteractionRegions Container"];
    [m_interactionRegionsLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
#endif
}

WebCore::GraphicsLayer::PlatformLayerID RemoteLayerTreeNode::layerID(CALayer *layer)
{
    auto* node = forCALayer(layer);
    return node ? node->layerID() : WebCore::GraphicsLayer::PlatformLayerID { };
}

RemoteLayerTreeNode* RemoteLayerTreeNode::forCALayer(CALayer *layer)
{
    return static_cast<RemoteLayerTreeNode*>([[layer valueForKey:WKRemoteLayerTreeNodePropertyKey] pointerValue]);
}

NSString *RemoteLayerTreeNode::appendLayerDescription(NSString *description, CALayer *layer)
{
    NSString *layerDescription = [NSString stringWithFormat:@" layerID = %llu \"%@\"", WebKit::RemoteLayerTreeNode::layerID(layer).object().toUInt64(), layer.name ? layer.name : @""];
    return [description stringByAppendingString:layerDescription];
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void RemoteLayerTreeNode::setAcceleratedEffectsAndBaseValues(const WebCore::AcceleratedEffects& effects, const WebCore::AcceleratedEffectValues& baseValues)
{
    ASSERT(isUIThread());

    WebCore::AcceleratedEffects clonedEffects;
    clonedEffects.reserveCapacity(effects.size());
    for (auto& effect : effects)
        clonedEffects.uncheckedAppend(effect->clone());

    auto clonedBaseValues = baseValues.clone();

    // FIXME: need to keep the node alive.
    WebCore::ScrollingThread::dispatch([this, clonedEffects = WTFMove(clonedEffects), clonedBaseValues = WTFMove(clonedBaseValues)] {
        if (!m_effectStack)
            m_effectStack = makeUnique<WebCore::AcceleratedEffectStack>();

        m_effectStack->setEffects(WTFMove(clonedEffects));
        m_effectStack->setBaseValues(WTFMove(clonedBaseValues));
    });
}

bool RemoteLayerTreeNode::hasAnimationEffects() const
{
    ASSERT(!isUIThread());
    return m_effectStack && m_effectStack->hasEffects();
}

void RemoteLayerTreeNode::applyAnimatedEffectStack(Seconds currentTime)
{
    ASSERT(!isUIThread());

    if (!m_effectStack)
        return;

    // The effect stack will have either primary layer effects or
    // backdrop layer effects. We call both application methods.
    m_effectStack->applyPrimaryLayerEffects(layer(), currentTime);
    m_effectStack->applyBackdropLayerEffects(layer(), currentTime);

    // We can clear the effect stack if it's empty, but the previous
    // call to applyEffects() is important so that the base values
    // were re-applied.
    if (!m_effectStack->hasEffects())
        m_effectStack = nullptr;
}
#endif

}
