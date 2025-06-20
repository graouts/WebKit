/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "APIObject.h"
#include "FrameInfoData.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/MediaDeviceHashSalts.h>
#include <WebCore/MediaStreamRequest.h>
#include <WebCore/UserMediaRequestIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class UserMediaPermissionRequestManagerProxy;

class UserMediaPermissionRequestProxy : public API::ObjectImpl<API::Object::Type::UserMediaPermissionRequest> {
public:
    static Ref<UserMediaPermissionRequestProxy> create(UserMediaPermissionRequestManagerProxy&, std::optional<WebCore::UserMediaRequestIdentifier>, WebCore::FrameIdentifier, FrameInfoData&&, Ref<WebCore::SecurityOrigin>&&, Ref<WebCore::SecurityOrigin>&&, Vector<WebCore::CaptureDevice>&&, Vector<WebCore::CaptureDevice>&&, WebCore::MediaStreamRequest&&, CompletionHandler<void(bool)>&& = { });

    ~UserMediaPermissionRequestProxy();

    void allow(const String& audioDeviceUID, const String& videoDeviceUID);
    void allow();
    void promptForGetUserMedia();

    enum class UserMediaDisplayCapturePromptType { Window, Screen, UserChoose };
    virtual void promptForGetDisplayMedia(UserMediaDisplayCapturePromptType);
    virtual bool canRequestDisplayCapturePermission();

    void doDefaultAction();
    enum class UserMediaAccessDenialReason { NoConstraints, UserMediaDisabled, NoCaptureDevices, InvalidConstraint, HardwareError, PermissionDenied, OtherFailure };
    void deny(UserMediaAccessDenialReason = UserMediaAccessDenialReason::UserMediaDisabled);

    virtual void invalidate();
    bool isPending() const;

    bool requiresAudioCapture() const { return m_eligibleAudioDevices.size(); }
    bool requiresVideoCapture() const { return !requiresDisplayCapture() && m_eligibleVideoDevices.size(); }
    bool requiresDisplayCapture() const { return m_request.type == WebCore::MediaStreamRequest::Type::DisplayMedia || m_request.type == WebCore::MediaStreamRequest::Type::DisplayMediaWithAudio; }
    bool requiresDisplayCaptureWithAudio() const { return m_request.type == WebCore::MediaStreamRequest::Type::DisplayMediaWithAudio; }

    void setEligibleVideoDevices(Vector<WebCore::CaptureDevice>&& devices) { m_eligibleVideoDevices = WTFMove(devices); }
    void setEligibleAudioDevices(Vector<WebCore::CaptureDevice>&& devices) { m_eligibleAudioDevices = WTFMove(devices); }

    Vector<String> videoDeviceUIDs() const;
    Vector<String> audioDeviceUIDs() const;
    bool hasAudioDevice() const { return !m_eligibleAudioDevices.isEmpty(); }
    bool hasVideoDevice() const { return !m_eligibleVideoDevices.isEmpty(); }

    bool hasPersistentAccess() const { return m_hasPersistentAccess; }
    void setHasPersistentAccess() { m_hasPersistentAccess = true; }

    std::optional<WebCore::UserMediaRequestIdentifier> userMediaID() const { return m_userMediaID; }
    WebCore::FrameIdentifier mainFrameID() const { return m_mainFrameID; }
    const FrameInfoData& frameInfo() const { return m_frameInfo; }
    WebCore::FrameIdentifier frameID() const { return m_frameInfo.frameID; }

    WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin() { return m_topLevelDocumentSecurityOrigin.get(); }
    WebCore::SecurityOrigin& userMediaDocumentSecurityOrigin() { return m_userMediaDocumentSecurityOrigin.get(); }
    const WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin() const { return m_topLevelDocumentSecurityOrigin.get(); }
    const WebCore::SecurityOrigin& userMediaDocumentSecurityOrigin() const { return m_userMediaDocumentSecurityOrigin.get(); }

    const WebCore::MediaStreamRequest& userRequest() const { return m_request; }

    WebCore::MediaStreamRequest::Type requestType() const { return m_request.type; }

    void setDeviceIdentifierHashSalts(WebCore::MediaDeviceHashSalts&& salts) { m_deviceIdentifierHashSalts = WTFMove(salts); }
    const WebCore::MediaDeviceHashSalts& deviceIdentifierHashSalts() const { return m_deviceIdentifierHashSalts; }

    WebCore::CaptureDevice audioDevice() const { return m_eligibleAudioDevices.isEmpty() ? WebCore::CaptureDevice { } : m_eligibleAudioDevices[0]; }
    WebCore::CaptureDevice videoDevice() const { return m_eligibleVideoDevices.isEmpty() ? WebCore::CaptureDevice { } : m_eligibleVideoDevices[0]; }

#if ENABLE(MEDIA_STREAM)
    bool isUserGesturePriviledged() const { return m_request.isUserGesturePriviledged; }
#endif

    CompletionHandler<void(bool)> decisionCompletionHandler() { return std::exchange(m_decisionCompletionHandler, { }); }
    void setBeforeStartingCaptureCallback(Function<void()>&& callback) { m_beforeStartingCaptureCallback = WTFMove(callback); }
    Function<void()> beforeStartingCaptureCallback() { return std::exchange(m_beforeStartingCaptureCallback, { }); }

protected:
    UserMediaPermissionRequestProxy(UserMediaPermissionRequestManagerProxy&, std::optional<WebCore::UserMediaRequestIdentifier>, WebCore::FrameIdentifier mainFrameID, FrameInfoData&&, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, Vector<WebCore::CaptureDevice>&& audioDevices, Vector<WebCore::CaptureDevice>&& videoDevices, WebCore::MediaStreamRequest&&, CompletionHandler<void(bool)>&&);

    UserMediaPermissionRequestManagerProxy* manager() const;
    RefPtr<UserMediaPermissionRequestManagerProxy> protectedManager() const;

private:
    WeakPtr<UserMediaPermissionRequestManagerProxy> m_manager;
    Markable<WebCore::UserMediaRequestIdentifier> m_userMediaID;
    WebCore::FrameIdentifier m_mainFrameID;
    FrameInfoData m_frameInfo;
    const Ref<WebCore::SecurityOrigin> m_userMediaDocumentSecurityOrigin;
    const Ref<WebCore::SecurityOrigin> m_topLevelDocumentSecurityOrigin;
    Vector<WebCore::CaptureDevice> m_eligibleVideoDevices;
    Vector<WebCore::CaptureDevice> m_eligibleAudioDevices;
    WebCore::MediaStreamRequest m_request;
    bool m_hasPersistentAccess { false };
    WebCore::MediaDeviceHashSalts m_deviceIdentifierHashSalts;
    CompletionHandler<void(bool)> m_decisionCompletionHandler;
    Function<void()> m_beforeStartingCaptureCallback;
};

String convertEnumerationToString(UserMediaPermissionRequestProxy::UserMediaAccessDenialReason);

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::UserMediaPermissionRequestProxy)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::UserMediaPermissionRequest; }
SPECIALIZE_TYPE_TRAITS_END()

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebKit::UserMediaPermissionRequestProxy::UserMediaAccessDenialReason> {
    static String toString(const WebKit::UserMediaPermissionRequestProxy::UserMediaAccessDenialReason type)
    {
        return convertEnumerationToString(type);
    }
};

}; // namespace WTF

